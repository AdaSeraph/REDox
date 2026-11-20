// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#include "redox/messagepack.hpp"
#include "redox/container.hpp"
#include "redox/writer.hpp"

#include <bit>
#include <limits>
#include <stdexcept>

namespace redox {

namespace {

class MessagePackParser {
public:
    MessagePackParser(std::span<const uint8_t> data, Document& doc, const MessagePackDocumentOptions& opts)
        : data_(data), doc_(doc), opts_(opts) {}

    uint32_t parse() {
        if (data_.empty()) throw DocumentParseException("Empty MessagePack input");
        uint32_t root = parse_item(0);
        if (pos_ != data_.size()) {
            throw DocumentParseException("Trailing bytes after MessagePack root item", pos_);
        }
        return root;
    }

private:
    uint8_t read_u8() {
        if (pos_ >= data_.size()) throw DocumentParseException("Unexpected EOF in MessagePack", pos_);
        return data_[pos_++];
    }

    uint16_t read_u16_be() {
        if (pos_ + 2 > data_.size()) throw DocumentParseException("Unexpected EOF in MessagePack", pos_);
        uint16_t v = (static_cast<uint16_t>(data_[pos_]) << 8) | data_[pos_ + 1];
        pos_ += 2;
        return v;
    }

    uint32_t read_u32_be() {
        if (pos_ + 4 > data_.size()) throw DocumentParseException("Unexpected EOF in MessagePack", pos_);
        uint32_t v = (static_cast<uint32_t>(data_[pos_]) << 24) |
                     (static_cast<uint32_t>(data_[pos_ + 1]) << 16) |
                     (static_cast<uint32_t>(data_[pos_ + 2]) << 8) |
                     static_cast<uint32_t>(data_[pos_ + 3]);
        pos_ += 4;
        return v;
    }

    uint64_t read_u64_be() {
        if (pos_ + 8 > data_.size()) throw DocumentParseException("Unexpected EOF in MessagePack", pos_);
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v = (v << 8) | data_[pos_ + i];
        pos_ += 8;
        return v;
    }

    uint32_t read_str(size_t len) {
        if (pos_ + len > data_.size()) throw DocumentParseException("MessagePack string out of bounds", pos_);
        std::string s(reinterpret_cast<const char*>(data_.data() + pos_), len);
        pos_ += len;
        return doc_.create_string_token(std::move(s));
    }

    uint32_t read_bin(size_t len) {
        if (pos_ + len > data_.size()) throw DocumentParseException("MessagePack bin out of bounds", pos_);
        std::vector<uint8_t> b(data_.begin() + pos_, data_.begin() + pos_ + len);
        pos_ += len;
        return doc_.create_bytes_token(std::move(b));
    }

    uint32_t read_array(size_t count, int depth) {
        uint32_t arr_id = doc_.create_empty_array_token();
        std::vector<uint32_t> elems;
        elems.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            elems.push_back(parse_item(depth + 1));
        }
        auto& arr = doc_.materialize_array(arr_id);
        arr = DArray(&doc_, arr_id, std::move(elems));
        return arr_id;
    }

    uint32_t read_map(size_t count, int depth) {
        uint32_t map_id = doc_.create_empty_object_token();
        std::vector<DObject::Entry> entries;
        entries.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            uint32_t k = parse_item(depth + 1);
            uint32_t v = parse_item(depth + 1);
            entries.push_back({k, v});
        }
        auto& obj = doc_.materialize_object(map_id);
        obj = DObject(&doc_, map_id, std::move(entries));
        return map_id;
    }

    uint32_t read_ext(size_t len) {
        int8_t ext_type = static_cast<int8_t>(read_u8());
        if (pos_ + len > data_.size()) throw DocumentParseException("MessagePack ext out of bounds", pos_);
        if (ext_type == -1) { // Timestamp extension
            Timestamp ts;
            ts.has_offset = true;
            if (len == 4) {
                ts.unix_seconds = static_cast<int64_t>(read_u32_be());
                ts.nanoseconds = 0;
                return doc_.create_timestamp_token(ts);
            }
            if (len == 8) {
                uint64_t v = read_u64_be();
                ts.nanoseconds = static_cast<int32_t>(v >> 34);
                ts.unix_seconds = static_cast<int64_t>(v & 0x00000003ffffffffULL);
                return doc_.create_timestamp_token(ts);
            }
            if (len == 12) {
                ts.nanoseconds = static_cast<int32_t>(read_u32_be());
                ts.unix_seconds = static_cast<int64_t>(read_u64_be());
                return doc_.create_timestamp_token(ts);
            }
        }
        return read_bin(len);
    }

    uint32_t parse_item(int depth) {
        if (depth > opts_.max_depth) throw DocumentParseException("MessagePack max depth exceeded", pos_);
        uint8_t b = read_u8();

        if (b <= 0x7F) return doc_.create_int64_token(static_cast<int64_t>(b));
        if (b >= 0xE0) return doc_.create_int64_token(static_cast<int64_t>(static_cast<int8_t>(b)));
        if ((b & 0xF0) == 0x80) return read_map(b & 0x0F, depth);
        if ((b & 0xF0) == 0x90) return read_array(b & 0x0F, depth);
        if ((b & 0xE0) == 0xA0) return read_str(b & 0x1F);

        switch (b) {
            case 0xC0: return doc_.create_null_token();
            case 0xC2: return doc_.create_bool_token(false);
            case 0xC3: return doc_.create_bool_token(true);
            case 0xC4: return read_bin(read_u8());
            case 0xC5: return read_bin(read_u16_be());
            case 0xC6: return read_bin(read_u32_be());
            case 0xC7: return read_ext(read_u8());
            case 0xC8: return read_ext(read_u16_be());
            case 0xC9: return read_ext(read_u32_be());
            case 0xCA: return doc_.create_double_token(static_cast<double>(std::bit_cast<float>(read_u32_be())), FloatKind::Single);
            case 0xCB: return doc_.create_double_token(std::bit_cast<double>(read_u64_be()), FloatKind::Default);
            case 0xCC: return doc_.create_int64_token(static_cast<int64_t>(read_u8()));
            case 0xCD: return doc_.create_int64_token(static_cast<int64_t>(read_u16_be()));
            case 0xCE: return doc_.create_int64_token(static_cast<int64_t>(read_u32_be()));
            case 0xCF: {
                uint64_t v = read_u64_be();
                if (v <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
                    return doc_.create_int64_token(static_cast<int64_t>(v));
                }
                return doc_.create_uint64_token(v);
            }
            case 0xD0: return doc_.create_int64_token(static_cast<int64_t>(static_cast<int8_t>(read_u8())));
            case 0xD1: return doc_.create_int64_token(static_cast<int64_t>(static_cast<int16_t>(read_u16_be())));
            case 0xD2: return doc_.create_int64_token(static_cast<int64_t>(static_cast<int32_t>(read_u32_be())));
            case 0xD3: return doc_.create_int64_token(static_cast<int64_t>(read_u64_be()));
            case 0xD4: return read_ext(1);
            case 0xD5: return read_ext(2);
            case 0xD6: return read_ext(4);
            case 0xD7: return read_ext(8);
            case 0xD8: return read_ext(16);
            case 0xD9: return read_str(read_u8());
            case 0xDA: return read_str(read_u16_be());
            case 0xDB: return read_str(read_u32_be());
            case 0xDC: return read_array(read_u16_be(), depth);
            case 0xDD: return read_array(read_u32_be(), depth);
            case 0xDE: return read_map(read_u16_be(), depth);
            case 0xDF: return read_map(read_u32_be(), depth);
            default:
                throw DocumentParseException("Invalid MessagePack format byte", pos_);
        }
    }

    std::span<const uint8_t> data_;
    Document& doc_;
    const MessagePackDocumentOptions& opts_;
    size_t pos_ = 0;
};

void write_mp_uint(BinaryWriter& w, uint64_t v) {
    if (v <= 0x7F) {
        w.write_u8(static_cast<uint8_t>(v));
    } else if (v <= 0xFF) {
        w.write_u8(0xCC);
        w.write_u8(static_cast<uint8_t>(v));
    } else if (v <= 0xFFFF) {
        w.write_u8(0xCD);
        w.write_u16_be(static_cast<uint16_t>(v));
    } else if (v <= 0xFFFFFFFFULL) {
        w.write_u8(0xCE);
        w.write_u32_be(static_cast<uint32_t>(v));
    } else {
        w.write_u8(0xCF);
        w.write_u64_be(v);
    }
}

void write_mp_int(BinaryWriter& w, int64_t v) {
    if (v >= 0) {
        write_mp_uint(w, static_cast<uint64_t>(v));
    } else if (v >= -32) {
        w.write_u8(static_cast<uint8_t>(v));
    } else if (v >= std::numeric_limits<int8_t>::min()) {
        w.write_u8(0xD0);
        w.write_u8(static_cast<uint8_t>(static_cast<int8_t>(v)));
    } else if (v >= std::numeric_limits<int16_t>::min()) {
        w.write_u8(0xD1);
        w.write_u16_be(static_cast<uint16_t>(static_cast<int16_t>(v)));
    } else if (v >= std::numeric_limits<int32_t>::min()) {
        w.write_u8(0xD2);
        w.write_u32_be(static_cast<uint32_t>(static_cast<int32_t>(v)));
    } else {
        w.write_u8(0xD3);
        w.write_u64_be(static_cast<uint64_t>(v));
    }
}

void write_mp_element(BinaryWriter& w, DElement elem) {
    if (!elem.is_valid() || elem.is_null()) {
        w.write_u8(0xC0);
        return;
    }
    DToken tok = elem.token();
    if (tok.type() == DTokenType::Array) {
        auto items = elem.array_elements();
        size_t n = items.size();
        if (n <= 15) {
            w.write_u8(static_cast<uint8_t>(0x90 | n));
        } else if (n <= 0xFFFF) {
            w.write_u8(0xDC);
            w.write_u16_be(static_cast<uint16_t>(n));
        } else {
            w.write_u8(0xDD);
            w.write_u32_be(static_cast<uint32_t>(n));
        }
        for (const auto& item : items) write_mp_element(w, item);
        return;
    }
    if (tok.type() == DTokenType::Map) {
        auto entries = elem.map_entries();
        size_t n = entries.size();
        if (n <= 15) {
            w.write_u8(static_cast<uint8_t>(0x80 | n));
        } else if (n <= 0xFFFF) {
            w.write_u8(0xDE);
            w.write_u16_be(static_cast<uint16_t>(n));
        } else {
            w.write_u8(0xDF);
            w.write_u32_be(static_cast<uint32_t>(n));
        }
        for (const auto& [k, v] : entries) {
            write_mp_element(w, k);
            write_mp_element(w, v);
        }
        return;
    }
    switch (tok.kind()) {
        case DTokenKind::Boolean:
            w.write_u8(elem.get_bool() ? 0xC3 : 0xC2);
            break;
        case DTokenKind::Integer:
            if (tok.integer_kind() == IntegerKind::Unsigned) {
                write_mp_uint(w, elem.get_uint64());
            } else {
                write_mp_int(w, elem.get_int64());
            }
            break;
        case DTokenKind::Float:
        case DTokenKind::InlineFloat:
            w.write_u8(0xCB);
            w.write_f64_be(elem.get_double());
            break;
        case DTokenKind::String:
        case DTokenKind::Symbol:
        case DTokenKind::BigNumber: {
            std::string s = elem.get_string();
            size_t n = s.size();
            if (n <= 31) {
                w.write_u8(static_cast<uint8_t>(0xA0 | n));
            } else if (n <= 0xFF) {
                w.write_u8(0xD9);
                w.write_u8(static_cast<uint8_t>(n));
            } else if (n <= 0xFFFF) {
                w.write_u8(0xDA);
                w.write_u16_be(static_cast<uint16_t>(n));
            } else {
                w.write_u8(0xDB);
                w.write_u32_be(static_cast<uint32_t>(n));
            }
            w.write_string(s);
            break;
        }
        case DTokenKind::ByteString: {
            auto bytes = elem.get_bytes();
            size_t n = bytes.size();
            if (n <= 0xFF) {
                w.write_u8(0xC4);
                w.write_u8(static_cast<uint8_t>(n));
            } else if (n <= 0xFFFF) {
                w.write_u8(0xC5);
                w.write_u16_be(static_cast<uint16_t>(n));
            } else {
                w.write_u8(0xC6);
                w.write_u32_be(static_cast<uint32_t>(n));
            }
            w.write_bytes(bytes);
            break;
        }
        case DTokenKind::Timestamp: {
            Timestamp ts = elem.get_timestamp();
            if (ts.unix_seconds >= 0 && ts.unix_seconds <= 0xFFFFFFFFLL && ts.nanoseconds == 0) {
                w.write_u8(0xD6); // fixext 4
                w.write_u8(0xFF); // -1
                w.write_u32_be(static_cast<uint32_t>(ts.unix_seconds));
            } else if (ts.unix_seconds >= 0 && ts.unix_seconds <= 0x3FFFFFFFFLL) {
                w.write_u8(0xD7); // fixext 8
                w.write_u8(0xFF);
                uint64_t v = (static_cast<uint64_t>(ts.nanoseconds) << 34) | static_cast<uint64_t>(ts.unix_seconds);
                w.write_u64_be(v);
            } else {
                w.write_u8(0xC7); // ext 8
                w.write_u8(12);
                w.write_u8(0xFF);
                w.write_u32_be(static_cast<uint32_t>(ts.nanoseconds));
                w.write_u64_be(static_cast<uint64_t>(ts.unix_seconds));
            }
            break;
        }
        default:
            w.write_u8(0xC0);
            break;
    }
}

} // namespace

std::unique_ptr<MessagePackDocument> MessagePackDocument::parse(std::span<const uint8_t> data, const MessagePackDocumentOptions& options) {
    auto doc = std::unique_ptr<MessagePackDocument>(new MessagePackDocument());
    doc->parse_impl(data, options);
    return doc;
}

bool MessagePackDocument::try_parse(std::span<const uint8_t> data, std::unique_ptr<MessagePackDocument>& out, const MessagePackDocumentOptions& options) noexcept {
    try {
        out = parse(data, options);
        return true;
    } catch (...) {
        out.reset();
        return false;
    }
}

void MessagePackDocument::parse_impl(std::span<const uint8_t> data, const MessagePackDocumentOptions& options) {
    reset();
    MessagePackParser parser(data, *this, options);
    set_root_id(parser.parse());
}

std::vector<uint8_t> MessagePackDocument::encode(DElement element, const MessagePackWriteOptions&) {
    BinaryWriter w(256);
    write_mp_element(w, element);
    return w.take_bytes();
}

} // namespace redox

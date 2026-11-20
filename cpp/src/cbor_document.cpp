// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#include "redox/cbor.hpp"
#include "redox/container.hpp"
#include "redox/writer.hpp"

#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace redox {

namespace {

double half_to_double(uint16_t h) noexcept {
    int exp = (h >> 10) & 0x1f;
    int mant = h & 0x3ff;
    double val = 0.0;
    if (exp == 0) {
        val = std::ldexp(static_cast<double>(mant), -24);
    } else if (exp != 31) {
        val = std::ldexp(static_cast<double>(mant + 1024), exp - 25);
    } else {
        val = (mant == 0) ? std::numeric_limits<double>::infinity() : std::numeric_limits<double>::quiet_NaN();
    }
    return (h & 0x8000) ? -val : val;
}

class CborParser {
public:
    CborParser(std::span<const uint8_t> data, Document& doc, const CborDocumentOptions& opts)
        : data_(data), doc_(doc), opts_(opts) {}

    uint32_t parse() {
        if (data_.empty()) throw DocumentParseException("Empty CBOR input");
        uint32_t root = parse_item(0);
        if (pos_ != data_.size()) {
            throw DocumentParseException("Trailing bytes after CBOR root item", pos_);
        }
        return root;
    }

private:
    uint8_t read_u8() {
        if (pos_ >= data_.size()) throw DocumentParseException("Unexpected EOF in CBOR", pos_);
        return data_[pos_++];
    }

    uint16_t read_u16_be() {
        if (pos_ + 2 > data_.size()) throw DocumentParseException("Unexpected EOF in CBOR", pos_);
        uint16_t v = (static_cast<uint16_t>(data_[pos_]) << 8) | data_[pos_ + 1];
        pos_ += 2;
        return v;
    }

    uint32_t read_u32_be() {
        if (pos_ + 4 > data_.size()) throw DocumentParseException("Unexpected EOF in CBOR", pos_);
        uint32_t v = (static_cast<uint32_t>(data_[pos_]) << 24) |
                     (static_cast<uint32_t>(data_[pos_ + 1]) << 16) |
                     (static_cast<uint32_t>(data_[pos_ + 2]) << 8) |
                     static_cast<uint32_t>(data_[pos_ + 3]);
        pos_ += 4;
        return v;
    }

    uint64_t read_u64_be() {
        if (pos_ + 8 > data_.size()) throw DocumentParseException("Unexpected EOF in CBOR", pos_);
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
            v = (v << 8) | data_[pos_ + i];
        }
        pos_ += 8;
        return v;
    }

    uint64_t read_arg(uint8_t ai) {
        if (ai < 24) return ai;
        if (ai == 24) return read_u8();
        if (ai == 25) return read_u16_be();
        if (ai == 26) return read_u32_be();
        if (ai == 27) return read_u64_be();
        throw DocumentParseException("Invalid CBOR additional info", pos_);
    }

    uint32_t parse_item(int depth) {
        if (depth > opts_.max_depth) throw DocumentParseException("CBOR max depth exceeded", pos_);
        uint8_t initial = read_u8();
        uint8_t major = initial >> 5;
        uint8_t ai = initial & 0x1F;

        switch (major) {
            case 0: { // Unsigned integer
                uint64_t v = read_arg(ai);
                if (v <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
                    return doc_.create_int64_token(static_cast<int64_t>(v));
                }
                return doc_.create_uint64_token(v);
            }
            case 1: { // Negative integer (-1 - n)
                uint64_t n = read_arg(ai);
                if (n > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
                    throw DocumentParseException("CBOR negative integer underflow", pos_);
                }
                return doc_.create_int64_token(-1LL - static_cast<int64_t>(n));
            }
            case 2: { // Byte string
                std::vector<uint8_t> bytes;
                if (ai == 31) {
                    while (pos_ < data_.size() && data_[pos_] != 0xFF) {
                        uint8_t ci = read_u8();
                        if ((ci >> 5) != 2 || (ci & 0x1F) == 31) throw DocumentParseException("Invalid indefinite byte string chunk", pos_);
                        uint64_t len = read_arg(ci & 0x1F);
                        if (pos_ + len > data_.size()) throw DocumentParseException("CBOR byte string out of bounds", pos_);
                        bytes.insert(bytes.end(), data_.begin() + pos_, data_.begin() + pos_ + len);
                        pos_ += static_cast<size_t>(len);
                    }
                    if (read_u8() != 0xFF) throw DocumentParseException("Unterminated indefinite byte string", pos_);
                } else {
                    uint64_t len = read_arg(ai);
                    if (pos_ + len > data_.size()) throw DocumentParseException("CBOR byte string out of bounds", pos_);
                    bytes.assign(data_.begin() + pos_, data_.begin() + pos_ + len);
                    pos_ += static_cast<size_t>(len);
                }
                return doc_.create_bytes_token(std::move(bytes));
            }
            case 3: { // Text string
                std::string text;
                if (ai == 31) {
                    while (pos_ < data_.size() && data_[pos_] != 0xFF) {
                        uint8_t ci = read_u8();
                        if ((ci >> 5) != 3 || (ci & 0x1F) == 31) throw DocumentParseException("Invalid indefinite text string chunk", pos_);
                        uint64_t len = read_arg(ci & 0x1F);
                        if (pos_ + len > data_.size()) throw DocumentParseException("CBOR text string out of bounds", pos_);
                        text.append(reinterpret_cast<const char*>(data_.data() + pos_), static_cast<size_t>(len));
                        pos_ += static_cast<size_t>(len);
                    }
                    if (read_u8() != 0xFF) throw DocumentParseException("Unterminated indefinite text string", pos_);
                } else {
                    uint64_t len = read_arg(ai);
                    if (pos_ + len > data_.size()) throw DocumentParseException("CBOR text string out of bounds", pos_);
                    text.assign(reinterpret_cast<const char*>(data_.data() + pos_), static_cast<size_t>(len));
                    pos_ += static_cast<size_t>(len);
                }
                return doc_.create_string_token(std::move(text));
            }
            case 4: { // Array
                uint32_t arr_id = doc_.create_empty_array_token();
                std::vector<uint32_t> elems;
                if (ai == 31) {
                    while (pos_ < data_.size() && data_[pos_] != 0xFF) {
                        elems.push_back(parse_item(depth + 1));
                    }
                    if (read_u8() != 0xFF) throw DocumentParseException("Unterminated indefinite CBOR array", pos_);
                } else {
                    uint64_t cnt = read_arg(ai);
                    elems.reserve(static_cast<size_t>(cnt));
                    for (uint64_t i = 0; i < cnt; ++i) {
                        elems.push_back(parse_item(depth + 1));
                    }
                }
                auto& arr = doc_.materialize_array(arr_id);
                arr = DArray(&doc_, arr_id, std::move(elems));
                return arr_id;
            }
            case 5: { // Map
                uint32_t map_id = doc_.create_empty_object_token();
                std::vector<DObject::Entry> entries;
                if (ai == 31) {
                    while (pos_ < data_.size() && data_[pos_] != 0xFF) {
                        uint32_t k = parse_item(depth + 1);
                        uint32_t v = parse_item(depth + 1);
                        entries.push_back({k, v});
                    }
                    if (read_u8() != 0xFF) throw DocumentParseException("Unterminated indefinite CBOR map", pos_);
                } else {
                    uint64_t cnt = read_arg(ai);
                    entries.reserve(static_cast<size_t>(cnt));
                    for (uint64_t i = 0; i < cnt; ++i) {
                        uint32_t k = parse_item(depth + 1);
                        uint32_t v = parse_item(depth + 1);
                        entries.push_back({k, v});
                    }
                }
                auto& obj = doc_.materialize_object(map_id);
                obj = DObject(&doc_, map_id, std::move(entries));
                return map_id;
            }
            case 6: { // Tag
                uint64_t tag = read_arg(ai);
                uint32_t inner = parse_item(depth + 1);
                if (tag == 0) { // Standard date/time string
                    Timestamp ts;
                    if (doc_.token_at(inner).kind() == DTokenKind::String &&
                        Timestamp::try_parse_iso8601(doc_.resolve_string(inner), ts)) {
                        doc_.free_token(inner);
                        return doc_.create_timestamp_token(ts);
                    }
                } else if (tag == 1) { // Epoch-based date/time
                    Timestamp ts;
                    if (doc_.token_at(inner).is_numeric()) {
                        double sec = doc_.resolve_double(inner);
                        ts.unix_seconds = static_cast<int64_t>(std::floor(sec));
                        ts.nanoseconds = static_cast<int32_t>((sec - static_cast<double>(ts.unix_seconds)) * 1e9);
                        ts.has_offset = true;
                        doc_.free_token(inner);
                        return doc_.create_timestamp_token(ts);
                    }
                }
                return inner;
            }
            case 7: { // Simple values & floats
                if (ai == 20) return doc_.create_bool_token(false);
                if (ai == 21) return doc_.create_bool_token(true);
                if (ai == 22) return doc_.create_null_token(NullKind::Default);
                if (ai == 23) return doc_.create_null_token(NullKind::Undefined);
                if (ai == 25) return doc_.create_double_token(half_to_double(read_u16_be()), FloatKind::Half);
                if (ai == 26) return doc_.create_double_token(static_cast<double>(std::bit_cast<float>(read_u32_be())), FloatKind::Single);
                if (ai == 27) return doc_.create_double_token(std::bit_cast<double>(read_u64_be()), FloatKind::Default);
                throw DocumentParseException("Unsupported CBOR simple value", pos_);
            }
            default:
                throw DocumentParseException("Invalid CBOR major type", pos_);
        }
    }

    std::span<const uint8_t> data_;
    Document& doc_;
    const CborDocumentOptions& opts_;
    size_t pos_ = 0;
};

void write_cbor_header(BinaryWriter& w, uint8_t major, uint64_t arg) {
    uint8_t prefix = static_cast<uint8_t>(major << 5);
    if (arg < 24) {
        w.write_u8(prefix | static_cast<uint8_t>(arg));
    } else if (arg <= 0xFF) {
        w.write_u8(prefix | 24);
        w.write_u8(static_cast<uint8_t>(arg));
    } else if (arg <= 0xFFFF) {
        w.write_u8(prefix | 25);
        w.write_u16_be(static_cast<uint16_t>(arg));
    } else if (arg <= 0xFFFFFFFFULL) {
        w.write_u8(prefix | 26);
        w.write_u32_be(static_cast<uint32_t>(arg));
    } else {
        w.write_u8(prefix | 27);
        w.write_u64_be(arg);
    }
}

void write_cbor_element(BinaryWriter& w, DElement elem) {
    if (!elem.is_valid() || elem.is_null()) {
        w.write_u8(0xF6); // null
        return;
    }
    DToken tok = elem.token();
    if (tok.type() == DTokenType::Array) {
        auto items = elem.array_elements();
        write_cbor_header(w, 4, items.size());
        for (const auto& item : items) write_cbor_element(w, item);
        return;
    }
    if (tok.type() == DTokenType::Map) {
        auto entries = elem.map_entries();
        write_cbor_header(w, 5, entries.size());
        for (const auto& [k, v] : entries) {
            write_cbor_element(w, k);
            write_cbor_element(w, v);
        }
        return;
    }
    switch (tok.kind()) {
        case DTokenKind::Boolean:
            w.write_u8(elem.get_bool() ? 0xF5 : 0xF4);
            break;
        case DTokenKind::Integer:
            if (tok.integer_kind() == IntegerKind::Unsigned) {
                write_cbor_header(w, 0, elem.get_uint64());
            } else {
                int64_t v = elem.get_int64();
                if (v >= 0) {
                    write_cbor_header(w, 0, static_cast<uint64_t>(v));
                } else {
                    write_cbor_header(w, 1, static_cast<uint64_t>(-1LL - v));
                }
            }
            break;
        case DTokenKind::Float:
        case DTokenKind::InlineFloat:
            w.write_u8(0xFB);
            w.write_f64_be(elem.get_double());
            break;
        case DTokenKind::String:
        case DTokenKind::Symbol:
        case DTokenKind::BigNumber: {
            std::string s = elem.get_string();
            write_cbor_header(w, 3, s.size());
            w.write_string(s);
            break;
        }
        case DTokenKind::ByteString: {
            auto bytes = elem.get_bytes();
            write_cbor_header(w, 2, bytes.size());
            w.write_bytes(bytes);
            break;
        }
        case DTokenKind::Timestamp: {
            write_cbor_header(w, 6, 0); // Tag 0: ISO-8601 date/time string
            std::string iso = elem.get_timestamp().to_iso8601();
            write_cbor_header(w, 3, iso.size());
            w.write_string(iso);
            break;
        }
        default:
            w.write_u8(0xF6);
            break;
    }
}

} // namespace

std::unique_ptr<CborDocument> CborDocument::parse(std::span<const uint8_t> data, const CborDocumentOptions& options) {
    auto doc = std::unique_ptr<CborDocument>(new CborDocument());
    doc->parse_impl(data, options);
    return doc;
}

bool CborDocument::try_parse(std::span<const uint8_t> data, std::unique_ptr<CborDocument>& out, const CborDocumentOptions& options) noexcept {
    try {
        out = parse(data, options);
        return true;
    } catch (...) {
        out.reset();
        return false;
    }
}

void CborDocument::parse_impl(std::span<const uint8_t> data, const CborDocumentOptions& options) {
    reset();
    CborParser parser(data, *this, options);
    set_root_id(parser.parse());
}

std::vector<uint8_t> CborDocument::encode(DElement element, const CborWriteOptions&) {
    BinaryWriter w(256);
    write_cbor_element(w, element);
    return w.take_bytes();
}

} // namespace redox

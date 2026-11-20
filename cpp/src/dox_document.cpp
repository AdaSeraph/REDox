// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#include "redox/dox.hpp"
#include "redox/writer.hpp"
#include "utf8_helper.hpp"

#include <bit>
#include <cstring>
#include <stdexcept>

namespace redox {

namespace {

constexpr int64_t kInlineValueMask          = 0x0080000000000000LL;
constexpr int64_t kInlineIntegerPayloadMask = 0x007fffffffffffffLL;
constexpr uint32_t kExtendedLengthMarker    = 0x1ffffffu;
constexpr int64_t kDoxMinInlineInteger      = -(1LL << 54);
constexpr int64_t kDoxMaxInlineInteger      = (1LL << 54) - 1;
constexpr uint64_t kDoxMaxInlineUnsigned    = (1ULL << 55) - 1;

// .NET ticks between 0001-01-01T00:00:00Z and 1970-01-01T00:00:00Z
constexpr int64_t kUnixEpochDotNetTicks = 621355968000000000LL;
constexpr int64_t kTicksPerSecond       = 10000000LL;

uint32_t read_u32_le(std::span<const uint8_t> data, size_t off) {
    if (off + 4 > data.size()) throw DocumentParseException("Unexpected EOF in DOX stream", off);
    return static_cast<uint32_t>(data[off]) |
           (static_cast<uint32_t>(data[off + 1]) << 8) |
           (static_cast<uint32_t>(data[off + 2]) << 16) |
           (static_cast<uint32_t>(data[off + 3]) << 24);
}

uint64_t read_u64_le(std::span<const uint8_t> data, size_t off) {
    if (off + 8 > data.size()) throw DocumentParseException("Unexpected EOF in DOX stream", off);
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= static_cast<uint64_t>(data[off + i]) << (i * 8);
    }
    return v;
}

DToken write_dox_string(BinaryWriter& w, std::string_view utf8, DTokenVariant variant) {
    if (utf8.empty()) {
        return DToken::make(variant, kInlineValueMask);
    }
    uint32_t offset = static_cast<uint32_t>(w.size());
    size_t utf16_len = utf8::count_utf16_code_units(utf8);
    if (utf8.size() > 0xfff || utf16_len > 0xfff) {
        w.write_u32_le(static_cast<uint32_t>(utf8.size()));
        w.write_string(utf8);
        return DToken::make(variant, DToken::encode_length_offset(0, offset));
    }
    w.write_string(utf8);
    uint32_t param = static_cast<uint32_t>(utf8.size()) | (static_cast<uint32_t>(utf16_len) << 12);
    return DToken::make(variant, DToken::encode_length_offset(param, offset));
}

DToken write_dox_bytes(BinaryWriter& w, std::span<const uint8_t> data, DTokenVariant variant) {
    uint32_t offset = static_cast<uint32_t>(w.size());
    if (data.size() >= kExtendedLengthMarker) {
        w.write_u32_le(static_cast<uint32_t>(data.size()));
        w.write_bytes(data);
        return DToken::make(variant, DToken::encode_length_offset(kExtendedLengthMarker, offset));
    }
    w.write_bytes(data);
    return DToken::make(variant, DToken::encode_length_offset(static_cast<uint32_t>(data.size()), offset));
}

DToken write_dox_int64(BinaryWriter& w, int64_t value) {
    if (value >= kDoxMinInlineInteger && value <= kDoxMaxInlineInteger) {
        return DToken::make(DTokenVariant::Integer, kInlineValueMask | (value & kInlineIntegerPayloadMask));
    }
    uint32_t offset = static_cast<uint32_t>(w.size());
    w.write_i64_le(value);
    return DToken::make(DTokenVariant::Integer, DToken::encode_length_offset(8, offset));
}

DToken write_dox_uint64(BinaryWriter& w, uint64_t value) {
    if (value <= kDoxMaxInlineUnsigned) {
        return DToken::make(DTokenVariant::IntegerUnsigned, kInlineValueMask | static_cast<int64_t>(value));
    }
    uint32_t offset = static_cast<uint32_t>(w.size());
    w.write_u64_le(value);
    return DToken::make(DTokenVariant::IntegerUnsigned, DToken::encode_length_offset(8, offset));
}

DToken write_dox_double(BinaryWriter& w, double value) {
    int64_t inline_payload = DToken::encode_inline_float(value);
    if (inline_payload >= 0) {
        return DToken::make_inline_float(inline_payload);
    }
    uint32_t offset = static_cast<uint32_t>(w.size());
    w.write_f64_le(value);
    return DToken::make(DTokenVariant::Float, static_cast<int64_t>(offset));
}

DToken write_dox_timestamp(BinaryWriter& w, const Timestamp& ts) {
    uint32_t offset = static_cast<uint32_t>(w.size());
    int64_t ticks = kUnixEpochDotNetTicks +
                    (ts.unix_seconds + static_cast<int64_t>(ts.offset_minutes) * 60) * kTicksPerSecond +
                    (ts.nanoseconds / 100);
    if (ts.has_offset) {
        w.write_i64_le(ticks);
        w.write_i64_le(static_cast<int64_t>(ts.offset_minutes) * 60 * kTicksPerSecond);
        return DToken::make(DTokenVariant::TimestampOffsetDateTime, DToken::encode_length_offset(0, offset));
    }
    // UTC DateTime.ToBinary sets kind bits (01 << 62)
    uint64_t bin = (1ULL << 62) | (static_cast<uint64_t>(ticks) & 0x3fffffffffffffffULL);
    w.write_u64_le(bin);
    return DToken::make(DTokenVariant::Timestamp, DToken::encode_length_offset(0, offset));
}

uint32_t encode_element_to_dox(BinaryWriter& data_writer, std::vector<DToken>& tokens, DElement elem) {
    if (!elem.is_valid() || elem.is_null()) {
        uint32_t id = static_cast<uint32_t>(tokens.size());
        tokens.push_back(DToken::make(DTokenVariant::Null, 0));
        return id;
    }

    DToken src_tok = elem.token();
    if (src_tok.type() == DTokenType::Array) {
        auto items = elem.array_elements();
        uint32_t id = static_cast<uint32_t>(tokens.size());
        tokens.push_back(DToken::make_array(static_cast<uint32_t>(items.size())));
        for (const auto& item : items) {
            encode_element_to_dox(data_writer, tokens, item);
        }
        tokens[id].set_link_id(static_cast<uint32_t>(tokens.size()));
        return id;
    }

    if (src_tok.type() == DTokenType::Map) {
        auto entries = elem.map_entries();
        uint32_t id = static_cast<uint32_t>(tokens.size());
        tokens.push_back(DToken::make_map(static_cast<uint32_t>(entries.size())));
        for (const auto& [k, v] : entries) {
            encode_element_to_dox(data_writer, tokens, k);
            encode_element_to_dox(data_writer, tokens, v);
        }
        tokens[id].set_link_id(static_cast<uint32_t>(tokens.size()));
        return id;
    }

    uint32_t id = static_cast<uint32_t>(tokens.size());
    switch (src_tok.kind()) {
        case DTokenKind::Boolean:
            tokens.push_back(DToken::make(elem.get_bool() ? DTokenVariant::BooleanTrue : DTokenVariant::BooleanFalse, 0));
            break;
        case DTokenKind::Integer:
            if (src_tok.integer_kind() == IntegerKind::Unsigned) {
                tokens.push_back(write_dox_uint64(data_writer, elem.get_uint64()));
            } else {
                tokens.push_back(write_dox_int64(data_writer, elem.get_int64()));
            }
            break;
        case DTokenKind::Float:
        case DTokenKind::InlineFloat:
            tokens.push_back(write_dox_double(data_writer, elem.get_double()));
            break;
        case DTokenKind::String:
        case DTokenKind::Symbol:
        case DTokenKind::BigNumber: {
            DTokenVariant v = (src_tok.kind() == DTokenKind::Symbol) ? DTokenVariant::Symbol : DTokenVariant::String;
            tokens.push_back(write_dox_string(data_writer, elem.get_string(), v));
            break;
        }
        case DTokenKind::ByteString:
            tokens.push_back(write_dox_bytes(data_writer, elem.get_bytes(), DTokenVariant::ByteString));
            break;
        case DTokenKind::Timestamp:
            tokens.push_back(write_dox_timestamp(data_writer, elem.get_timestamp()));
            break;
        default:
            tokens.push_back(DToken::make(DTokenVariant::Null, 0));
            break;
    }
    return id;
}

} // namespace

std::unique_ptr<DoxDocument> DoxDocument::parse(std::span<const uint8_t> data, const DoxDocumentOptions&) {
    auto doc = std::unique_ptr<DoxDocument>(new DoxDocument());
    doc->read_impl(data);
    return doc;
}

bool DoxDocument::try_parse(std::span<const uint8_t> data, std::unique_ptr<DoxDocument>& out, const DoxDocumentOptions& options) noexcept {
    try {
        out = parse(data, options);
        return true;
    } catch (...) {
        out.reset();
        return false;
    }
}

void DoxDocument::read_impl(std::span<const uint8_t> data) {
    reset();
    if (data.size() < 16) {
        throw DocumentParseException("DOX buffer too small (< 16 bytes)");
    }

    uint32_t magic = read_u32_le(data, 0);
    if (magic != kMagic) {
        throw DocumentParseException("Invalid DOX magic header");
    }

    uint32_t version = read_u32_le(data, 4);
    if ((version >> 16) != static_cast<uint32_t>(kMajorVersion)) {
        throw DocumentParseException("Unsupported DOX major version", 4);
    }

    size_t footer_offset = data.size() - 8;
    uint32_t token_length = read_u32_le(data, footer_offset);
    uint32_t token_offset = read_u32_le(data, footer_offset + 4);

    if (token_length < 2 || token_offset < 8 || token_offset > footer_offset) {
        throw DocumentParseException("Invalid DOX footer metadata", footer_offset);
    }

    size_t token_byte_length = static_cast<size_t>(token_length) * sizeof(DToken);
    if (token_byte_length > footer_offset - token_offset) {
        throw DocumentParseException("DOX token table exceeds buffer bounds", token_offset);
    }

    source_.assign(data.begin(), data.end());
    tokens_.resize(token_length);
    for (uint32_t i = 0; i < token_length; ++i) {
        tokens_[i] = DToken(static_cast<int64_t>(read_u64_le(data, token_offset + i * 8)));
    }

    // Reconstruct container link_ids for any C#-written containers that only linked siblings
    // (In our encoder link_id is always end_id; for C# compatibility, ensure root_id skips ignore tokens)
    uint32_t rid = 1;
    while (rid < token_length && tokens_[rid].is_ignore()) {
        ++rid;
    }
    if (rid >= token_length) {
        throw DocumentParseException("DOX document missing root value");
    }
    set_root_id(rid);
}

std::vector<uint8_t> DoxDocument::encode(DElement element, const DoxWriteOptions&) {
    BinaryWriter w(256);
    // 8-byte header
    w.write_u32_le(kMagic);
    w.write_u32_le((static_cast<uint32_t>(kMajorVersion) << 16) | static_cast<uint32_t>(kMinorVersion));

    std::vector<DToken> tokens;
    tokens.push_back(DToken::make_nop());
    encode_element_to_dox(w, tokens, element);

    uint32_t token_offset = static_cast<uint32_t>(w.size());
    for (const auto& tok : tokens) {
        w.write_i64_le(tok.raw);
    }

    // 8-byte footer: token_length, token_offset
    w.write_u32_le(static_cast<uint32_t>(tokens.size()));
    w.write_u32_le(token_offset);
    return w.take_bytes();
}

std::string DoxDocument::decode_string(DToken token) const {
    if ((token.raw & kInlineValueMask) != 0) {
        return {};
    }
    auto [param, offset] = DToken::decode_length_offset(token);
    if (param == 0) {
        uint32_t len = read_u32_le(source_, offset);
        return std::string(reinterpret_cast<const char*>(source_.data() + offset + 4), len);
    }
    uint32_t len = param & 0xfffu;
    return std::string(reinterpret_cast<const char*>(source_.data() + offset), len);
}

std::string_view DoxDocument::decode_raw_utf8(DToken token, bool& ok) const {
    if (token.kind() != DTokenKind::String && token.kind() != DTokenKind::Symbol) {
        ok = false;
        return {};
    }
    if ((token.raw & kInlineValueMask) != 0) {
        ok = true;
        return {};
    }
    auto [param, offset] = DToken::decode_length_offset(token);
    if (param == 0) {
        uint32_t len = read_u32_le(source_, offset);
        ok = true;
        return std::string_view(reinterpret_cast<const char*>(source_.data() + offset + 4), len);
    }
    uint32_t len = param & 0xfffu;
    ok = true;
    return std::string_view(reinterpret_cast<const char*>(source_.data() + offset), len);
}

int64_t DoxDocument::decode_integer(DToken token) const {
    if ((token.raw & kInlineValueMask) != 0) {
        if (token.variant() == DTokenVariant::IntegerUnsigned) {
            return token.raw & kInlineIntegerPayloadMask;
        }
        // Sign-extend 55-bit integer
        uint64_t u = static_cast<uint64_t>(token.raw) & static_cast<uint64_t>(kInlineIntegerPayloadMask);
        if (u & (1ULL << 54)) {
            u |= ~static_cast<uint64_t>(kInlineIntegerPayloadMask);
        }
        return static_cast<int64_t>(u);
    }
    auto [len, offset] = DToken::decode_length_offset(token);
    (void)len;
    return static_cast<int64_t>(read_u64_le(source_, offset));
}

uint64_t DoxDocument::decode_uinteger(DToken token) const {
    if ((token.raw & kInlineValueMask) != 0) {
        return static_cast<uint64_t>(token.raw & kInlineIntegerPayloadMask);
    }
    auto [len, offset] = DToken::decode_length_offset(token);
    (void)len;
    return read_u64_le(source_, offset);
}

double DoxDocument::decode_float(DToken token) const {
    uint32_t param = static_cast<uint32_t>(token.raw);
    if ((token.raw & kInlineValueMask) != 0) {
        return static_cast<double>(std::bit_cast<float>(param));
    }
    return std::bit_cast<double>(read_u64_le(source_, param));
}

std::vector<uint8_t> DoxDocument::decode_bytes(DToken token) const {
    auto [len, offset] = DToken::decode_length_offset(token);
    if (len == kExtendedLengthMarker) {
        len = read_u32_le(source_, offset);
        offset += 4;
    }
    if (offset + len > source_.size()) {
        throw DocumentParseException("DOX ByteString out of bounds", offset);
    }
    return std::vector<uint8_t>(source_.begin() + offset, source_.begin() + offset + len);
}

Timestamp DoxDocument::decode_timestamp(DToken token) const {
    auto [len, offset] = DToken::decode_length_offset(token);
    (void)len;
    Timestamp ts;
    if (token.variant() == DTokenVariant::TimestampOffsetDateTime) {
        int64_t dt_bin = static_cast<int64_t>(read_u64_le(source_, offset));
        int64_t off_ticks = static_cast<int64_t>(read_u64_le(source_, offset + 8));
        int64_t ticks = dt_bin & 0x3fffffffffffffffLL;
        int32_t off_min = static_cast<int32_t>(off_ticks / (60 * kTicksPerSecond));
        int64_t local_rel = ticks - kUnixEpochDotNetTicks;
        int64_t local_sec = local_rel >= 0 ? local_rel / kTicksPerSecond : (local_rel - kTicksPerSecond + 1) / kTicksPerSecond;
        int64_t rem_ticks = local_rel - local_sec * kTicksPerSecond;
        ts.unix_seconds = local_sec - static_cast<int64_t>(off_min) * 60;
        ts.nanoseconds = static_cast<int32_t>(rem_ticks * 100);
        ts.offset_minutes = off_min;
        ts.has_offset = true;
        ts.kind = TimestampKind::OffsetDateTime;
    } else {
        int64_t dt_bin = static_cast<int64_t>(read_u64_le(source_, offset));
        int64_t ticks = dt_bin & 0x3fffffffffffffffLL;
        int64_t rel = ticks - kUnixEpochDotNetTicks;
        int64_t sec = rel >= 0 ? rel / kTicksPerSecond : (rel - kTicksPerSecond + 1) / kTicksPerSecond;
        int64_t rem_ticks = rel - sec * kTicksPerSecond;
        ts.unix_seconds = sec;
        ts.nanoseconds = static_cast<int32_t>(rem_ticks * 100);
        ts.offset_minutes = 0;
        ts.has_offset = false;
        ts.kind = TimestampKind::Default;
    }
    return ts;
}

} // namespace redox

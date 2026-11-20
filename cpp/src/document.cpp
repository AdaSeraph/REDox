// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#include "redox/document.hpp"
#include "redox/container.hpp"
#include "redox/element.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <limits>
#include <stdexcept>

namespace redox {

namespace {

// Civil date algorithms (Howard Hinnant's public-domain formulas)
void days_to_civil(int64_t z, int& y, unsigned& m, unsigned& d) noexcept {
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const uint64_t doe = static_cast<uint64_t>(z - era * 146097);
    const uint64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t yr = static_cast<int64_t>(yoe) + era * 400;
    const uint64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const uint64_t mp = (5 * doy + 2) / 153;
    d = static_cast<unsigned>(doy - (153 * mp + 2) / 5 + 1);
    m = static_cast<unsigned>(mp < 10 ? mp + 3 : mp - 9);
    y = static_cast<int>(yr + (m <= 2));
}

int64_t civil_to_days(int y, unsigned m, unsigned d) noexcept {
    y -= (m <= 2);
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const uint64_t yoe = static_cast<uint64_t>(y - era * 400);
    const uint64_t doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    const uint64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

} // namespace

std::string Timestamp::to_iso8601() const {
    int64_t local_sec = unix_seconds + static_cast<int64_t>(offset_minutes) * 60;
    int64_t days = local_sec >= 0 ? local_sec / 86400 : (local_sec - 86399) / 86400;
    int64_t rem = local_sec - days * 86400;
    int hour = static_cast<int>(rem / 3600);
    int minute = static_cast<int>((rem % 3600) / 60);
    int second = static_cast<int>(rem % 60);
    int year = 0;
    unsigned month = 0, day = 0;
    days_to_civil(days, year, month, day);

    char buf[64];
    int n = std::snprintf(buf, sizeof(buf), "%04d-%02u-%02uT%02d:%02d:%02d",
                          year, month, day, hour, minute, second);
    std::string result(buf, static_cast<size_t>(n));

    if (nanoseconds > 0) {
        char frac[16];
        std::snprintf(frac, sizeof(frac), ".%09d", nanoseconds);
        std::string_view fv(frac);
        while (fv.size() > 2 && fv.back() == '0') {
            fv.remove_suffix(1);
        }
        result.append(fv);
    }

    if (has_offset) {
        if (offset_minutes == 0) {
            result.push_back('Z');
        } else {
            char sign = offset_minutes >= 0 ? '+' : '-';
            int abs_off = offset_minutes >= 0 ? offset_minutes : -offset_minutes;
            char obuf[16];
            std::snprintf(obuf, sizeof(obuf), "%c%02d:%02d", sign, abs_off / 60, abs_off % 60);
            result.append(obuf);
        }
    } else {
        result.push_back('Z');
    }
    return result;
}

bool Timestamp::try_parse_iso8601(std::string_view text, Timestamp& out) noexcept {
    if (text.size() < 19) return false;
    auto parse_u = [&](size_t pos, size_t len, int& v) -> bool {
        if (pos + len > text.size()) return false;
        auto [ptr, ec] = std::from_chars(text.data() + pos, text.data() + pos + len, v);
        return ec == std::errc{} && ptr == text.data() + pos + len;
    };
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    if (text[4] != '-' || text[7] != '-' || (text[10] != 'T' && text[10] != 't' && text[10] != ' ') ||
        text[13] != ':' || text[16] != ':') {
        return false;
    }
    if (!parse_u(0, 4, year) || !parse_u(5, 2, month) || !parse_u(8, 2, day) ||
        !parse_u(11, 2, hour) || !parse_u(14, 2, minute) || !parse_u(17, 2, second)) {
        return false;
    }
    if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 60) {
        return false;
    }

    size_t pos = 19;
    int32_t nanos = 0;
    if (pos < text.size() && text[pos] == '.') {
        ++pos;
        int scale = 100000000;
        while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
            if (scale > 0) {
                nanos += (text[pos] - '0') * scale;
                scale /= 10;
            }
            ++pos;
        }
    }

    bool has_offset = false;
    int32_t offset_minutes = 0;
    if (pos < text.size()) {
        if (text[pos] == 'Z' || text[pos] == 'z') {
            has_offset = true;
            ++pos;
        } else if (text[pos] == '+' || text[pos] == '-') {
            int sign = (text[pos] == '+') ? 1 : -1;
            ++pos;
            int oh = 0, om = 0;
            if (!parse_u(pos, 2, oh)) return false;
            pos += 2;
            if (pos < text.size() && text[pos] == ':') ++pos;
            if (!parse_u(pos, 2, om)) return false;
            pos += 2;
            offset_minutes = sign * (oh * 60 + om);
            has_offset = true;
        }
    }
    if (pos != text.size()) return false;

    int64_t days = civil_to_days(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
    int64_t sec = days * 86400 + hour * 3600 + minute * 60 + second - static_cast<int64_t>(offset_minutes) * 60;
    out.unix_seconds = sec;
    out.nanoseconds = nanos;
    out.offset_minutes = offset_minutes;
    out.has_offset = has_offset;
    out.kind = has_offset ? TimestampKind::OffsetDateTime : TimestampKind::LocalDateTime;
    return true;
}

// ============================================================================
// Document
// ============================================================================

Document::Document() {
    tokens_.push_back(DToken::make_nop());
    extends_.emplace_back(std::monostate{});
}

Document::~Document() = default;

void Document::reset() {
    tokens_.clear();
    tokens_.push_back(DToken::make_nop());
    extends_.clear();
    extends_.emplace_back(std::monostate{});
    root_id_ = 0;
    empty_token_id_ = 0;
    empty_extend_id_ = 0;
    ++version_;
    parent_table_.clear();
}

DElement Document::root_element() const noexcept {
    return DElement(const_cast<Document*>(this), root_id_);
}

DToken Document::token_at(uint32_t id) const {
    if (id == 0 || id >= tokens_.size()) {
        throw std::out_of_range("Token id out of range");
    }
    return tokens_[id];
}

void Document::set_token(uint32_t id, DToken token) {
    if (id == 0 || id >= tokens_.size()) {
        throw std::out_of_range("Token id out of range");
    }
    tokens_[id] = token;
    invalidate_parent_table();
}

uint32_t Document::alloc_token(DToken token) {
    uint32_t id = static_cast<uint32_t>(tokens_.size());
    tokens_.push_back(token);
    invalidate_parent_table();
    return id;
}

uint32_t Document::alloc_empty_token(DToken token) {
    if (empty_token_id_ != 0) {
        uint32_t id = empty_token_id_;
        empty_token_id_ = tokens_[id].empty_id();
        tokens_[id] = token;
        invalidate_parent_table();
        return id;
    }
    return alloc_token(token);
}

void Document::free_token(uint32_t token_id) {
    if (token_id == 0 || token_id >= tokens_.size()) return;
    DToken tok = tokens_[token_id];
    if (tok.is_extended()) {
        if (!tok.is_extend_inline_literal() && !tok.is_empty()) {
            uint32_t ext_id = tok.extend_id();
            if (ext_id != 0) {
                free_extend(ext_id);
            }
        }
        if (!tok.is_inline_payload() && !tok.is_container() && !tok.is_empty()) {
            uint32_t triv_id = tok.trivia_id();
            if (triv_id != 0) {
                free_extend(triv_id);
            }
        }
    }
    tokens_[token_id] = DToken::make_empty(empty_token_id_);
    empty_token_id_ = token_id;
    invalidate_parent_table();
}

uint32_t Document::alloc_extend(ExtendValue value) {
    if (empty_extend_id_ != 0) {
        uint32_t id = empty_extend_id_;
        // Reclaim next empty id stored as int64_t sentinel in extends_[id]
        if (auto* next = std::get_if<int64_t>(&extends_[id])) {
            empty_extend_id_ = static_cast<uint32_t>(*next);
        } else {
            empty_extend_id_ = 0;
        }
        extends_[id] = std::move(value);
        return id;
    }
    uint32_t id = static_cast<uint32_t>(extends_.size());
    extends_.push_back(std::move(value));
    return id;
}

void Document::free_extend(uint32_t extend_id) {
    if (extend_id == 0 || extend_id >= extends_.size()) return;
    extends_[extend_id] = static_cast<int64_t>(empty_extend_id_);
    empty_extend_id_ = extend_id;
}

ExtendValue& Document::extend_at(uint32_t extend_id) {
    if (extend_id == 0 || extend_id >= extends_.size()) {
        throw std::out_of_range("Extend id out of range");
    }
    return extends_[extend_id];
}

const ExtendValue& Document::extend_at(uint32_t extend_id) const {
    if (extend_id == 0 || extend_id >= extends_.size()) {
        throw std::out_of_range("Extend id out of range");
    }
    return extends_[extend_id];
}

uint32_t Document::create_null_token(NullKind kind) {
    DTokenVariant v = (kind == NullKind::Undefined) ? DTokenVariant::NullUndefined : DTokenVariant::Null;
    return alloc_empty_token(DToken::make_extend_literal(v, 0));
}

uint32_t Document::create_bool_token(bool value) {
    DTokenVariant v = value ? DTokenVariant::BooleanTrue : DTokenVariant::BooleanFalse;
    return alloc_empty_token(DToken::make_extend_literal(v, 0));
}

uint32_t Document::create_int64_token(int64_t value, IntegerKind kind) {
    if (value >= DToken::kMinInlineInteger && value <= DToken::kMaxInlineInteger) {
        return alloc_empty_token(DToken::make_extend_inline_integer(kind, value));
    }
    uint32_t ext_id = alloc_extend(value);
    DTokenVariant v = static_cast<DTokenVariant>((static_cast<uint8_t>(DTokenKind::Integer) << 3) | static_cast<uint8_t>(kind));
    return alloc_empty_token(DToken::make_extend(v, 0, ext_id));
}

uint32_t Document::create_uint64_token(uint64_t value) {
    if (value <= static_cast<uint64_t>(DToken::kMaxInlineInteger)) {
        return alloc_empty_token(DToken::make_extend_inline_integer(IntegerKind::Unsigned, static_cast<int64_t>(value)));
    }
    uint32_t ext_id = alloc_extend(value);
    return alloc_empty_token(DToken::make_extend(DTokenVariant::IntegerUnsigned, 0, ext_id));
}

uint32_t Document::create_double_token(double value, FloatKind kind) {
    if (kind == FloatKind::Default) {
        int64_t payload = DToken::encode_inline_float(value);
        if (payload >= 0) {
            return alloc_empty_token(DToken::make_extend_inline_float(payload));
        }
    }
    uint32_t ext_id = alloc_extend(value);
    DTokenVariant v = static_cast<DTokenVariant>((static_cast<uint8_t>(DTokenKind::Float) << 3) | static_cast<uint8_t>(kind));
    return alloc_empty_token(DToken::make_extend(v, 0, ext_id));
}

uint32_t Document::create_string_token(std::string value, StringKind kind) {
    uint32_t ext_id = alloc_extend(std::move(value));
    DTokenVariant v = static_cast<DTokenVariant>((static_cast<uint8_t>(DTokenKind::String) << 3) | static_cast<uint8_t>(kind));
    return alloc_empty_token(DToken::make_extend(v, 0, ext_id));
}

uint32_t Document::create_bytes_token(std::vector<uint8_t> bytes, ByteStringKind kind) {
    uint32_t ext_id = alloc_extend(std::move(bytes));
    DTokenVariant v = static_cast<DTokenVariant>((static_cast<uint8_t>(DTokenKind::ByteString) << 3) | static_cast<uint8_t>(kind));
    return alloc_empty_token(DToken::make_extend(v, 0, ext_id));
}

uint32_t Document::create_timestamp_token(Timestamp ts) {
    DTokenVariant v = static_cast<DTokenVariant>((static_cast<uint8_t>(DTokenKind::Timestamp) << 3) | static_cast<uint8_t>(ts.kind));
    uint32_t ext_id = alloc_extend(ts);
    return alloc_empty_token(DToken::make_extend(v, 0, ext_id));
}

uint32_t Document::create_empty_object_token() {
    uint32_t token_id = alloc_empty_token(DToken::make_nop());
    auto obj = std::make_unique<DObject>(this, token_id, std::vector<DObject::Entry>{});
    uint32_t ext_id = alloc_extend(std::move(obj));
    tokens_[token_id] = DToken::make_extend_container(DTokenType::Map, ext_id, 0);
    return token_id;
}

uint32_t Document::create_empty_array_token() {
    uint32_t token_id = alloc_empty_token(DToken::make_nop());
    auto arr = std::make_unique<DArray>(this, token_id, std::vector<uint32_t>{});
    uint32_t ext_id = alloc_extend(std::move(arr));
    tokens_[token_id] = DToken::make_extend_container(DTokenType::Array, ext_id, 0);
    return token_id;
}

uint32_t Document::create_empty_map_token() {
    uint32_t token_id = alloc_empty_token(DToken::make_nop());
    auto m = std::make_unique<DMap>(this, token_id, std::vector<DMap::Entry>{});
    uint32_t ext_id = alloc_extend(std::move(m));
    tokens_[token_id] = DToken::make_extend_container(DTokenType::Map, ext_id, 0);
    return token_id;
}

uint32_t Document::create_value_token(const DValue& value) {
    if (value.is_empty_object_marker()) return create_empty_object_token();
    if (value.is_empty_array_marker()) return create_empty_array_token();

    return std::visit([this](const auto& v) -> uint32_t {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return create_null_token();
        } else if constexpr (std::is_same_v<T, bool>) {
            return create_bool_token(v);
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return create_int64_token(v);
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            return create_uint64_token(v);
        } else if constexpr (std::is_same_v<T, double>) {
            return create_double_token(v);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return create_string_token(v);
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            return create_bytes_token(v);
        } else if constexpr (std::is_same_v<T, Timestamp>) {
            return create_timestamp_token(v);
        } else if constexpr (std::is_same_v<T, DElement>) {
            if (!v.is_valid()) return create_null_token();
            return clone_subtree(*v.document(), v.id());
        }
    }, value.storage());
}

uint32_t Document::clone_subtree(const Document& src, uint32_t src_token_id) {
    DToken tok = src.token_at(src_token_id);
    uint32_t new_id = 0;

    if (tok.type() == DTokenType::Array) {
        new_id = create_empty_array_token();
        auto& arr = *std::get<std::unique_ptr<DArray>>(extends_[tokens_[new_id].extend_id()]);
        auto src_elems = src.array_elements(src_token_id);
        std::vector<uint32_t> cloned;
        cloned.reserve(src_elems.size());
        for (uint32_t cid : src_elems) {
            cloned.push_back(clone_subtree(src, cid));
        }
        arr = DArray(this, new_id, std::move(cloned));
    } else if (tok.type() == DTokenType::Map) {
        new_id = create_empty_object_token();
        auto& obj = *std::get<std::unique_ptr<DObject>>(extends_[tokens_[new_id].extend_id()]);
        auto src_entries = src.map_entries(src_token_id);
        std::vector<DObject::Entry> cloned;
        cloned.reserve(src_entries.size());
        for (auto [kid, vid] : src_entries) {
            uint32_t ck = clone_subtree(src, kid);
            uint32_t cv = clone_subtree(src, vid);
            cloned.push_back({ck, cv});
        }
        obj = DObject(this, new_id, std::move(cloned));
    } else {
        switch (tok.kind()) {
            case DTokenKind::Null:
                new_id = create_null_token(static_cast<NullKind>(tok.sub_kind()));
                break;
            case DTokenKind::Boolean:
                new_id = create_bool_token(src.resolve_bool(src_token_id));
                break;
            case DTokenKind::Integer:
                if (tok.integer_kind() == IntegerKind::Unsigned) {
                    new_id = create_uint64_token(src.resolve_uint64(src_token_id));
                } else {
                    new_id = create_int64_token(src.resolve_int64(src_token_id), tok.integer_kind());
                }
                break;
            case DTokenKind::Float:
            case DTokenKind::InlineFloat:
                new_id = create_double_token(src.resolve_double(src_token_id));
                break;
            case DTokenKind::BigNumber:
            case DTokenKind::String:
            case DTokenKind::Symbol:
                new_id = create_string_token(src.resolve_string(src_token_id),
                                             tok.kind() == DTokenKind::String ? tok.string_kind() : StringKind::Default);
                break;
            case DTokenKind::ByteString:
                new_id = create_bytes_token(src.resolve_bytes(src_token_id), tok.byte_string_kind());
                break;
            case DTokenKind::Timestamp:
                new_id = create_timestamp_token(src.resolve_timestamp(src_token_id));
                break;
            default:
                new_id = create_null_token();
                break;
        }
    }

    // Preserve attached trivia if any
    std::vector<DTrivia> leading, trailing;
    src.collect_leading_trivia(src_token_id, leading);
    src.collect_trailing_trivia(src_token_id, trailing);
    if (!leading.empty() || !trailing.empty()) {
        auto& tl = ensure_trivia(new_id);
        tl.leading = std::move(leading);
        tl.trailing = std::move(trailing);
    }

    return new_id;
}

std::unique_ptr<Document> Document::duplicate() const {
    auto copy = std::make_unique<MemoryDocument>();
    if (is_valid()) {
        uint32_t new_root = copy->clone_subtree(*this, root_id_);
        copy->set_root_id(new_root);
    }
    return copy;
}

bool Document::resolve_bool(uint32_t token_id) const {
    DToken tok = token_at(token_id);
    if (tok.kind() != DTokenKind::Boolean) {
        throw FormatException("Token is not a boolean");
    }
    return tok.variant() == DTokenVariant::BooleanTrue;
}

int64_t Document::resolve_int64(uint32_t token_id) const {
    DToken tok = token_at(token_id);
    if (tok.is_extended()) {
        if (tok.kind() == DTokenKind::Integer) {
            if (tok.is_inline_payload()) {
                return DToken::decode_inline_integer(tok);
            }
            const auto& ext = extend_at(tok.extend_id());
            if (auto* p = std::get_if<int64_t>(&ext)) return *p;
            if (auto* u = std::get_if<uint64_t>(&ext)) {
                if (*u <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
                    return static_cast<int64_t>(*u);
                }
                throw FormatException("Unsigned integer overflows int64");
            }
        } else if (tok.kind() == DTokenKind::InlineFloat) {
            return static_cast<int64_t>(DToken::decode_inline_float(tok));
        } else if (tok.kind() == DTokenKind::Float) {
            if (tok.is_inline_payload()) return static_cast<int64_t>(DToken::decode_inline_single(tok));
            return static_cast<int64_t>(std::get<double>(extend_at(tok.extend_id())));
        }
        throw FormatException("Token is not an integer");
    }
    if (tok.kind() == DTokenKind::Integer) {
        return decode_integer(tok);
    }
    if (tok.kind() == DTokenKind::InlineFloat) {
        return static_cast<int64_t>(DToken::decode_inline_float(tok));
    }
    if (tok.kind() == DTokenKind::Float) {
        return static_cast<int64_t>(decode_float(tok));
    }
    throw FormatException("Token is not an integer");
}

uint64_t Document::resolve_uint64(uint32_t token_id) const {
    DToken tok = token_at(token_id);
    if (tok.is_extended()) {
        if (tok.kind() == DTokenKind::Integer) {
            if (tok.is_inline_payload()) {
                int64_t v = DToken::decode_inline_integer(tok);
                if (v < 0) throw FormatException("Negative integer cannot convert to uint64");
                return static_cast<uint64_t>(v);
            }
            const auto& ext = extend_at(tok.extend_id());
            if (auto* u = std::get_if<uint64_t>(&ext)) return *u;
            if (auto* p = std::get_if<int64_t>(&ext)) {
                if (*p < 0) throw FormatException("Negative integer cannot convert to uint64");
                return static_cast<uint64_t>(*p);
            }
        }
        throw FormatException("Token is not an unsigned integer");
    }
    if (tok.kind() == DTokenKind::Integer) {
        return decode_uinteger(tok);
    }
    throw FormatException("Token is not an unsigned integer");
}

double Document::resolve_double(uint32_t token_id) const {
    DToken tok = token_at(token_id);
    if (tok.kind() == DTokenKind::InlineFloat) {
        return DToken::decode_inline_float(tok);
    }
    if (tok.is_extended()) {
        if (tok.kind() == DTokenKind::Float) {
            if (tok.is_inline_payload()) return static_cast<double>(DToken::decode_inline_single(tok));
            return std::get<double>(extend_at(tok.extend_id()));
        }
        if (tok.kind() == DTokenKind::Integer) {
            if (tok.is_inline_payload()) return static_cast<double>(DToken::decode_inline_integer(tok));
            const auto& ext = extend_at(tok.extend_id());
            if (auto* p = std::get_if<int64_t>(&ext)) return static_cast<double>(*p);
            if (auto* u = std::get_if<uint64_t>(&ext)) return static_cast<double>(*u);
        }
        throw FormatException("Token is not numeric");
    }
    if (tok.kind() == DTokenKind::Float) {
        return decode_float(tok);
    }
    if (tok.kind() == DTokenKind::Integer) {
        if (tok.integer_kind() == IntegerKind::Unsigned) {
            return static_cast<double>(decode_uinteger(tok));
        }
        return static_cast<double>(decode_integer(tok));
    }
    throw FormatException("Token is not numeric");
}

std::string Document::resolve_string(uint32_t token_id) const {
    DToken tok = token_at(token_id);
    if (tok.is_extended()) {
        if (tok.kind() == DTokenKind::String || tok.kind() == DTokenKind::Symbol || tok.kind() == DTokenKind::BigNumber) {
            return std::get<std::string>(extend_at(tok.extend_id()));
        }
        throw FormatException("Token is not a string");
    }
    if (tok.kind() == DTokenKind::String || tok.kind() == DTokenKind::Symbol || tok.kind() == DTokenKind::BigNumber) {
        return decode_string(tok);
    }
    throw FormatException("Token is not a string");
}

std::string_view Document::resolve_string_view_or_empty(uint32_t token_id, bool& is_direct_view) const {
    DToken tok = token_at(token_id);
    if (tok.is_extended()) {
        if (tok.kind() == DTokenKind::String || tok.kind() == DTokenKind::Symbol) {
            is_direct_view = true;
            return std::get<std::string>(extend_at(tok.extend_id()));
        }
        is_direct_view = false;
        return {};
    }
    return decode_raw_utf8(tok, is_direct_view);
}

std::vector<uint8_t> Document::resolve_bytes(uint32_t token_id) const {
    DToken tok = token_at(token_id);
    if (tok.is_extended()) {
        if (tok.kind() == DTokenKind::ByteString) {
            return std::get<std::vector<uint8_t>>(extend_at(tok.extend_id()));
        }
        throw FormatException("Token is not a byte string");
    }
    if (tok.kind() == DTokenKind::ByteString) {
        return decode_bytes(tok);
    }
    throw FormatException("Token is not a byte string");
}

Timestamp Document::resolve_timestamp(uint32_t token_id) const {
    DToken tok = token_at(token_id);
    if (tok.is_extended()) {
        if (tok.kind() == DTokenKind::Timestamp) {
            return std::get<Timestamp>(extend_at(tok.extend_id()));
        }
        throw FormatException("Token is not a timestamp");
    }
    if (tok.kind() == DTokenKind::Timestamp) {
        return decode_timestamp(tok);
    }
    throw FormatException("Token is not a timestamp");
}

TriviaList* Document::get_trivia_mut(uint32_t token_id) {
    DToken tok = token_at(token_id);
    if (tok.is_extended() && !tok.is_inline_payload() && !tok.is_container() && !tok.is_empty()) {
        uint32_t tid = tok.trivia_id();
        if (tid != 0) {
            return std::get_if<TriviaList>(&extends_[tid]);
        }
    }
    return nullptr;
}

const TriviaList* Document::get_trivia(uint32_t token_id) const {
    DToken tok = token_at(token_id);
    if (tok.is_extended() && !tok.is_inline_payload() && !tok.is_container() && !tok.is_empty()) {
        uint32_t tid = tok.trivia_id();
        if (tid != 0) {
            return std::get_if<TriviaList>(&extends_[tid]);
        }
    }
    return nullptr;
}

TriviaList& Document::ensure_trivia(uint32_t token_id) {
    DToken tok = token_at(token_id);
    // If inline extended token, promote to non-inline extended token so it can hold trivia_id
    if (tok.is_extended() && tok.is_inline_payload()) {
        if (tok.kind() == DTokenKind::Integer) {
            int64_t val = DToken::decode_inline_integer(tok);
            uint32_t ext_id = alloc_extend(val);
            DTokenVariant v = static_cast<DTokenVariant>((static_cast<uint8_t>(DTokenKind::Integer) << 3) |
                                                         static_cast<uint8_t>(tok.integer_kind()));
            tok = DToken::make_extend(v, 0, ext_id);
            tokens_[token_id] = tok;
        } else if (tok.kind() == DTokenKind::InlineFloat || tok.kind() == DTokenKind::Float) {
            double val = (tok.kind() == DTokenKind::InlineFloat)
                             ? DToken::decode_inline_float(tok)
                             : static_cast<double>(DToken::decode_inline_single(tok));
            uint32_t ext_id = alloc_extend(val);
            tok = DToken::make_extend(DTokenVariant::Float, 0, ext_id);
            tokens_[token_id] = tok;
        }
    } else if (!tok.is_extended() && !tok.is_container()) {
        // Promote source-backed scalar to extended so trivia can be attached
        if (tok.kind() == DTokenKind::Null) {
            tok = DToken::make_extend_literal(tok.variant(), 0);
        } else if (tok.kind() == DTokenKind::Boolean) {
            tok = DToken::make_extend_literal(tok.variant(), 0);
        } else if (tok.kind() == DTokenKind::Integer) {
            uint32_t ext_id = alloc_extend(decode_integer(tok));
            tok = DToken::make_extend(tok.variant(), 0, ext_id);
        } else if (tok.kind() == DTokenKind::Float || tok.kind() == DTokenKind::InlineFloat) {
            uint32_t ext_id = alloc_extend(resolve_double(token_id));
            tok = DToken::make_extend(DTokenVariant::Float, 0, ext_id);
        } else if (tok.kind() == DTokenKind::String || tok.kind() == DTokenKind::Symbol) {
            uint32_t ext_id = alloc_extend(decode_string(tok));
            tok = DToken::make_extend(tok.variant(), 0, ext_id);
        }
        tokens_[token_id] = tok;
    }

    if (tok.is_container()) {
        // Store trivia in a dedicated slot? Containers don't have trivia_id bits; for simplicity,
        // leading/trailing trivia on containers is handled on their preceding tape tokens or key tokens.
        // We still return a static empty/fallback if called on a container directly.
        static thread_local TriviaList dummy;
        dummy = {};
        return dummy;
    }

    uint32_t tid = tok.trivia_id();
    if (tid == 0) {
        tid = alloc_extend(TriviaList{});
        tok.set_trivia_id(tid);
        tokens_[token_id] = tok;
    }
    return std::get<TriviaList>(extends_[tid]);
}

void Document::collect_leading_trivia(uint32_t token_id, std::vector<DTrivia>& out) const {
    // 1. Extended trivia attached directly
    if (const auto* tl = get_trivia(token_id)) {
        out.insert(out.end(), tl->leading.begin(), tl->leading.end());
        return;
    }
    // 2. Leading trivia tokens immediately preceding token_id on the tape
    std::vector<DTrivia> rev;
    uint32_t cur = token_id;
    while (cur > 1) {
        DToken prev = tokens_[cur - 1];
        if (prev.kind() == DTokenKind::Trivia && !prev.is_extended()) {
            rev.push_back({prev.trivia_kind(), decode_trivia(prev)});
            --cur;
        } else {
            break;
        }
    }
    for (auto it = rev.rbegin(); it != rev.rend(); ++it) {
        out.push_back(std::move(*it));
    }
}

void Document::collect_trailing_trivia(uint32_t token_id, std::vector<DTrivia>& out) const {
    if (const auto* tl = get_trivia(token_id)) {
        out.insert(out.end(), tl->trailing.begin(), tl->trailing.end());
    }
}

uint32_t Document::skip_trivia(uint32_t token_id) const {
    while (token_id < tokens_.size()) {
        DToken tok = tokens_[token_id];
        if (tok.kind() == DTokenKind::Trivia) {
            ++token_id;
        } else if (tok.kind() == DTokenKind::Control && !tok.is_extended() && tok.raw != 0) {
            token_id = tok.jump_id();
        } else {
            break;
        }
    }
    return token_id;
}

uint32_t Document::next_token_id(uint32_t token_id) const {
    DToken tok = token_at(token_id);
    uint32_t next = (tok.is_container() && tok.link_id() != 0) ? tok.link_id() : (token_id + 1);
    return skip_trivia(next);
}

size_t Document::container_count(uint32_t token_id) const {
    DToken tok = token_at(token_id);
    if (!tok.is_container()) {
        throw FormatException("Token is not a container");
    }
    if (!tok.is_extended()) {
        return tok.count();
    }
    const auto& ext = extend_at(tok.extend_id());
    if (auto* arr = std::get_if<std::unique_ptr<DArray>>(&ext)) return (*arr)->count();
    if (auto* obj = std::get_if<std::unique_ptr<DObject>>(&ext)) return (*obj)->count();
    if (auto* m = std::get_if<std::unique_ptr<DMap>>(&ext)) return (*m)->count();
    return 0;
}

std::vector<uint32_t> Document::array_elements(uint32_t token_id) const {
    DToken tok = token_at(token_id);
    if (tok.type() != DTokenType::Array) {
        throw FormatException("Token is not an array");
    }
    if (tok.is_extended()) {
        return std::get<std::unique_ptr<DArray>>(extend_at(tok.extend_id()))->item_ids();
    }
    uint32_t cnt = tok.count();
    std::vector<uint32_t> out;
    out.reserve(cnt);
    uint32_t cur = skip_trivia(token_id + 1);
    for (uint32_t i = 0; i < cnt && cur < tokens_.size(); ++i) {
        out.push_back(cur);
        cur = next_token_id(cur);
    }
    return out;
}

std::vector<std::pair<uint32_t, uint32_t>> Document::map_entries(uint32_t token_id) const {
    DToken tok = token_at(token_id);
    if (tok.type() != DTokenType::Map) {
        throw FormatException("Token is not a map/object");
    }
    if (tok.is_extended()) {
        const auto& ext = extend_at(tok.extend_id());
        std::vector<std::pair<uint32_t, uint32_t>> out;
        if (auto* obj = std::get_if<std::unique_ptr<DObject>>(&ext)) {
            out.reserve((*obj)->count());
            for (const auto& e : (*obj)->entries()) out.emplace_back(e.key_id, e.value_id);
            return out;
        }
        if (auto* m = std::get_if<std::unique_ptr<DMap>>(&ext)) {
            out.reserve((*m)->count());
            for (const auto& e : (*m)->entries()) out.emplace_back(e.key_id, e.value_id);
            return out;
        }
    }
    uint32_t cnt = tok.count();
    std::vector<std::pair<uint32_t, uint32_t>> out;
    out.reserve(cnt);
    uint32_t cur = skip_trivia(token_id + 1);
    for (uint32_t i = 0; i < cnt && cur < tokens_.size(); ++i) {
        uint32_t key_id = cur;
        uint32_t val_id = next_token_id(key_id);
        out.emplace_back(key_id, val_id);
        cur = next_token_id(val_id);
    }
    return out;
}

DObject& Document::materialize_object(uint32_t token_id) {
    DToken tok = token_at(token_id);
    if (tok.type() != DTokenType::Map) {
        throw FormatException("Token is not an object");
    }
    if (tok.is_extended()) {
        auto& ext = extend_at(tok.extend_id());
        if (auto* obj = std::get_if<std::unique_ptr<DObject>>(&ext)) {
            return **obj;
        }
        if (auto* m = std::get_if<std::unique_ptr<DMap>>(&ext)) {
            std::vector<DObject::Entry> entries;
            entries.reserve((*m)->count());
            for (const auto& e : (*m)->entries()) entries.push_back({e.key_id, e.value_id});
            ext = std::make_unique<DObject>(this, token_id, std::move(entries));
            return *std::get<std::unique_ptr<DObject>>(ext);
        }
    }
    auto pairs = map_entries(token_id);
    std::vector<DObject::Entry> entries;
    entries.reserve(pairs.size());
    for (auto [k, v] : pairs) entries.push_back({k, v});
    uint32_t link = tok.link_id();
    uint32_t ext_id = alloc_extend(std::make_unique<DObject>(this, token_id, std::move(entries)));
    tokens_[token_id] = DToken::make_extend_container(DTokenType::Map, ext_id, link);
    return *std::get<std::unique_ptr<DObject>>(extends_[ext_id]);
}

DArray& Document::materialize_array(uint32_t token_id) {
    DToken tok = token_at(token_id);
    if (tok.type() != DTokenType::Array) {
        throw FormatException("Token is not an array");
    }
    if (tok.is_extended()) {
        return *std::get<std::unique_ptr<DArray>>(extend_at(tok.extend_id()));
    }
    auto elems = array_elements(token_id);
    uint32_t link = tok.link_id();
    uint32_t ext_id = alloc_extend(std::make_unique<DArray>(this, token_id, std::move(elems)));
    tokens_[token_id] = DToken::make_extend_container(DTokenType::Array, ext_id, link);
    return *std::get<std::unique_ptr<DArray>>(extends_[ext_id]);
}

DMap& Document::materialize_map(uint32_t token_id) {
    DToken tok = token_at(token_id);
    if (tok.type() != DTokenType::Map) {
        throw FormatException("Token is not a map");
    }
    if (tok.is_extended()) {
        auto& ext = extend_at(tok.extend_id());
        if (auto* m = std::get_if<std::unique_ptr<DMap>>(&ext)) {
            return **m;
        }
        if (auto* obj = std::get_if<std::unique_ptr<DObject>>(&ext)) {
            std::vector<DMap::Entry> entries;
            entries.reserve((*obj)->count());
            for (const auto& e : (*obj)->entries()) entries.push_back({e.key_id, e.value_id});
            ext = std::make_unique<DMap>(this, token_id, std::move(entries));
            return *std::get<std::unique_ptr<DMap>>(ext);
        }
    }
    auto pairs = map_entries(token_id);
    std::vector<DMap::Entry> entries;
    entries.reserve(pairs.size());
    for (auto [k, v] : pairs) entries.push_back({k, v});
    uint32_t link = tok.link_id();
    uint32_t ext_id = alloc_extend(std::make_unique<DMap>(this, token_id, std::move(entries)));
    tokens_[token_id] = DToken::make_extend_container(DTokenType::Map, ext_id, link);
    return *std::get<std::unique_ptr<DMap>>(extends_[ext_id]);
}

void Document::invalidate_parent_table() noexcept {
    if (!parent_table_.empty()) {
        parent_table_[0] = 0;
    }
}

void Document::build_parent_table() const {
    parent_table_.assign(tokens_.size(), 0);
    if (!is_valid()) return;
    parent_table_[0] = static_cast<uint32_t>(tokens_.size());

    std::vector<uint32_t> stack;
    stack.push_back(root_id_);
    while (!stack.empty()) {
        uint32_t pid = stack.back();
        stack.pop_back();
        DToken tok = tokens_[pid];
        if (tok.type() == DTokenType::Array) {
            for (uint32_t cid : array_elements(pid)) {
                if (cid < parent_table_.size()) {
                    parent_table_[cid] = pid;
                    if (tokens_[cid].is_container()) stack.push_back(cid);
                }
            }
        } else if (tok.type() == DTokenType::Map) {
            for (auto [kid, vid] : map_entries(pid)) {
                if (kid < parent_table_.size()) parent_table_[kid] = pid;
                if (vid < parent_table_.size()) {
                    parent_table_[vid] = pid;
                    if (tokens_[vid].is_container()) stack.push_back(vid);
                }
            }
        }
    }
}

uint32_t Document::parent_of(uint32_t token_id) const {
    if (parent_table_.empty() || parent_table_[0] != tokens_.size()) {
        build_parent_table();
    }
    if (token_id == 0 || token_id >= parent_table_.size()) return 0;
    return parent_table_[token_id];
}

// Default format hooks (overridden by concrete format documents)
std::string Document::decode_string(DToken) const { throw FormatException("Unsupported source string decoding"); }
std::string_view Document::decode_raw_utf8(DToken, bool& ok) const { ok = false; return {}; }
int64_t Document::decode_integer(DToken) const { throw FormatException("Unsupported source integer decoding"); }
uint64_t Document::decode_uinteger(DToken tok) const { return static_cast<uint64_t>(decode_integer(tok)); }
double Document::decode_float(DToken) const { throw FormatException("Unsupported source float decoding"); }
std::vector<uint8_t> Document::decode_bytes(DToken) const { throw FormatException("Unsupported source bytes decoding"); }
Timestamp Document::decode_timestamp(DToken) const { throw FormatException("Unsupported source timestamp decoding"); }
std::string Document::decode_trivia(DToken) const { return {}; }

// ============================================================================
// MemoryDocument
// ============================================================================

MemoryDocument::MemoryDocument() = default;

std::unique_ptr<MemoryDocument> MemoryDocument::create_object() {
    auto doc = std::make_unique<MemoryDocument>();
    doc->set_root_id(doc->create_empty_object_token());
    return doc;
}

std::unique_ptr<MemoryDocument> MemoryDocument::create_array() {
    auto doc = std::make_unique<MemoryDocument>();
    doc->set_root_id(doc->create_empty_array_token());
    return doc;
}

std::unique_ptr<MemoryDocument> MemoryDocument::create_map() {
    auto doc = std::make_unique<MemoryDocument>();
    doc->set_root_id(doc->create_empty_map_token());
    return doc;
}

std::unique_ptr<MemoryDocument> MemoryDocument::create_value(const DValue& value) {
    auto doc = std::make_unique<MemoryDocument>();
    doc->set_root_id(doc->create_value_token(value));
    return doc;
}

} // namespace redox

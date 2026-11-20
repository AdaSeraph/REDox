// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <bit>
#include <cassert>
#include <cstdint>
#include <string>
#include <utility>

namespace redox {

enum class DTokenType : uint8_t {
    Ignore    = 0b000,
    Literal   = 0b001,
    Number    = 0b010,
    ExtNumber = 0b011,
    Text      = 0b100,
    Binary    = 0b101,
    Map       = 0b110,
    Array     = 0b111,
};

enum class DTokenKind : uint8_t {
    // 000x: ignore / control or metadata
    Control     = 0b0000,
    Trivia      = 0b0001,
    // 001x: literal primitive
    Boolean     = 0b0010,
    Null        = 0b0011,
    // 010x: number
    Integer     = 0b0100,
    Float       = 0b0101,
    // 011x: extended number
    BigNumber   = 0b0110,
    InlineFloat = 0b0111,
    // 100x: text
    String      = 0b1000,
    Symbol      = 0b1001,
    // 101x: binary
    ByteString  = 0b1010,
    Timestamp   = 0b1011,
    // 11xx: container
    Map         = 0b1100,
    Array       = 0b1110,
};

enum class TriviaKind : uint8_t {
    Default      = 0,
    Whitespace   = 1,
    BlockComment = 2,
    LineComment  = 3,
    Separator    = 4,
    Tag          = 5,
    Style        = 6,
    Inherit      = 7,
};

enum class BooleanKind : uint8_t {
    False = 0,
    True  = 1,
};

enum class NullKind : uint8_t {
    Default   = 0,
    Undefined = 1,
};

enum class IntegerKind : uint8_t {
    Default     = 0,
    Hexadecimal = 1,
    Octal       = 2,
    Binary      = 3,
    Unsigned    = 4,
    Inherit     = 7,
};

enum class FloatKind : uint8_t {
    Default = 0,
    Half    = 1,
    Single  = 2,
    Decimal = 3,
    Inherit = 7,
};

enum class BigNumberKind : uint8_t {
    Default     = 0,
    Int128      = 1,
    UInt128     = 2,
    Integer     = 3,
    Decimal     = 4,
    Float       = 5,
    Hexadecimal = 6,
    Inherit     = 7,
};

enum class StringKind : uint8_t {
    Default              = 0,
    Literal              = 1,
    DoubleQuote          = 2,
    SingleQuote          = 3,
    MultilineDoubleQuote = 4,
    MultilineSingleQuote = 5,
    Inherit              = 7,
};

enum class SymbolKind : uint8_t {
    Default    = 0,
    Identifier = 1,
    Reference  = 2,
    TypeName   = 3,
    Metadata   = 4,
    Inherit    = 7,
};

enum class ByteStringKind : uint8_t {
    Default   = 0,
    Base64    = 1,
    Base64Url = 2,
    Base16    = 3,
    Guid      = 4,
    Raw       = 5,
    Inherit   = 7,
};

enum class TimestampKind : uint8_t {
    Default        = 0,
    OffsetDateTime = 1,
    LocalDateTime  = 2,
    LocalDate      = 3,
    LocalTime      = 4,
    Inherit        = 7,
};

enum class DTokenVariant : uint8_t {
    Undefined                  = 0,

    Trivia                     = (static_cast<uint8_t>(DTokenKind::Trivia) << 3) | static_cast<uint8_t>(TriviaKind::Default),
    TriviaWhitespace           = (static_cast<uint8_t>(DTokenKind::Trivia) << 3) | static_cast<uint8_t>(TriviaKind::Whitespace),
    TriviaBlockComment         = (static_cast<uint8_t>(DTokenKind::Trivia) << 3) | static_cast<uint8_t>(TriviaKind::BlockComment),
    TriviaLineComment          = (static_cast<uint8_t>(DTokenKind::Trivia) << 3) | static_cast<uint8_t>(TriviaKind::LineComment),
    TriviaSeparator            = (static_cast<uint8_t>(DTokenKind::Trivia) << 3) | static_cast<uint8_t>(TriviaKind::Separator),
    TriviaTag                  = (static_cast<uint8_t>(DTokenKind::Trivia) << 3) | static_cast<uint8_t>(TriviaKind::Tag),
    TriviaStyle                = (static_cast<uint8_t>(DTokenKind::Trivia) << 3) | static_cast<uint8_t>(TriviaKind::Style),

    BooleanFalse               = (static_cast<uint8_t>(DTokenKind::Boolean) << 3) | static_cast<uint8_t>(BooleanKind::False),
    BooleanTrue                = (static_cast<uint8_t>(DTokenKind::Boolean) << 3) | static_cast<uint8_t>(BooleanKind::True),

    Null                       = (static_cast<uint8_t>(DTokenKind::Null) << 3) | static_cast<uint8_t>(NullKind::Default),
    NullUndefined              = (static_cast<uint8_t>(DTokenKind::Null) << 3) | static_cast<uint8_t>(NullKind::Undefined),

    String                     = (static_cast<uint8_t>(DTokenKind::String) << 3) | static_cast<uint8_t>(StringKind::Default),
    StringLiteral              = (static_cast<uint8_t>(DTokenKind::String) << 3) | static_cast<uint8_t>(StringKind::Literal),
    StringDoubleQuote          = (static_cast<uint8_t>(DTokenKind::String) << 3) | static_cast<uint8_t>(StringKind::DoubleQuote),
    StringSingleQuote          = (static_cast<uint8_t>(DTokenKind::String) << 3) | static_cast<uint8_t>(StringKind::SingleQuote),
    StringMultilineDoubleQuote = (static_cast<uint8_t>(DTokenKind::String) << 3) | static_cast<uint8_t>(StringKind::MultilineDoubleQuote),
    StringMultilineSingleQuote = (static_cast<uint8_t>(DTokenKind::String) << 3) | static_cast<uint8_t>(StringKind::MultilineSingleQuote),

    Symbol                     = (static_cast<uint8_t>(DTokenKind::Symbol) << 3) | static_cast<uint8_t>(SymbolKind::Default),
    SymbolIdentifier           = (static_cast<uint8_t>(DTokenKind::Symbol) << 3) | static_cast<uint8_t>(SymbolKind::Identifier),
    SymbolReference            = (static_cast<uint8_t>(DTokenKind::Symbol) << 3) | static_cast<uint8_t>(SymbolKind::Reference),
    SymbolTypeName             = (static_cast<uint8_t>(DTokenKind::Symbol) << 3) | static_cast<uint8_t>(SymbolKind::TypeName),
    SymbolMetadata             = (static_cast<uint8_t>(DTokenKind::Symbol) << 3) | static_cast<uint8_t>(SymbolKind::Metadata),

    Float                      = (static_cast<uint8_t>(DTokenKind::Float) << 3) | static_cast<uint8_t>(FloatKind::Default),
    FloatHalf                  = (static_cast<uint8_t>(DTokenKind::Float) << 3) | static_cast<uint8_t>(FloatKind::Half),
    FloatSingle                = (static_cast<uint8_t>(DTokenKind::Float) << 3) | static_cast<uint8_t>(FloatKind::Single),
    FloatDecimal               = (static_cast<uint8_t>(DTokenKind::Float) << 3) | static_cast<uint8_t>(FloatKind::Decimal),

    Integer                    = (static_cast<uint8_t>(DTokenKind::Integer) << 3) | static_cast<uint8_t>(IntegerKind::Default),
    IntegerHexadecimal         = (static_cast<uint8_t>(DTokenKind::Integer) << 3) | static_cast<uint8_t>(IntegerKind::Hexadecimal),
    IntegerOctal               = (static_cast<uint8_t>(DTokenKind::Integer) << 3) | static_cast<uint8_t>(IntegerKind::Octal),
    IntegerBinary              = (static_cast<uint8_t>(DTokenKind::Integer) << 3) | static_cast<uint8_t>(IntegerKind::Binary),
    IntegerUnsigned            = (static_cast<uint8_t>(DTokenKind::Integer) << 3) | static_cast<uint8_t>(IntegerKind::Unsigned),

    BigNumber                  = (static_cast<uint8_t>(DTokenKind::BigNumber) << 3) | static_cast<uint8_t>(BigNumberKind::Default),
    BigNumberInt128            = (static_cast<uint8_t>(DTokenKind::BigNumber) << 3) | static_cast<uint8_t>(BigNumberKind::Int128),
    BigNumberUInt128           = (static_cast<uint8_t>(DTokenKind::BigNumber) << 3) | static_cast<uint8_t>(BigNumberKind::UInt128),
    BigNumberInteger           = (static_cast<uint8_t>(DTokenKind::BigNumber) << 3) | static_cast<uint8_t>(BigNumberKind::Integer),
    BigNumberDecimal           = (static_cast<uint8_t>(DTokenKind::BigNumber) << 3) | static_cast<uint8_t>(BigNumberKind::Decimal),
    BigNumberFloat             = (static_cast<uint8_t>(DTokenKind::BigNumber) << 3) | static_cast<uint8_t>(BigNumberKind::Float),
    BigNumberHexadecimal       = (static_cast<uint8_t>(DTokenKind::BigNumber) << 3) | static_cast<uint8_t>(BigNumberKind::Hexadecimal),

    ByteString                 = (static_cast<uint8_t>(DTokenKind::ByteString) << 3) | static_cast<uint8_t>(ByteStringKind::Default),
    ByteStringBase64           = (static_cast<uint8_t>(DTokenKind::ByteString) << 3) | static_cast<uint8_t>(ByteStringKind::Base64),
    ByteStringBase64Url        = (static_cast<uint8_t>(DTokenKind::ByteString) << 3) | static_cast<uint8_t>(ByteStringKind::Base64Url),
    ByteStringBase16           = (static_cast<uint8_t>(DTokenKind::ByteString) << 3) | static_cast<uint8_t>(ByteStringKind::Base16),
    ByteStringGuid             = (static_cast<uint8_t>(DTokenKind::ByteString) << 3) | static_cast<uint8_t>(ByteStringKind::Guid),
    ByteStringRaw              = (static_cast<uint8_t>(DTokenKind::ByteString) << 3) | static_cast<uint8_t>(ByteStringKind::Raw),

    Timestamp                  = (static_cast<uint8_t>(DTokenKind::Timestamp) << 3) | static_cast<uint8_t>(TimestampKind::Default),
    TimestampOffsetDateTime    = (static_cast<uint8_t>(DTokenKind::Timestamp) << 3) | static_cast<uint8_t>(TimestampKind::OffsetDateTime),
    TimestampLocalDateTime     = (static_cast<uint8_t>(DTokenKind::Timestamp) << 3) | static_cast<uint8_t>(TimestampKind::LocalDateTime),
    TimestampLocalDate         = (static_cast<uint8_t>(DTokenKind::Timestamp) << 3) | static_cast<uint8_t>(TimestampKind::LocalDate),
    TimestampLocalTime         = (static_cast<uint8_t>(DTokenKind::Timestamp) << 3) | static_cast<uint8_t>(TimestampKind::LocalTime),
};

constexpr bool variant_is_inherit(DTokenVariant v) noexcept {
    return (static_cast<uint8_t>(v) & 7u) == 7u;
}

constexpr DTokenKind variant_to_kind(DTokenVariant v) noexcept {
    return static_cast<DTokenKind>(static_cast<uint8_t>(v) >> 3);
}

constexpr DTokenType variant_to_type(DTokenVariant v) noexcept {
    return static_cast<DTokenType>(static_cast<uint8_t>(v) >> 4);
}

// 64-bit dual-mode token strictly matching RE:Dox C# DToken bit layout.
struct DToken {
    static constexpr uint32_t kMaxTokenId    = 0x3fffffffu;
    static constexpr uint32_t kMaxValueCount = 0x3fffffffu;
    static constexpr uint32_t kMaxExtendId   = 0x0fffffffu;

    static constexpr int64_t kSignBit           = static_cast<int64_t>(0x8000000000000000ULL);
    static constexpr int64_t kPayloadMask       = 0x00FFFFFFFFFFFFFFLL;
    static constexpr int64_t kInlineFloatMask   = 0x07FFFFFFFFFFFFFFLL;
    static constexpr int64_t kInlinePayloadMask = 0x0700000000000000LL;

    static constexpr int64_t kMinInlineInteger  = -(1LL << 52);
    static constexpr int64_t kMaxInlineInteger  = (1LL << 52) - 1;

    int64_t raw = 0;

    constexpr DToken() noexcept = default;
    constexpr explicit DToken(int64_t v) noexcept : raw(v) {}

    constexpr bool operator==(const DToken& other) const noexcept = default;

    // --- Factory methods ---
    static constexpr DToken make_nop() noexcept {
        return DToken(0);
    }

    static constexpr DToken make_jump(uint32_t jump_id) noexcept {
        assert(jump_id <= kMaxTokenId);
        return DToken(static_cast<int64_t>(jump_id));
    }

    static constexpr DToken make_array(uint32_t count) noexcept {
        assert(count <= kMaxValueCount);
        return DToken((static_cast<int64_t>(DTokenType::Array) << 60) | static_cast<int64_t>(count));
    }

    static constexpr DToken make_map(uint32_t count) noexcept {
        assert(count <= kMaxValueCount);
        return DToken((static_cast<int64_t>(DTokenType::Map) << 60) | static_cast<int64_t>(count));
    }

    static constexpr DToken make(DTokenVariant variant, int64_t payload = 0) noexcept {
        assert((payload & ~kPayloadMask) == 0);
        return DToken((static_cast<int64_t>(variant) << 56) | payload);
    }

    static constexpr DToken make(DTokenKind kind, int64_t payload = 0) noexcept {
        assert((payload & ~kPayloadMask) == 0);
        return DToken((static_cast<int64_t>(kind) << 59) | payload);
    }

    static constexpr DToken make_inline_float(int64_t payload) noexcept {
        assert((payload & ~kInlineFloatMask) == 0);
        return DToken((static_cast<int64_t>(DTokenKind::InlineFloat) << 59) | payload);
    }

    static constexpr DToken make_extend_container(DTokenType type, uint32_t extend_id, uint32_t link_id = 0) noexcept {
        assert(type == DTokenType::Map || type == DTokenType::Array);
        assert(extend_id <= kMaxExtendId);
        assert(link_id <= kMaxTokenId);
        return DToken(kSignBit | (static_cast<int64_t>(type) << 60) |
                      static_cast<int64_t>(extend_id) | (static_cast<int64_t>(link_id) << 30));
    }

    static constexpr DToken make_extend_literal(DTokenVariant variant, uint32_t trivia_id = 0) noexcept {
        assert(trivia_id <= kMaxExtendId);
        assert(variant_to_type(variant) == DTokenType::Literal);
        return DToken(kSignBit | (static_cast<int64_t>(variant) << 56) | (static_cast<int64_t>(trivia_id) << 28));
    }

    static constexpr DToken make_extend(DTokenVariant variant, uint32_t trivia_id, uint32_t extend_id) noexcept {
        assert(trivia_id <= kMaxExtendId);
        assert(extend_id <= kMaxExtendId);
        return DToken(kSignBit | (static_cast<int64_t>(variant) << 56) |
                      static_cast<int64_t>(extend_id) | (static_cast<int64_t>(trivia_id) << 28));
    }

    static constexpr DToken make_extend_inline_integer(IntegerKind kind, int64_t value) noexcept {
        assert(value >= kMinInlineInteger && value <= kMaxInlineInteger);
        return DToken(kSignBit | (static_cast<int64_t>(DTokenKind::Integer) << 59) |
                      kInlinePayloadMask | (static_cast<int64_t>(kind) << 53) |
                      (value & 0x1fffffffffffffLL));
    }

    static DToken make_extend_inline_single(FloatKind kind, float value) noexcept {
        auto bits = std::bit_cast<uint32_t>(value);
        return DToken(kSignBit | (static_cast<int64_t>(DTokenKind::Float) << 59) |
                      kInlinePayloadMask | static_cast<int64_t>(bits) |
                      (static_cast<int64_t>(kind) << 53));
    }

    static constexpr DToken make_extend_inline_float(int64_t payload) noexcept {
        assert((payload & ~kInlineFloatMask) == 0);
        return DToken(kSignBit | (static_cast<int64_t>(DTokenKind::InlineFloat) << 59) | payload);
    }

    static constexpr DToken make_empty(uint32_t empty_id) noexcept {
        assert(empty_id <= kMaxTokenId);
        return DToken(kSignBit | (static_cast<int64_t>(empty_id) << 28));
    }

    static constexpr DToken make_empty_extend(uint32_t empty_id, uint32_t extend_id) noexcept {
        assert(empty_id <= kMaxTokenId);
        assert(extend_id <= kMaxExtendId);
        return DToken(kSignBit | (static_cast<int64_t>(empty_id) << 28) |
                      static_cast<int64_t>(extend_id) | (1LL << 58));
    }

    // --- Classification ---
    constexpr bool is_extended() const noexcept { return raw < 0; }
    constexpr bool is_empty_token() const noexcept { return (static_cast<uint64_t>(raw) >> 58) == 0b100000ULL; }
    constexpr bool is_empty_extend() const noexcept { return (static_cast<uint64_t>(raw) >> 58) == 0b100001ULL; }
    constexpr bool is_empty() const noexcept { return is_empty_token() || is_empty_extend(); }
    constexpr bool is_leading_trivia() const noexcept { return (static_cast<uint64_t>(raw) >> 59) == 0b00001ULL; }

    constexpr bool is_container() const noexcept { return ((static_cast<uint64_t>(raw) >> 61) & 3ULL) == 3ULL; }
    constexpr bool is_ignore() const noexcept { return ((static_cast<uint64_t>(raw) >> 60) & 7ULL) == 0ULL; }
    constexpr bool is_literal() const noexcept { return ((static_cast<uint64_t>(raw) >> 60) & 7ULL) == 1ULL; }
    constexpr bool is_numeric() const noexcept { return ((static_cast<uint64_t>(raw) >> 61) & 3ULL) == 1ULL; }
    constexpr bool is_string_encoded() const noexcept { return ((static_cast<uint64_t>(raw) >> 61) & 3ULL) == 2ULL; }

    constexpr DTokenType type() const noexcept {
        return static_cast<DTokenType>((static_cast<uint64_t>(raw) >> 60) & 7ULL);
    }

    constexpr DTokenKind kind() const noexcept {
        return static_cast<DTokenKind>((static_cast<uint64_t>(raw) >> 59) & 0x0fULL);
    }

    constexpr DTokenVariant variant() const noexcept {
        return static_cast<DTokenVariant>((static_cast<uint64_t>(raw) >> 56) & 0x7fULL);
    }

    constexpr void set_variant(DTokenVariant v) noexcept {
        raw = static_cast<int64_t>((static_cast<uint64_t>(raw) & 0x80ffffffffffffffULL) |
                                   (static_cast<uint64_t>(v) << 56));
    }

    constexpr bool is_inline_payload() const noexcept {
        return is_extended() && !is_container() &&
               (kind() == DTokenKind::InlineFloat || variant_is_inherit(variant()));
    }

    constexpr bool is_extend_inline_literal() const noexcept {
        return is_extended() && (type() == DTokenType::Literal || is_inline_payload());
    }

    constexpr bool has_inline_sub_kind() const noexcept {
        constexpr uint32_t kInlineFloatHeader = 0x80u | (static_cast<uint32_t>(DTokenKind::InlineFloat) << 3) | 7u;
        uint32_t header = static_cast<uint32_t>(static_cast<uint64_t>(raw) >> 56);
        return ((header & 7u) == 7u) && ((header - 0x80u) < 0x60u) && (header != kInlineFloatHeader);
    }

    // --- Sub-kind accessors ---
    constexpr uint8_t sub_kind() const noexcept {
        return static_cast<uint8_t>((static_cast<uint64_t>(raw) >> 56) & 7ULL);
    }

    constexpr uint8_t inline_sub_kind() const noexcept {
        uint8_t sk = sub_kind();
        if (sk == 7) {
            return is_extended() ? static_cast<uint8_t>((static_cast<uint64_t>(raw) >> 53) & 7ULL) : 0;
        }
        return sk;
    }

    constexpr IntegerKind integer_kind() const noexcept { return static_cast<IntegerKind>(inline_sub_kind()); }
    constexpr FloatKind float_kind() const noexcept { return static_cast<FloatKind>(inline_sub_kind()); }
    constexpr StringKind string_kind() const noexcept { return static_cast<StringKind>(sub_kind()); }
    constexpr SymbolKind symbol_kind() const noexcept { return static_cast<SymbolKind>(sub_kind()); }
    constexpr TriviaKind trivia_kind() const noexcept { return static_cast<TriviaKind>(sub_kind()); }
    constexpr TimestampKind timestamp_kind() const noexcept { return static_cast<TimestampKind>(sub_kind()); }
    constexpr ByteStringKind byte_string_kind() const noexcept { return static_cast<ByteStringKind>(sub_kind()); }
    constexpr BigNumberKind big_number_kind() const noexcept { return static_cast<BigNumberKind>(sub_kind()); }

    // --- Field accessors ---
    constexpr uint32_t extend_id() const noexcept {
        assert(is_extended());
        return static_cast<uint32_t>(raw & kMaxExtendId);
    }

    constexpr uint32_t trivia_id() const noexcept {
        assert(!is_inline_payload() && is_extended() && !is_container());
        return static_cast<uint32_t>((static_cast<uint64_t>(raw) >> 28) & kMaxExtendId);
    }

    constexpr void set_trivia_id(uint32_t id) noexcept {
        assert(!is_inline_payload() && is_extended() && !is_container());
        assert(id <= kMaxExtendId);
        raw = static_cast<int64_t>((static_cast<uint64_t>(raw) & ~0x00fffffff0000000ULL) |
                                   (static_cast<uint64_t>(id) << 28));
    }

    constexpr uint32_t link_id() const noexcept {
        assert(is_container());
        return static_cast<uint32_t>((static_cast<uint64_t>(raw) >> 30) & kMaxTokenId);
    }

    constexpr void set_link_id(uint32_t id) noexcept {
        assert(is_container());
        assert(id <= kMaxTokenId);
        raw = static_cast<int64_t>((static_cast<uint64_t>(raw) & ~0x0fffffffc0000000ULL) |
                                   (static_cast<uint64_t>(id) << 30));
    }

    constexpr uint32_t empty_id() const noexcept {
        assert(is_empty());
        return static_cast<uint32_t>((static_cast<uint64_t>(raw) >> 28) & kMaxTokenId);
    }

    constexpr uint32_t jump_id() const noexcept {
        assert(kind() == DTokenKind::Control && !is_extended());
        return static_cast<uint32_t>(raw & kMaxTokenId);
    }

    constexpr uint32_t count() const noexcept {
        assert(is_container() && !is_extended());
        return static_cast<uint32_t>(raw & kMaxTokenId);
    }

    constexpr void set_count(uint32_t c) noexcept {
        assert(is_container() && !is_extended());
        assert(c <= kMaxValueCount);
        raw = static_cast<int64_t>((static_cast<uint64_t>(raw) & 0x7fffffffc0000000ULL) | c);
    }

    constexpr void increment_count() noexcept {
        assert(is_container() && !is_extended());
        ++raw;
    }

    // --- Payload codecs ---
    static constexpr int32_t encode_string_param(int32_t length, bool escaped) noexcept {
        if (length > 0x7fffff) {
            length >>= 8;
            length |= 0x800000;
        }
        if (escaped) {
            length |= 0x1000000;
        }
        return length;
    }

    static constexpr std::pair<bool, int32_t> decode_string_param(int32_t param) noexcept {
        return {(param & 0x1000000) != 0, param & 0xffffff};
    }

    static constexpr int64_t encode_length_offset(uint32_t length, uint32_t offset) noexcept {
        assert(length <= 0x1ffffffu);
        assert(offset <= 0x7fffffffu);
        return static_cast<int64_t>(offset) | (static_cast<int64_t>(length) << 31);
    }

    static constexpr std::pair<uint32_t, uint32_t> decode_length_offset(DToken token) noexcept {
        return {
            static_cast<uint32_t>((static_cast<uint64_t>(token.raw) >> 31) & 0x1ffffffULL),
            static_cast<uint32_t>(static_cast<uint64_t>(token.raw) & 0x7fffffffULL)
        };
    }

    static int64_t encode_inline_float(double value) noexcept {
        uint64_t bits = std::bit_cast<uint64_t>(value);
        uint64_t sign = (bits >> 63) & 1ULL;
        uint64_t exponent = (bits >> 52) & 0x7ffULL;
        uint64_t mantissa = bits & 0x000fffffffffffffULL;

        if ((bits & 0x7fffffffffffffffULL) == 0ULL) {
            return static_cast<int64_t>(sign << 52);
        }

        if (exponent >= 992 && exponent <= 1055) {
            uint64_t exp_idx = exponent - 992;
            if (exp_idx == 0 && mantissa == 0) {
                return -1;
            }
            return static_cast<int64_t>((exp_idx << 53) | (sign << 52) | mantissa);
        }

        return -1;
    }

    static double decode_inline_float(DToken token) noexcept {
        assert(token.kind() == DTokenKind::InlineFloat);
        uint64_t raw_u = static_cast<uint64_t>(token.raw);
        uint64_t exp_idx = (raw_u >> 53) & 0x3fULL;
        uint64_t sign = (raw_u >> 52) & 1ULL;
        uint64_t mantissa = raw_u & 0x000fffffffffffffULL;

        if (exp_idx == 0 && mantissa == 0) {
            return std::bit_cast<double>(sign << 63);
        }

        uint64_t exponent = exp_idx + 992;
        uint64_t bits = (sign << 63) | (exponent << 52) | mantissa;
        return std::bit_cast<double>(bits);
    }

    static constexpr int64_t decode_inline_integer(DToken token) noexcept {
        assert(token.kind() == DTokenKind::Integer && token.is_inline_payload());
        // Sign-extend 53-bit signed integer
        uint64_t u = static_cast<uint64_t>(token.raw) & 0x1fffffffffffffULL;
        if (u & (1ULL << 52)) {
            u |= ~0x1fffffffffffffULL;
        }
        return static_cast<int64_t>(u);
    }

    static float decode_inline_single(DToken token) noexcept {
        assert(token.kind() == DTokenKind::Float && token.is_inline_payload());
        return std::bit_cast<float>(static_cast<uint32_t>(token.raw));
    }

    std::string to_debug_string() const;
};

static_assert(sizeof(DToken) == 8, "DToken must be exactly 64 bits");

} // namespace redox

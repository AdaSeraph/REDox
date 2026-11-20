// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#include "redox/json.hpp"
#include "utf8_helper.hpp"

#include <limits>
#include <stdexcept>

namespace redox {

namespace {

class Json5Parser {
public:
    Json5Parser(std::string_view src, std::vector<DToken>& tokens, const Json5DocumentOptions& opts)
        : src_(src), tokens_(tokens), opts_(opts) {}

    uint32_t parse() {
        skip_ws_and_trivia(opts_.preserve_trivia);
        if (pos_ >= src_.size()) {
            fail("Empty JSON5 input");
        }
        uint32_t root = parse_value(0);
        skip_ws_and_trivia(false);
        if (pos_ < src_.size()) {
            fail("Unexpected trailing characters after JSON5 value");
        }
        return root;
    }

private:
    [[noreturn]] void fail(const std::string& msg) const {
        size_t line = 1, col = 1;
        for (size_t i = 0; i < pos_ && i < src_.size(); ++i) {
            if (src_[i] == '\n') {
                ++line;
                col = 1;
            } else {
                ++col;
            }
        }
        throw DocumentParseException(msg, pos_, line, col);
    }

    uint32_t emit(DToken tok) {
        uint32_t id = static_cast<uint32_t>(tokens_.size());
        tokens_.push_back(tok);
        return id;
    }

    static bool is_ws(unsigned char c) noexcept {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
    }

    static bool is_ident_start(unsigned char c) noexcept {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$' || c >= 0x80;
    }

    static bool is_ident_part(unsigned char c) noexcept {
        return is_ident_start(c) || (c >= '0' && c <= '9');
    }

    void skip_ws_and_trivia(bool emit_comments) {
        while (pos_ < src_.size()) {
            unsigned char c = static_cast<unsigned char>(src_[pos_]);
            if (is_ws(c)) {
                ++pos_;
            } else if (c == 0xC2 && pos_ + 1 < src_.size() && static_cast<unsigned char>(src_[pos_ + 1]) == 0xA0) {
                pos_ += 2; // NBSP
            } else if (c == 0xEF && pos_ + 2 < src_.size() &&
                       static_cast<unsigned char>(src_[pos_ + 1]) == 0xBB &&
                       static_cast<unsigned char>(src_[pos_ + 2]) == 0xBF) {
                pos_ += 3; // UTF-8 BOM
            } else if (c == 0xE2 && pos_ + 2 < src_.size() &&
                       static_cast<unsigned char>(src_[pos_ + 1]) == 0x80 &&
                       (static_cast<unsigned char>(src_[pos_ + 2]) == 0xA8 ||
                        static_cast<unsigned char>(src_[pos_ + 2]) == 0xA9)) {
                pos_ += 3; // LS / PS
            } else if (c == '/' && pos_ + 1 < src_.size()) {
                if (src_[pos_ + 1] == '/') {
                    size_t start = pos_;
                    pos_ += 2;
                    while (pos_ < src_.size() && src_[pos_] != '\n' && src_[pos_] != '\r') {
                        ++pos_;
                    }
                    if (emit_comments) {
                        emit(DToken::make(DTokenVariant::TriviaLineComment,
                                          DToken::encode_length_offset(static_cast<uint32_t>(pos_ - start),
                                                                       static_cast<uint32_t>(start))));
                    }
                } else if (src_[pos_ + 1] == '*') {
                    size_t start = pos_;
                    pos_ += 2;
                    bool closed = false;
                    while (pos_ + 1 < src_.size()) {
                        if (src_[pos_] == '*' && src_[pos_ + 1] == '/') {
                            pos_ += 2;
                            closed = true;
                            break;
                        }
                        ++pos_;
                    }
                    if (!closed) fail("Unterminated JSON5 block comment");
                    if (emit_comments) {
                        emit(DToken::make(DTokenVariant::TriviaBlockComment,
                                          DToken::encode_length_offset(static_cast<uint32_t>(pos_ - start),
                                                                       static_cast<uint32_t>(start))));
                    }
                } else {
                    break;
                }
            } else {
                break;
            }
        }
    }

    uint32_t parse_value(int depth) {
        if (depth > opts_.max_depth) fail("Maximum JSON5 nesting depth exceeded");
        if (pos_ >= src_.size()) fail("Unexpected end of JSON5 input");
        char c = src_[pos_];
        switch (c) {
            case '{': return parse_object(depth + 1);
            case '[': return parse_array(depth + 1);
            case '"':
            case '\'': return parse_quoted_string(c);
            case 't': return parse_keyword("true", DTokenVariant::BooleanTrue);
            case 'f': return parse_keyword("false", DTokenVariant::BooleanFalse);
            case 'n': return parse_keyword("null", DTokenVariant::Null);
            case 'I':
            case 'N':
            case '+':
            case '-':
            case '.':
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                return parse_number();
            default:
                fail(std::string("Unexpected character in JSON5: '") + c + "'");
        }
    }

    uint32_t parse_object(int depth) {
        ++pos_; // '{'
        uint32_t container_id = emit(DToken::make_map(0));
        uint32_t count = 0;

        while (true) {
            skip_ws_and_trivia(opts_.preserve_trivia);
            if (pos_ >= src_.size()) fail("Unterminated JSON5 object");
            if (src_[pos_] == '}') {
                ++pos_;
                break;
            }

            // Key: quoted string or unquoted identifier
            if (src_[pos_] == '"' || src_[pos_] == '\'') {
                parse_quoted_string(src_[pos_]);
            } else if (is_ident_start(static_cast<unsigned char>(src_[pos_]))) {
                parse_identifier_key();
            } else {
                fail("Expected property key in JSON5 object");
            }

            skip_ws_and_trivia(false);
            if (pos_ >= src_.size() || src_[pos_] != ':') {
                fail("Expected ':' after JSON5 object key");
            }
            ++pos_;

            skip_ws_and_trivia(opts_.preserve_trivia);
            parse_value(depth);
            ++count;

            skip_ws_and_trivia(false);
            if (pos_ >= src_.size()) fail("Unterminated JSON5 object");
            if (src_[pos_] == ',') {
                ++pos_;
            } else if (src_[pos_] == '}') {
                ++pos_;
                break;
            } else {
                fail("Expected ',' or '}' in JSON5 object");
            }
        }

        tokens_[container_id].set_count(count);
        tokens_[container_id].set_link_id(static_cast<uint32_t>(tokens_.size()));
        return container_id;
    }

    uint32_t parse_array(int depth) {
        ++pos_; // '['
        uint32_t container_id = emit(DToken::make_array(0));
        uint32_t count = 0;

        while (true) {
            skip_ws_and_trivia(opts_.preserve_trivia);
            if (pos_ >= src_.size()) fail("Unterminated JSON5 array");
            if (src_[pos_] == ']') {
                ++pos_;
                break;
            }

            parse_value(depth);
            ++count;

            skip_ws_and_trivia(false);
            if (pos_ >= src_.size()) fail("Unterminated JSON5 array");
            if (src_[pos_] == ',') {
                ++pos_;
            } else if (src_[pos_] == ']') {
                ++pos_;
                break;
            } else {
                fail("Expected ',' or ']' in JSON5 array");
            }
        }

        tokens_[container_id].set_count(count);
        tokens_[container_id].set_link_id(static_cast<uint32_t>(tokens_.size()));
        return container_id;
    }

    uint32_t parse_identifier_key() {
        size_t start = pos_;
        ++pos_;
        while (pos_ < src_.size() && is_ident_part(static_cast<unsigned char>(src_[pos_]))) {
            ++pos_;
        }
        size_t len = pos_ - start;
        int32_t param = DToken::encode_string_param(static_cast<int32_t>(len), false);
        return emit(DToken::make(DTokenVariant::SymbolIdentifier,
                                 DToken::encode_length_offset(static_cast<uint32_t>(param),
                                                              static_cast<uint32_t>(start))));
    }

    uint32_t parse_quoted_string(char quote) {
        ++pos_; // opening quote
        size_t start = pos_;
        bool escaped = false;
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (c == quote) {
                size_t len = pos_ - start;
                ++pos_;
                if (escaped) {
                    (void)utf8::unescape_json_string(src_.substr(start, len), true);
                }
                DTokenVariant v = (quote == '\'') ? DTokenVariant::StringSingleQuote : DTokenVariant::StringDoubleQuote;
                int32_t param = DToken::encode_string_param(static_cast<int32_t>(len), escaped);
                return emit(DToken::make(v, DToken::encode_length_offset(static_cast<uint32_t>(param),
                                                                         static_cast<uint32_t>(start))));
            }
            if (c == '\n' || c == '\r') {
                fail("Unescaped newline in JSON5 string");
            }
            if (c == '\\') {
                escaped = true;
                ++pos_;
                if (pos_ >= src_.size()) fail("Unterminated escape in JSON5 string");
                if (src_[pos_] == '\r' && pos_ + 1 < src_.size() && src_[pos_ + 1] == '\n') {
                    pos_ += 2;
                } else {
                    ++pos_;
                }
            } else {
                ++pos_;
            }
        }
        fail("Unterminated JSON5 string");
    }

    uint32_t parse_keyword(std::string_view kw, DTokenVariant variant) {
        if (src_.substr(pos_, kw.size()) == kw) {
            pos_ += kw.size();
            return emit(DToken::make(variant));
        }
        fail("Unexpected identifier in JSON5");
    }

    uint32_t parse_number() {
        size_t start = pos_;
        if (src_[pos_] == '+' || src_[pos_] == '-') {
            ++pos_;
        }
        if (pos_ >= src_.size()) fail("Invalid JSON5 number");

        if (src_.substr(pos_, 8) == "Infinity") {
            pos_ += 8;
            return emit(DToken::make(DTokenVariant::Float,
                                     DToken::encode_length_offset(static_cast<uint32_t>(pos_ - start),
                                                                  static_cast<uint32_t>(start))));
        }
        if (src_.substr(pos_, 3) == "NaN") {
            pos_ += 3;
            return emit(DToken::make(DTokenVariant::Float,
                                     DToken::encode_length_offset(static_cast<uint32_t>(pos_ - start),
                                                                  static_cast<uint32_t>(start))));
        }

        // Hexadecimal integer: 0x...
        if (src_[pos_] == '0' && pos_ + 1 < src_.size() && (src_[pos_ + 1] == 'x' || src_[pos_ + 1] == 'X')) {
            pos_ += 2;
            size_t hex_start = pos_;
            while (pos_ < src_.size()) {
                char h = src_[pos_];
                if ((h >= '0' && h <= '9') || (h >= 'a' && h <= 'f') || (h >= 'A' && h <= 'F')) {
                    ++pos_;
                } else {
                    break;
                }
            }
            if (pos_ == hex_start) fail("Expected hex digits after 0x");
            return emit(DToken::make(DTokenVariant::IntegerHexadecimal,
                                     DToken::encode_length_offset(static_cast<uint32_t>(pos_ - start),
                                                                  static_cast<uint32_t>(start))));
        }

        bool is_float = false;
        bool has_digits = false;

        while (pos_ < src_.size() && src_[pos_] >= '0' && src_[pos_] <= '9') {
            has_digits = true;
            ++pos_;
        }
        if (pos_ < src_.size() && src_[pos_] == '.') {
            is_float = true;
            ++pos_;
            while (pos_ < src_.size() && src_[pos_] >= '0' && src_[pos_] <= '9') {
                has_digits = true;
                ++pos_;
            }
        }
        if (!has_digits) fail("Invalid JSON5 number");

        if (pos_ < src_.size() && (src_[pos_] == 'e' || src_[pos_] == 'E')) {
            is_float = true;
            ++pos_;
            if (pos_ < src_.size() && (src_[pos_] == '+' || src_[pos_] == '-')) ++pos_;
            if (pos_ >= src_.size() || src_[pos_] < '0' || src_[pos_] > '9') {
                fail("Invalid exponent in JSON5 number");
            }
            while (pos_ < src_.size() && src_[pos_] >= '0' && src_[pos_] <= '9') ++pos_;
        }

        size_t len = pos_ - start;
        DTokenVariant v = is_float ? DTokenVariant::Float : DTokenVariant::Integer;
        if (!is_float) {
            int64_t iv = 0;
            if (!utf8::parse_json5_integer(src_.substr(start, len), iv)) {
                v = DTokenVariant::Float;
            }
        }
        return emit(DToken::make(v, DToken::encode_length_offset(static_cast<uint32_t>(len),
                                                                 static_cast<uint32_t>(start))));
    }

    std::string_view src_;
    std::vector<DToken>& tokens_;
    const Json5DocumentOptions& opts_;
    size_t pos_ = 0;
};

} // namespace

std::unique_ptr<Json5Document> Json5Document::parse(std::string_view utf8_json5, const Json5DocumentOptions& options) {
    if (!utf8::is_valid_utf8(utf8_json5)) {
        throw DocumentParseException("Invalid UTF-8 sequence in JSON5 input");
    }
    auto doc = std::unique_ptr<Json5Document>(new Json5Document());
    doc->source_.assign(utf8_json5);
    doc->parse_impl(options);
    return doc;
}

bool Json5Document::try_parse(std::string_view utf8_json5, std::unique_ptr<Json5Document>& out, const Json5DocumentOptions& options) noexcept {
    try {
        out = parse(utf8_json5, options);
        return true;
    } catch (...) {
        out.reset();
        return false;
    }
}

void Json5Document::parse_impl(const Json5DocumentOptions& options) {
    reset();
    tokens_.reserve(std::max<size_t>(16, source_.size() / 4));
    Json5Parser parser(source_, tokens_, options);
    uint32_t root = parser.parse();
    set_root_id(root);
}

std::string Json5Document::decode_string(DToken token) const {
    auto [param, offset] = DToken::decode_length_offset(token);
    auto [escaped, enc_len] = DToken::decode_string_param(static_cast<int32_t>(param));
    std::string_view slice(source_.data() + offset, static_cast<size_t>(enc_len));
    if (!escaped) return std::string(slice);
    return utf8::unescape_json_string(slice, true);
}

std::string_view Json5Document::decode_raw_utf8(DToken token, bool& ok) const {
    if (token.kind() != DTokenKind::String && token.kind() != DTokenKind::Symbol) {
        ok = false;
        return {};
    }
    auto [param, offset] = DToken::decode_length_offset(token);
    auto [escaped, enc_len] = DToken::decode_string_param(static_cast<int32_t>(param));
    if (escaped || (enc_len & 0x800000)) {
        ok = false;
        return {};
    }
    ok = true;
    return std::string_view(source_.data() + offset, static_cast<size_t>(enc_len));
}

int64_t Json5Document::decode_integer(DToken token) const {
    auto [len, offset] = DToken::decode_length_offset(token);
    int64_t v = 0;
    if (!utf8::parse_json5_integer(std::string_view(source_.data() + offset, len), v)) {
        throw FormatException("Failed to decode JSON5 integer");
    }
    return v;
}

uint64_t Json5Document::decode_uinteger(DToken token) const {
    return static_cast<uint64_t>(decode_integer(token));
}

double Json5Document::decode_float(DToken token) const {
    auto [len, offset] = DToken::decode_length_offset(token);
    double v = 0.0;
    if (!utf8::parse_json5_double(std::string_view(source_.data() + offset, len), v)) {
        throw FormatException("Failed to decode JSON5 float");
    }
    return v;
}

std::string Json5Document::decode_trivia(DToken token) const {
    auto [len, offset] = DToken::decode_length_offset(token);
    return std::string(source_.data() + offset, len);
}

} // namespace redox

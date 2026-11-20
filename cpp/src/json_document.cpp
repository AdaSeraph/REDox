// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#include "redox/json.hpp"
#include "utf8_helper.hpp"

#include <limits>
#include <stdexcept>

namespace redox {

namespace {

class JsonParser {
public:
    JsonParser(std::string_view src, std::vector<DToken>& tokens, const JsonDocumentOptions& opts)
        : src_(src), tokens_(tokens), opts_(opts) {}

    uint32_t parse() {
        // Strip UTF-8 BOM if present? RFC 8259 forbids BOM, so do not strip by default.
        skip_ws();
        if (pos_ >= src_.size()) {
            fail("Empty JSON input");
        }
        uint32_t root = parse_value(0);
        skip_ws();
        if (pos_ < src_.size()) {
            fail("Unexpected trailing characters after JSON value");
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

    void skip_ws() {
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                ++pos_;
            } else if (opts_.comment_handling == JsonCommentHandling::Skip && c == '/' && pos_ + 1 < src_.size()) {
                if (src_[pos_ + 1] == '/') {
                    pos_ += 2;
                    while (pos_ < src_.size() && src_[pos_] != '\n') ++pos_;
                } else if (src_[pos_ + 1] == '*') {
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
                    if (!closed) fail("Unterminated block comment");
                } else {
                    break;
                }
            } else {
                break;
            }
        }
    }

    uint32_t emit(DToken tok) {
        uint32_t id = static_cast<uint32_t>(tokens_.size());
        tokens_.push_back(tok);
        return id;
    }

    uint32_t parse_value(int depth) {
        if (depth > opts_.max_depth) {
            fail("Maximum JSON nesting depth exceeded");
        }
        if (pos_ >= src_.size()) {
            fail("Unexpected end of JSON input");
        }
        char c = src_[pos_];
        switch (c) {
            case '{': return parse_object(depth + 1);
            case '[': return parse_array(depth + 1);
            case '"': return parse_string();
            case 't': return parse_true();
            case 'f': return parse_false();
            case 'n': return parse_null();
            case '-':
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                return parse_number();
            default:
                fail(std::string("Unexpected character: '") + c + "'");
        }
    }

    uint32_t parse_object(int depth) {
        ++pos_; // '{'
        uint32_t container_id = emit(DToken::make_map(0));
        uint32_t count = 0;

        skip_ws();
        if (pos_ < src_.size() && src_[pos_] == '}') {
            ++pos_;
            tokens_[container_id].set_count(0);
            tokens_[container_id].set_link_id(static_cast<uint32_t>(tokens_.size()));
            return container_id;
        }

        while (true) {
            skip_ws();
            if (pos_ >= src_.size()) fail("Unterminated JSON object");
            if (src_[pos_] != '"') fail("Expected string key in JSON object");
            parse_string();

            skip_ws();
            if (pos_ >= src_.size() || src_[pos_] != ':') fail("Expected ':' after object key");
            ++pos_;

            skip_ws();
            parse_value(depth);
            ++count;

            skip_ws();
            if (pos_ >= src_.size()) fail("Unterminated JSON object");
            if (src_[pos_] == ',') {
                ++pos_;
                skip_ws();
                if (opts_.allow_trailing_commas && pos_ < src_.size() && src_[pos_] == '}') {
                    ++pos_;
                    break;
                }
            } else if (src_[pos_] == '}') {
                ++pos_;
                break;
            } else {
                fail("Expected ',' or '}' in JSON object");
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

        skip_ws();
        if (pos_ < src_.size() && src_[pos_] == ']') {
            ++pos_;
            tokens_[container_id].set_count(0);
            tokens_[container_id].set_link_id(static_cast<uint32_t>(tokens_.size()));
            return container_id;
        }

        while (true) {
            skip_ws();
            parse_value(depth);
            ++count;

            skip_ws();
            if (pos_ >= src_.size()) fail("Unterminated JSON array");
            if (src_[pos_] == ',') {
                ++pos_;
                skip_ws();
                if (opts_.allow_trailing_commas && pos_ < src_.size() && src_[pos_] == ']') {
                    ++pos_;
                    break;
                }
            } else if (src_[pos_] == ']') {
                ++pos_;
                break;
            } else {
                fail("Expected ',' or ']' in JSON array");
            }
        }

        tokens_[container_id].set_count(count);
        tokens_[container_id].set_link_id(static_cast<uint32_t>(tokens_.size()));
        return container_id;
    }

    uint32_t parse_string() {
        ++pos_; // '"'
        size_t start = pos_;
        bool escaped = false;
        while (pos_ < src_.size()) {
            unsigned char c = static_cast<unsigned char>(src_[pos_]);
            if (c == '"') {
                size_t len = pos_ - start;
                ++pos_;
                if (escaped) {
                    // Validate escapes eagerly for RFC 8259 conformance
                    (void)utf8::unescape_json_string(src_.substr(start, len), false);
                }
                int32_t param = DToken::encode_string_param(static_cast<int32_t>(len), escaped);
                return emit(DToken::make(DTokenVariant::StringDoubleQuote,
                                         DToken::encode_length_offset(static_cast<uint32_t>(param),
                                                                      static_cast<uint32_t>(start))));
            }
            if (c < 0x20) {
                fail("Unescaped control character in JSON string");
            }
            if (c == '\\') {
                escaped = true;
                ++pos_;
                if (pos_ >= src_.size()) fail("Unterminated escape in JSON string");
                char e = src_[pos_++];
                switch (e) {
                    case '"': case '\\': case '/': case 'b':
                    case 'f': case 'n': case 'r': case 't':
                        break;
                    case 'u':
                        if (pos_ + 4 > src_.size()) fail("Truncated \\uXXXX escape");
                        pos_ += 4;
                        break;
                    default:
                        fail("Invalid escape character in JSON string");
                }
            } else {
                ++pos_;
            }
        }
        fail("Unterminated JSON string");
    }

    uint32_t parse_true() {
        if (src_.substr(pos_, 4) == "true") {
            pos_ += 4;
            return emit(DToken::make(DTokenVariant::BooleanTrue));
        }
        fail("Invalid literal (expected 'true')");
    }

    uint32_t parse_false() {
        if (src_.substr(pos_, 5) == "false") {
            pos_ += 5;
            return emit(DToken::make(DTokenVariant::BooleanFalse));
        }
        fail("Invalid literal (expected 'false')");
    }

    uint32_t parse_null() {
        if (src_.substr(pos_, 4) == "null") {
            pos_ += 4;
            return emit(DToken::make(DTokenVariant::Null));
        }
        fail("Invalid literal (expected 'null')");
    }

    uint32_t parse_number() {
        size_t start = pos_;
        bool is_float = false;

        if (src_[pos_] == '-') {
            ++pos_;
            if (pos_ >= src_.size()) fail("Invalid number after '-'");
        }

        if (src_[pos_] == '0') {
            ++pos_;
            if (pos_ < src_.size() && src_[pos_] >= '0' && src_[pos_] <= '9') {
                fail("Leading zeros are not allowed in JSON numbers");
            }
        } else if (src_[pos_] >= '1' && src_[pos_] <= '9') {
            ++pos_;
            while (pos_ < src_.size() && src_[pos_] >= '0' && src_[pos_] <= '9') ++pos_;
        } else {
            fail("Expected digit in JSON number");
        }

        if (pos_ < src_.size() && src_[pos_] == '.') {
            is_float = true;
            ++pos_;
            if (pos_ >= src_.size() || src_[pos_] < '0' || src_[pos_] > '9') {
                fail("Expected digit after decimal point");
            }
            while (pos_ < src_.size() && src_[pos_] >= '0' && src_[pos_] <= '9') ++pos_;
        }

        if (pos_ < src_.size() && (src_[pos_] == 'e' || src_[pos_] == 'E')) {
            is_float = true;
            ++pos_;
            if (pos_ < src_.size() && (src_[pos_] == '+' || src_[pos_] == '-')) ++pos_;
            if (pos_ >= src_.size() || src_[pos_] < '0' || src_[pos_] > '9') {
                fail("Expected digit in exponent");
            }
            while (pos_ < src_.size() && src_[pos_] >= '0' && src_[pos_] <= '9') ++pos_;
        }

        size_t len = pos_ - start;
        if (!is_float) {
            std::string_view slice = src_.substr(start, len);
            int64_t iv = 0;
            uint64_t uv = 0;
            if (utf8::parse_int64(slice, iv)) {
                return emit(DToken::make(DTokenVariant::Integer,
                                         DToken::encode_length_offset(static_cast<uint32_t>(len),
                                                                      static_cast<uint32_t>(start))));
            }
            if (utf8::parse_uint64(slice, uv)) {
                return emit(DToken::make(DTokenVariant::IntegerUnsigned,
                                         DToken::encode_length_offset(static_cast<uint32_t>(len),
                                                                      static_cast<uint32_t>(start))));
            }
            // Falls back to float for huge integers
            is_float = true;
        }

        return emit(DToken::make(DTokenVariant::Float,
                                 DToken::encode_length_offset(static_cast<uint32_t>(len),
                                                              static_cast<uint32_t>(start))));
    }

    std::string_view src_;
    std::vector<DToken>& tokens_;
    const JsonDocumentOptions& opts_;
    size_t pos_ = 0;
};

} // namespace

std::unique_ptr<JsonDocument> JsonDocument::parse(std::string_view utf8_json, const JsonDocumentOptions& options) {
    if (!utf8::is_valid_utf8(utf8_json)) {
        throw DocumentParseException("Invalid UTF-8 sequence in JSON input");
    }
    auto doc = std::unique_ptr<JsonDocument>(new JsonDocument());
    doc->source_.assign(utf8_json);
    doc->parse_impl(options);
    return doc;
}

std::unique_ptr<JsonDocument> JsonDocument::parse(std::span<const uint8_t> utf8_bytes, const JsonDocumentOptions& options) {
    return parse(std::string_view(reinterpret_cast<const char*>(utf8_bytes.data()), utf8_bytes.size()), options);
}

bool JsonDocument::try_parse(std::string_view utf8_json, std::unique_ptr<JsonDocument>& out, const JsonDocumentOptions& options) noexcept {
    try {
        out = parse(utf8_json, options);
        return true;
    } catch (...) {
        out.reset();
        return false;
    }
}

std::vector<std::unique_ptr<JsonDocument>> JsonDocument::parse_ndjson(std::string_view utf8_ndjson, const JsonDocumentOptions& options) {
    std::vector<std::unique_ptr<JsonDocument>> docs;
    size_t pos = 0;
    while (pos < utf8_ndjson.size()) {
        size_t nl = utf8_ndjson.find('\n', pos);
        std::string_view line = (nl == std::string_view::npos)
                                    ? utf8_ndjson.substr(pos)
                                    : utf8_ndjson.substr(pos, nl - pos);
        pos = (nl == std::string_view::npos) ? utf8_ndjson.size() : nl + 1;
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t' || line.front() == '\r')) {
            line.remove_prefix(1);
        }
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r')) {
            line.remove_suffix(1);
        }
        if (line.empty()) continue;
        docs.push_back(parse(line, options));
    }
    return docs;
}

void JsonDocument::parse_impl(const JsonDocumentOptions& options) {
    reset();
    tokens_.reserve(std::max<size_t>(16, source_.size() / 4));
    JsonParser parser(source_, tokens_, options);
    uint32_t root = parser.parse();
    set_root_id(root);
}

std::string JsonDocument::decode_string(DToken token) const {
    auto [param, offset] = DToken::decode_length_offset(token);
    auto [escaped, enc_len] = DToken::decode_string_param(static_cast<int32_t>(param));
    size_t len = static_cast<size_t>(enc_len);
    if (enc_len & 0x800000) {
        // Long string: scan to closing quote
        size_t p = offset;
        while (p < source_.size()) {
            if (source_[p] == '\\') {
                p += 2;
            } else if (source_[p] == '"') {
                break;
            } else {
                ++p;
            }
        }
        len = p - offset;
    }
    std::string_view slice(source_.data() + offset, len);
    if (!escaped) {
        return std::string(slice);
    }
    return utf8::unescape_json_string(slice, false);
}

std::string_view JsonDocument::decode_raw_utf8(DToken token, bool& ok) const {
    if (token.kind() != DTokenKind::String) {
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

int64_t JsonDocument::decode_integer(DToken token) const {
    auto [len, offset] = DToken::decode_length_offset(token);
    int64_t v = 0;
    if (!utf8::parse_int64(std::string_view(source_.data() + offset, len), v)) {
        throw FormatException("Failed to decode JSON integer");
    }
    return v;
}

uint64_t JsonDocument::decode_uinteger(DToken token) const {
    auto [len, offset] = DToken::decode_length_offset(token);
    uint64_t v = 0;
    if (!utf8::parse_uint64(std::string_view(source_.data() + offset, len), v)) {
        throw FormatException("Failed to decode JSON unsigned integer");
    }
    return v;
}

double JsonDocument::decode_float(DToken token) const {
    auto [len, offset] = DToken::decode_length_offset(token);
    double v = 0.0;
    if (!utf8::parse_double(std::string_view(source_.data() + offset, len), v)) {
        throw FormatException("Failed to decode JSON float");
    }
    return v;
}

} // namespace redox

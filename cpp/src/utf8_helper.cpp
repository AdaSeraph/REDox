// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#include "utf8_helper.hpp"
#include "redox/document.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace redox::utf8 {

bool is_valid_utf8(std::string_view text) noexcept {
    const auto* s = reinterpret_cast<const uint8_t*>(text.data());
    size_t i = 0;
    const size_t n = text.size();
    while (i < n) {
        uint8_t c = s[i];
        if (c <= 0x7F) {
            ++i;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= n || (s[i + 1] & 0xC0) != 0x80) return false;
            uint32_t cp = ((c & 0x1F) << 6) | (s[i + 1] & 0x3F);
            if (cp < 0x80) return false;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= n || (s[i + 1] & 0xC0) != 0x80 || (s[i + 2] & 0xC0) != 0x80) return false;
            uint32_t cp = ((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F);
            if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= n || (s[i + 1] & 0xC0) != 0x80 || (s[i + 2] & 0xC0) != 0x80 || (s[i + 3] & 0xC0) != 0x80) {
                return false;
            }
            uint32_t cp = ((c & 0x07) << 18) | ((s[i + 1] & 0x3F) << 12) |
                          ((s[i + 2] & 0x3F) << 6) | (s[i + 3] & 0x3F);
            if (cp < 0x10000 || cp > 0x10FFFF) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

bool append_codepoint(std::string& out, uint32_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
        return true;
    }
    if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        return true;
    }
    if (cp <= 0xFFFF) {
        if (cp >= 0xD800 && cp <= 0xDFFF) return false;
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        return true;
    }
    if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        return true;
    }
    return false;
}

size_t count_utf16_code_units(std::string_view utf8) noexcept {
    size_t units = 0;
    const auto* s = reinterpret_cast<const uint8_t*>(utf8.data());
    size_t i = 0;
    const size_t n = utf8.size();
    while (i < n) {
        uint8_t c = s[i];
        if (c <= 0x7F) {
            units += 1;
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            units += 1;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            units += 1;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            units += 2; // surrogate pair
            i += 4;
        } else {
            units += 1;
            i += 1;
        }
    }
    return units;
}

namespace {

int hex_digit(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool parse_hex4(std::string_view s, size_t pos, uint32_t& out) noexcept {
    if (pos + 4 > s.size()) return false;
    uint32_t v = 0;
    for (size_t k = 0; k < 4; ++k) {
        int d = hex_digit(s[pos + k]);
        if (d < 0) return false;
        v = (v << 4) | static_cast<uint32_t>(d);
    }
    out = v;
    return true;
}

} // namespace

std::string unescape_json_string(std::string_view raw, bool allow_json5_escapes) {
    std::string out;
    out.reserve(raw.size());
    size_t i = 0;
    const size_t n = raw.size();
    while (i < n) {
        char c = raw[i++];
        if (c != '\\') {
            out.push_back(c);
            continue;
        }
        if (i >= n) {
            throw DocumentParseException("Unterminated escape sequence in string");
        }
        char esc = raw[i++];
        switch (esc) {
            case '"':  out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/':  out.push_back('/'); break;
            case 'b':  out.push_back('\b'); break;
            case 'f':  out.push_back('\f'); break;
            case 'n':  out.push_back('\n'); break;
            case 'r':  out.push_back('\r'); break;
            case 't':  out.push_back('\t'); break;
            case 'u': {
                uint32_t cp = 0;
                if (!parse_hex4(raw, i, cp)) {
                    throw DocumentParseException("Invalid \\uXXXX escape sequence");
                }
                i += 4;
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    if (i + 6 <= n && raw[i] == '\\' && raw[i + 1] == 'u') {
                        uint32_t low = 0;
                        if (parse_hex4(raw, i + 2, low) && low >= 0xDC00 && low <= 0xDFFF) {
                            i += 6;
                            cp = 0x10000 + (((cp - 0xD800) << 10) | (low - 0xDC00));
                        } else {
                            throw DocumentParseException("Invalid low surrogate in \\uXXXX escape");
                        }
                    } else {
                        throw DocumentParseException("Unpaired high surrogate in \\uXXXX escape");
                    }
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    throw DocumentParseException("Unpaired low surrogate in \\uXXXX escape");
                }
                append_codepoint(out, cp);
                break;
            }
            default:
                if (allow_json5_escapes) {
                    if (esc == '\'') { out.push_back('\''); break; }
                    if (esc == 'v')  { out.push_back('\v'); break; }
                    if (esc == '0')  { out.push_back('\0'); break; }
                    if (esc == '\n') { break; } // line continuation
                    if (esc == '\r') {
                        if (i < n && raw[i] == '\n') ++i;
                        break;
                    }
                    if (esc == 'x') {
                        if (i + 2 > n) throw DocumentParseException("Invalid \\xXX escape in JSON5");
                        int h1 = hex_digit(raw[i]);
                        int h2 = hex_digit(raw[i + 1]);
                        if (h1 < 0 || h2 < 0) throw DocumentParseException("Invalid \\xXX escape in JSON5");
                        i += 2;
                        append_codepoint(out, static_cast<uint32_t>((h1 << 4) | h2));
                        break;
                    }
                    // JSON5 allows escaping any non-digit character as itself
                    if (esc < '0' || esc > '9') {
                        out.push_back(esc);
                        break;
                    }
                }
                throw DocumentParseException("Invalid escape character in string");
        }
    }
    return out;
}

void write_escaped_json_string(std::string& out, std::string_view text, char quote_char, bool escape_non_ascii) {
    static constexpr char kHex[] = "0123456789abcdef";
    out.push_back(quote_char);
    const auto* s = reinterpret_cast<const uint8_t*>(text.data());
    const size_t n = text.size();
    for (size_t i = 0; i < n; ++i) {
        uint8_t c = s[i];
        if (c == static_cast<uint8_t>(quote_char)) {
            out.push_back('\\');
            out.push_back(quote_char);
        } else if (c == '\\') {
            out.append("\\\\");
        } else if (c == '\b') {
            out.append("\\b");
        } else if (c == '\f') {
            out.append("\\f");
        } else if (c == '\n') {
            out.append("\\n");
        } else if (c == '\r') {
            out.append("\\r");
        } else if (c == '\t') {
            out.append("\\t");
        } else if (c < 0x20) {
            out.append("\\u00");
            out.push_back(kHex[(c >> 4) & 0xF]);
            out.push_back(kHex[c & 0xF]);
        } else if (escape_non_ascii && c >= 0x80) {
            // Decode UTF-8 codepoint and emit \uXXXX (or surrogate pair)
            uint32_t cp = 0;
            if ((c & 0xE0) == 0xC0 && i + 1 < n) {
                cp = ((c & 0x1F) << 6) | (s[i + 1] & 0x3F);
                i += 1;
            } else if ((c & 0xF0) == 0xE0 && i + 2 < n) {
                cp = ((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F);
                i += 2;
            } else if ((c & 0xF8) == 0xF0 && i + 3 < n) {
                cp = ((c & 0x07) << 18) | ((s[i + 1] & 0x3F) << 12) |
                     ((s[i + 2] & 0x3F) << 6) | (s[i + 3] & 0x3F);
                i += 3;
            }
            auto emit_u16 = [&](uint16_t u) {
                out.append("\\u");
                out.push_back(kHex[(u >> 12) & 0xF]);
                out.push_back(kHex[(u >> 8) & 0xF]);
                out.push_back(kHex[(u >> 4) & 0xF]);
                out.push_back(kHex[u & 0xF]);
            };
            if (cp <= 0xFFFF) {
                emit_u16(static_cast<uint16_t>(cp));
            } else {
                cp -= 0x10000;
                emit_u16(static_cast<uint16_t>(0xD800 + (cp >> 10)));
                emit_u16(static_cast<uint16_t>(0xDC00 + (cp & 0x3FF)));
            }
        } else {
            out.push_back(static_cast<char>(c));
        }
    }
    out.push_back(quote_char);
}

bool parse_int64(std::string_view text, int64_t& out) noexcept {
    if (text.empty()) return false;
    auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), out, 10);
    return ec == std::errc{} && ptr == text.data() + text.size();
}

bool parse_uint64(std::string_view text, uint64_t& out) noexcept {
    if (text.empty()) return false;
    auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), out, 10);
    return ec == std::errc{} && ptr == text.data() + text.size();
}

bool parse_double(std::string_view text, double& out) noexcept {
    if (text.empty()) return false;
    auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), out);
    if (ec == std::errc{} && ptr == text.data() + text.size()) {
        return true;
    }
    // Fallback for subnormals/very large exponents if needed
    std::string tmp(text);
    char* end = nullptr;
    out = std::strtod(tmp.c_str(), &end);
    return end == tmp.c_str() + tmp.size();
}

bool parse_json5_integer(std::string_view text, int64_t& out) noexcept {
    if (text.empty()) return false;
    bool neg = false;
    size_t pos = 0;
    if (text[0] == '+') {
        pos = 1;
    } else if (text[0] == '-') {
        neg = true;
        pos = 1;
    }
    std::string_view body = text.substr(pos);
    if (body.size() > 2 && body[0] == '0' && (body[1] == 'x' || body[1] == 'X')) {
        uint64_t u = 0;
        auto [ptr, ec] = std::from_chars(body.data() + 2, body.data() + body.size(), u, 16);
        if (ec != std::errc{} || ptr != body.data() + body.size()) return false;
        if (neg) {
            if (u > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1ULL) return false;
            out = (u == static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1ULL)
                      ? std::numeric_limits<int64_t>::min()
                      : -static_cast<int64_t>(u);
        } else {
            if (u > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) return false;
            out = static_cast<int64_t>(u);
        }
        return true;
    }
    if (text[0] == '+') {
        return parse_int64(body, out);
    }
    return parse_int64(text, out);
}

bool parse_json5_double(std::string_view text, double& out) noexcept {
    if (text.empty()) return false;
    if (text == "Infinity" || text == "+Infinity") {
        out = std::numeric_limits<double>::infinity();
        return true;
    }
    if (text == "-Infinity") {
        out = -std::numeric_limits<double>::infinity();
        return true;
    }
    if (text == "NaN" || text == "+NaN" || text == "-NaN") {
        out = std::numeric_limits<double>::quiet_NaN();
        return true;
    }
    int64_t iv = 0;
    if (parse_json5_integer(text, iv)) {
        out = static_cast<double>(iv);
        return true;
    }
    std::string_view body = text;
    if (!body.empty() && body[0] == '+') {
        body.remove_prefix(1);
    }
    return parse_double(body, out);
}

bool is_valid_json5_identifier(std::string_view text) noexcept {
    if (text.empty()) return false;
    auto is_start = [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$';
    };
    auto is_part = [&](unsigned char c) {
        return is_start(c) || (c >= '0' && c <= '9');
    };
    if (!is_start(static_cast<unsigned char>(text[0]))) return false;
    for (size_t i = 1; i < text.size(); ++i) {
        if (!is_part(static_cast<unsigned char>(text[i]))) return false;
    }
    return true;
}

std::string base64_encode(std::span<const uint8_t> data) {
    static constexpr char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    size_t i = 0;
    while (i + 3 <= data.size()) {
        uint32_t v = (static_cast<uint32_t>(data[i]) << 16) |
                     (static_cast<uint32_t>(data[i + 1]) << 8) |
                     static_cast<uint32_t>(data[i + 2]);
        out.push_back(kTable[(v >> 18) & 0x3F]);
        out.push_back(kTable[(v >> 12) & 0x3F]);
        out.push_back(kTable[(v >> 6) & 0x3F]);
        out.push_back(kTable[v & 0x3F]);
        i += 3;
    }
    size_t rem = data.size() - i;
    if (rem == 1) {
        uint32_t v = static_cast<uint32_t>(data[i]) << 16;
        out.push_back(kTable[(v >> 18) & 0x3F]);
        out.push_back(kTable[(v >> 12) & 0x3F]);
        out.push_back('=');
        out.push_back('=');
    } else if (rem == 2) {
        uint32_t v = (static_cast<uint32_t>(data[i]) << 16) |
                     (static_cast<uint32_t>(data[i + 1]) << 8);
        out.push_back(kTable[(v >> 18) & 0x3F]);
        out.push_back(kTable[(v >> 12) & 0x3F]);
        out.push_back(kTable[(v >> 6) & 0x3F]);
        out.push_back('=');
    }
    return out;
}

bool base64_decode(std::string_view text, std::vector<uint8_t>& out) {
    auto decode_char = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    out.clear();
    if (text.size() % 4 != 0) return false;
    out.reserve((text.size() / 4) * 3);
    for (size_t i = 0; i < text.size(); i += 4) {
        int a = decode_char(text[i]);
        int b = decode_char(text[i + 1]);
        if (a < 0 || b < 0) return false;
        int c = (text[i + 2] == '=') ? -2 : decode_char(text[i + 2]);
        int d = (text[i + 3] == '=') ? -2 : decode_char(text[i + 3]);
        if (c == -1 || d == -1) return false;
        out.push_back(static_cast<uint8_t>((a << 2) | (b >> 4)));
        if (c != -2) {
            out.push_back(static_cast<uint8_t>(((b & 0x0F) << 4) | (c >> 2)));
            if (d != -2) {
                out.push_back(static_cast<uint8_t>(((c & 0x03) << 6) | d));
            }
        }
    }
    return true;
}

} // namespace redox::utf8

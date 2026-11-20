// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#include "redox/json.hpp"
#include "redox/writer.hpp"
#include "utf8_helper.hpp"

#include <cmath>

namespace redox {

namespace {

void write_json_element(Utf8TextWriter& w, DElement elem, const JsonWriteOptions& opts, int depth) {
    if (!elem.is_valid() || elem.is_null()) {
        w.write_raw("null");
        return;
    }

    DToken tok = elem.token();
    if (tok.type() == DTokenType::Array) {
        auto items = elem.array_elements();
        w.write_char('[');
        if (!items.empty()) {
            for (size_t i = 0; i < items.size(); ++i) {
                if (i > 0) w.write_char(',');
                if (opts.indented) {
                    w.write_newline(opts.newline);
                    w.write_indent(depth + 1, opts.indent_string);
                }
                write_json_element(w, items[i], opts, depth + 1);
            }
            if (opts.indented) {
                w.write_newline(opts.newline);
                w.write_indent(depth, opts.indent_string);
            }
        }
        w.write_char(']');
        return;
    }

    if (tok.type() == DTokenType::Map) {
        auto entries = elem.map_entries();
        w.write_char('{');
        if (!entries.empty()) {
            for (size_t i = 0; i < entries.size(); ++i) {
                if (i > 0) w.write_char(',');
                if (opts.indented) {
                    w.write_newline(opts.newline);
                    w.write_indent(depth + 1, opts.indent_string);
                }
                std::string key_str = entries[i].first.is_string()
                                          ? entries[i].first.get_string()
                                          : JsonWriter::write_json(entries[i].first);
                std::string escaped;
                utf8::write_escaped_json_string(escaped, key_str, '"', opts.escape_non_ascii);
                w.write_raw(escaped);
                w.write_raw(opts.indented ? ": " : ":");
                write_json_element(w, entries[i].second, opts, depth + 1);
            }
            if (opts.indented) {
                w.write_newline(opts.newline);
                w.write_indent(depth, opts.indent_string);
            }
        }
        w.write_char('}');
        return;
    }

    switch (tok.kind()) {
        case DTokenKind::Boolean:
            w.write_raw(elem.get_bool() ? "true" : "false");
            break;
        case DTokenKind::Integer:
            if (tok.integer_kind() == IntegerKind::Unsigned) {
                w.write_uint64(elem.get_uint64());
            } else {
                w.write_int64(elem.get_int64());
            }
            break;
        case DTokenKind::Float:
        case DTokenKind::InlineFloat:
            w.write_double(elem.get_double());
            break;
        case DTokenKind::String:
        case DTokenKind::Symbol:
        case DTokenKind::BigNumber: {
            std::string escaped;
            utf8::write_escaped_json_string(escaped, elem.get_string(), '"', opts.escape_non_ascii);
            w.write_raw(escaped);
            break;
        }
        case DTokenKind::ByteString: {
            std::string b64 = utf8::base64_encode(elem.get_bytes());
            w.write_char('"');
            w.write_raw(b64);
            w.write_char('"');
            break;
        }
        case DTokenKind::Timestamp: {
            w.write_char('"');
            w.write_raw(elem.get_timestamp().to_iso8601());
            w.write_char('"');
            break;
        }
        default:
            w.write_raw("null");
            break;
    }
}

void write_leading_trivia(Utf8TextWriter& w, DElement elem, const Json5WriteOptions& opts, int depth) {
    if (!opts.preserve_trivia) return;
    auto trivias = elem.leading_trivia();
    for (const auto& t : trivias) {
        if (t.text.empty()) continue;
        w.write_raw(t.text);
        w.write_newline(opts.newline);
        if (opts.indented) {
            w.write_indent(depth, opts.indent_string);
        }
    }
}

void write_json5_element(Utf8TextWriter& w, DElement elem, const Json5WriteOptions& opts, int depth) {
    if (!elem.is_valid() || elem.is_null()) {
        w.write_raw("null");
        return;
    }

    DToken tok = elem.token();
    if (tok.type() == DTokenType::Array) {
        auto items = elem.array_elements();
        w.write_char('[');
        if (!items.empty()) {
            for (size_t i = 0; i < items.size(); ++i) {
                if (opts.indented) {
                    w.write_newline(opts.newline);
                    w.write_indent(depth + 1, opts.indent_string);
                }
                write_leading_trivia(w, items[i], opts, depth + 1);
                write_json5_element(w, items[i], opts, depth + 1);
                if (i + 1 < items.size() || opts.trailing_commas) {
                    w.write_char(',');
                }
            }
            if (opts.indented) {
                w.write_newline(opts.newline);
                w.write_indent(depth, opts.indent_string);
            }
        }
        w.write_char(']');
        return;
    }

    if (tok.type() == DTokenType::Map) {
        auto entries = elem.map_entries();
        w.write_char('{');
        if (!entries.empty()) {
            for (size_t i = 0; i < entries.size(); ++i) {
                if (opts.indented) {
                    w.write_newline(opts.newline);
                    w.write_indent(depth + 1, opts.indent_string);
                }
                write_leading_trivia(w, entries[i].first, opts, depth + 1);
                std::string key_str = entries[i].first.is_string()
                                          ? entries[i].first.get_string()
                                          : JsonWriter::write_json(entries[i].first);
                if (opts.allow_unquoted_keys && utf8::is_valid_json5_identifier(key_str)) {
                    w.write_raw(key_str);
                } else {
                    char q = (opts.quote_style == Json5QuoteStyle::Single) ? '\'' : '"';
                    std::string escaped;
                    utf8::write_escaped_json_string(escaped, key_str, q, false);
                    w.write_raw(escaped);
                }
                w.write_raw(opts.indented ? ": " : ":");
                write_json5_element(w, entries[i].second, opts, depth + 1);
                if (i + 1 < entries.size() || opts.trailing_commas) {
                    w.write_char(',');
                }
            }
            if (opts.indented) {
                w.write_newline(opts.newline);
                w.write_indent(depth, opts.indent_string);
            }
        }
        w.write_char('}');
        return;
    }

    switch (tok.kind()) {
        case DTokenKind::Boolean:
            w.write_raw(elem.get_bool() ? "true" : "false");
            break;
        case DTokenKind::Integer:
            if (tok.integer_kind() == IntegerKind::Unsigned) {
                w.write_uint64(elem.get_uint64());
            } else {
                w.write_int64(elem.get_int64());
            }
            break;
        case DTokenKind::Float:
        case DTokenKind::InlineFloat: {
            double v = elem.get_double();
            if (std::isnan(v)) {
                w.write_raw("NaN");
            } else if (std::isinf(v)) {
                w.write_raw(std::signbit(v) ? "-Infinity" : "Infinity");
            } else {
                w.write_double(v);
            }
            break;
        }
        case DTokenKind::String:
        case DTokenKind::Symbol:
        case DTokenKind::BigNumber: {
            char q = '"';
            if (opts.quote_style == Json5QuoteStyle::Single) {
                q = '\'';
            } else if (opts.quote_style == Json5QuoteStyle::Preserve &&
                       tok.kind() == DTokenKind::String &&
                       tok.string_kind() == StringKind::SingleQuote) {
                q = '\'';
            }
            std::string escaped;
            utf8::write_escaped_json_string(escaped, elem.get_string(), q, false);
            w.write_raw(escaped);
            break;
        }
        case DTokenKind::ByteString: {
            std::string b64 = utf8::base64_encode(elem.get_bytes());
            w.write_char('"');
            w.write_raw(b64);
            w.write_char('"');
            break;
        }
        case DTokenKind::Timestamp: {
            w.write_char('"');
            w.write_raw(elem.get_timestamp().to_iso8601());
            w.write_char('"');
            break;
        }
        default:
            w.write_raw("null");
            break;
    }
}

} // namespace

std::string JsonWriter::write_json(DElement element, const JsonWriteOptions& options) {
    Utf8TextWriter w;
    write_json_element(w, element, options, 0);
    return w.take_string();
}

std::string JsonWriter::write_json5(DElement element, const Json5WriteOptions& options) {
    Utf8TextWriter w;
    write_leading_trivia(w, element, options, 0);
    write_json5_element(w, element, options, 0);
    return w.take_string();
}

} // namespace redox

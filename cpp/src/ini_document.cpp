// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#include "redox/ini.hpp"
#include "redox/container.hpp"
#include "redox/writer.hpp"
#include "utf8_helper.hpp"

#include <string>
#include <string_view>

namespace redox {

namespace {

std::string_view trim_ws(std::string_view s) noexcept {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

DValue parse_ini_scalar(std::string_view raw) {
    if (raw.size() >= 2 && ((raw.front() == '"' && raw.back() == '"') ||
                            (raw.front() == '\'' && raw.back() == '\''))) {
        return DValue(std::string(raw.substr(1, raw.size() - 2)));
    }
    if (raw == "true") return DValue(true);
    if (raw == "false") return DValue(false);
    int64_t iv = 0;
    if (utf8::parse_int64(raw, iv)) return DValue(iv);
    double dv = 0.0;
    if (raw.find('.') != std::string_view::npos && utf8::parse_double(raw, dv)) {
        return DValue(dv);
    }
    return DValue(std::string(raw));
}

void write_ini_scalar(Utf8TextWriter& w, DElement elem) {
    if (!elem.is_valid() || elem.is_null()) return;
    switch (elem.kind()) {
        case DTokenKind::Boolean:
            w.write_raw(elem.get_bool() ? "true" : "false");
            break;
        case DTokenKind::Integer:
            if (elem.token().integer_kind() == IntegerKind::Unsigned) {
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
        case DTokenKind::BigNumber:
            w.write_raw(elem.get_string());
            break;
        case DTokenKind::Timestamp:
            w.write_raw(elem.get_timestamp().to_iso8601());
            break;
        default:
            break;
    }
}

} // namespace

std::unique_ptr<IniDocument> IniDocument::parse(std::string_view utf8_ini, const IniDocumentOptions& options) {
    auto doc = std::unique_ptr<IniDocument>(new IniDocument());
    doc->parse_impl(utf8_ini, options);
    return doc;
}

bool IniDocument::try_parse(std::string_view utf8_ini, std::unique_ptr<IniDocument>& out, const IniDocumentOptions& options) noexcept {
    try {
        out = parse(utf8_ini, options);
        return true;
    } catch (...) {
        out.reset();
        return false;
    }
}

void IniDocument::parse_impl(std::string_view utf8_ini, const IniDocumentOptions& options) {
    reset();
    uint32_t root_id = create_empty_object_token();
    set_root_id(root_id);

    DObject* root = &materialize_object(root_id);
    DObject* current_section = root;
    std::vector<DTrivia> pending_trivia;

    size_t pos = 0;
    size_t line_no = 0;
    while (pos < utf8_ini.size()) {
        ++line_no;
        size_t nl = utf8_ini.find('\n', pos);
        std::string_view line = (nl == std::string_view::npos)
                                    ? utf8_ini.substr(pos)
                                    : utf8_ini.substr(pos, nl - pos);
        pos = (nl == std::string_view::npos) ? utf8_ini.size() : nl + 1;

        std::string_view trimmed = trim_ws(line);
        if (trimmed.empty()) continue;

        if ((options.allow_semicolon_comments && trimmed.front() == ';') ||
            (options.allow_hash_comments && trimmed.front() == '#')) {
            if (options.preserve_trivia) {
                pending_trivia.push_back({TriviaKind::LineComment, std::string(trimmed)});
            }
            continue;
        }

        if (trimmed.front() == '[') {
            size_t close = trimmed.find(']');
            if (close == std::string_view::npos) {
                throw DocumentParseException("Unterminated INI section header", pos, line_no, 1);
            }
            std::string_view section_name = trim_ws(trimmed.substr(1, close - 1));
            DElement sec_elem;
            if (!root->try_get(section_name, sec_elem)) {
                current_section = &root->add_object(section_name);
                if (!pending_trivia.empty()) {
                    // Attach comment to the section key token
                    auto& entries = root->entries();
                    auto& tl = ensure_trivia(entries.back().key_id);
                    tl.leading = std::move(pending_trivia);
                    pending_trivia.clear();
                }
            } else {
                current_section = &sec_elem.as_object();
                pending_trivia.clear();
            }
            continue;
        }

        size_t eq = trimmed.find('=');
        if (eq == std::string_view::npos) {
            throw DocumentParseException("Expected '=' in INI key-value line", pos, line_no, 1);
        }
        std::string_view key = trim_ws(trimmed.substr(0, eq));
        std::string_view val = trim_ws(trimmed.substr(eq + 1));

        current_section->set(key, parse_ini_scalar(val));
        if (!pending_trivia.empty()) {
            const auto& entries = current_section->entries();
            if (!entries.empty()) {
                auto& tl = ensure_trivia(entries.back().key_id);
                tl.leading = std::move(pending_trivia);
                pending_trivia.clear();
            }
        }
    }
}

std::string IniDocument::encode(DElement element, const IniWriteOptions& options) {
    Utf8TextWriter w(256);
    if (!element.is_valid() || !element.is_map()) {
        return {};
    }

    auto entries = element.map_entries();
    // Write top-level scalar properties first
    bool wrote_any = false;
    for (const auto& [k, v] : entries) {
        if (v.is_map()) continue;
        if (options.preserve_trivia) {
            for (const auto& t : k.leading_trivia()) {
                w.write_raw(t.text);
                w.write_newline(options.newline);
            }
        }
        w.write_raw(k.get_string());
        w.write_raw(" = ");
        write_ini_scalar(w, v);
        w.write_newline(options.newline);
        wrote_any = true;
    }

    // Write [Section] blocks
    for (const auto& [k, v] : entries) {
        if (!v.is_map()) continue;
        if (wrote_any) w.write_newline(options.newline);
        if (options.preserve_trivia) {
            for (const auto& t : k.leading_trivia()) {
                w.write_raw(t.text);
                w.write_newline(options.newline);
            }
        }
        w.write_char('[');
        w.write_raw(k.get_string());
        w.write_char(']');
        w.write_newline(options.newline);

        for (const auto& [sk, sv] : v.map_entries()) {
            if (options.preserve_trivia) {
                for (const auto& t : sk.leading_trivia()) {
                    w.write_raw(t.text);
                    w.write_newline(options.newline);
                }
            }
            w.write_raw(sk.get_string());
            w.write_raw(" = ");
            write_ini_scalar(w, sv);
            w.write_newline(options.newline);
        }
        wrote_any = true;
    }

    return w.take_string();
}

} // namespace redox

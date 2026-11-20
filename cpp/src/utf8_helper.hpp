// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace redox::utf8 {

bool is_valid_utf8(std::string_view text) noexcept;

// Encode a Unicode code point (U+0000..U+10FFFF) as UTF-8 and append to out.
bool append_codepoint(std::string& out, uint32_t cp);

// Count UTF-16 code units for a UTF-8 string (needed for DOX v1.0 wire compatibility).
size_t count_utf16_code_units(std::string_view utf8) noexcept;

// JSON / JSON5 string escaping and unescaping
std::string unescape_json_string(std::string_view raw, bool allow_json5_escapes);
void write_escaped_json_string(std::string& out, std::string_view text, char quote_char = '"', bool escape_non_ascii = false);

// Number parsing helpers
bool parse_int64(std::string_view text, int64_t& out) noexcept;
bool parse_uint64(std::string_view text, uint64_t& out) noexcept;
bool parse_double(std::string_view text, double& out) noexcept;
bool parse_json5_integer(std::string_view text, int64_t& out) noexcept;
bool parse_json5_double(std::string_view text, double& out) noexcept;

// JSON5 identifier check for unquoted object keys
bool is_valid_json5_identifier(std::string_view text) noexcept;

// Base64 encode/decode for ByteString JSON projection
std::string base64_encode(std::span<const uint8_t> data);
bool base64_decode(std::string_view text, std::vector<uint8_t>& out);

} // namespace redox::utf8

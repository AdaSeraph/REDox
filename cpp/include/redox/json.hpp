// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "redox/document.hpp"
#include "redox/element.hpp"
#include "redox/writer.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace redox {

enum class JsonCommentHandling : uint8_t {
    Disallow = 0,
    Skip     = 1,
};

struct JsonDocumentOptions {
    bool allow_trailing_commas = false;
    JsonCommentHandling comment_handling = JsonCommentHandling::Disallow;
    int max_depth = 64;
};

struct JsonWriteOptions {
    bool indented = false;
    std::string indent_string = "  ";
    std::string newline = "\n";
    bool escape_non_ascii = false;
};

enum class Json5QuoteStyle : uint8_t {
    Double = 0,
    Single = 1,
    Preserve = 2,
};

struct Json5DocumentOptions {
    int max_depth = 64;
    bool preserve_trivia = true;
};

struct Json5WriteOptions {
    bool indented = true;
    std::string indent_string = "  ";
    std::string newline = "\n";
    bool preserve_trivia = true;
    bool allow_unquoted_keys = true;
    bool trailing_commas = false;
    Json5QuoteStyle quote_style = Json5QuoteStyle::Preserve;
};

class JsonDocument final : public Document {
public:
    static std::unique_ptr<JsonDocument> parse(std::string_view utf8_json, const JsonDocumentOptions& options = {});
    static std::unique_ptr<JsonDocument> parse(std::span<const uint8_t> utf8_bytes, const JsonDocumentOptions& options = {});
    static bool try_parse(std::string_view utf8_json, std::unique_ptr<JsonDocument>& out, const JsonDocumentOptions& options = {}) noexcept;

    // NDJSON (newline-delimited JSON) helper
    static std::vector<std::unique_ptr<JsonDocument>> parse_ndjson(std::string_view utf8_ndjson, const JsonDocumentOptions& options = {});

    std::string_view source() const noexcept { return source_; }

    std::string decode_string(DToken token) const override;
    std::string_view decode_raw_utf8(DToken token, bool& ok) const override;
    int64_t decode_integer(DToken token) const override;
    uint64_t decode_uinteger(DToken token) const override;
    double decode_float(DToken token) const override;

private:
    JsonDocument() = default;
    void parse_impl(const JsonDocumentOptions& options);

    std::string source_;
};

class Json5Document final : public Document {
public:
    static std::unique_ptr<Json5Document> parse(std::string_view utf8_json5, const Json5DocumentOptions& options = {});
    static bool try_parse(std::string_view utf8_json5, std::unique_ptr<Json5Document>& out, const Json5DocumentOptions& options = {}) noexcept;

    std::string_view source() const noexcept { return source_; }

    std::string decode_string(DToken token) const override;
    std::string_view decode_raw_utf8(DToken token, bool& ok) const override;
    int64_t decode_integer(DToken token) const override;
    uint64_t decode_uinteger(DToken token) const override;
    double decode_float(DToken token) const override;
    std::string decode_trivia(DToken token) const override;

private:
    Json5Document() = default;
    void parse_impl(const Json5DocumentOptions& options);

    std::string source_;
};

class JsonWriter {
public:
    static std::string write_json(DElement element, const JsonWriteOptions& options = {});
    static std::string write_json5(DElement element, const Json5WriteOptions& options = {});
};

} // namespace redox

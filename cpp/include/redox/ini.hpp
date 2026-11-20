// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "redox/document.hpp"
#include "redox/element.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace redox {

enum class IniCommentStyle : uint8_t {
    Semicolon = 0, // ;
    Hash      = 1, // #
};

struct IniDocumentOptions {
    bool allow_semicolon_comments = true;
    bool allow_hash_comments = true;
    bool preserve_trivia = true;
};

struct IniWriteOptions {
    IniCommentStyle comment_style = IniCommentStyle::Semicolon;
    std::string newline = "\n";
    bool preserve_trivia = true;
};

class IniDocument final : public Document {
public:
    static std::unique_ptr<IniDocument> parse(std::string_view utf8_ini, const IniDocumentOptions& options = {});
    static bool try_parse(std::string_view utf8_ini, std::unique_ptr<IniDocument>& out, const IniDocumentOptions& options = {}) noexcept;

    static std::string encode(DElement element, const IniWriteOptions& options = {});

private:
    IniDocument() = default;
    void parse_impl(std::string_view utf8_ini, const IniDocumentOptions& options);
};

} // namespace redox

// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "redox/document.hpp"
#include "redox/element.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace redox {

struct MessagePackDocumentOptions {
    int max_depth = 64;
};

struct MessagePackWriteOptions {};

class MessagePackDocument final : public Document {
public:
    static std::unique_ptr<MessagePackDocument> parse(std::span<const uint8_t> data, const MessagePackDocumentOptions& options = {});
    static bool try_parse(std::span<const uint8_t> data, std::unique_ptr<MessagePackDocument>& out, const MessagePackDocumentOptions& options = {}) noexcept;

    static std::vector<uint8_t> encode(DElement element, const MessagePackWriteOptions& options = {});

private:
    MessagePackDocument() = default;
    void parse_impl(std::span<const uint8_t> data, const MessagePackDocumentOptions& options);
};

} // namespace redox

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

struct CborDocumentOptions {
    int max_depth = 64;
};

struct CborWriteOptions {};

class CborDocument final : public Document {
public:
    static std::unique_ptr<CborDocument> parse(std::span<const uint8_t> data, const CborDocumentOptions& options = {});
    static bool try_parse(std::span<const uint8_t> data, std::unique_ptr<CborDocument>& out, const CborDocumentOptions& options = {}) noexcept;

    static std::vector<uint8_t> encode(DElement element, const CborWriteOptions& options = {});

private:
    CborDocument() = default;
    void parse_impl(std::span<const uint8_t> data, const CborDocumentOptions& options);
};

} // namespace redox

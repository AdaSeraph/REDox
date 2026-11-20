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

struct DoxDocumentOptions {};
struct DoxWriteOptions {};

// Wire-compatible with RE:Dox C# DoxDocument v1.0 ("DOX\0" magic).
class DoxDocument final : public Document {
public:
    static constexpr int16_t kMajorVersion = 1;
    static constexpr int16_t kMinorVersion = 0;
    static constexpr uint32_t kMagic = static_cast<uint32_t>('D') |
                                       (static_cast<uint32_t>('O') << 8) |
                                       (static_cast<uint32_t>('X') << 16);

    static std::unique_ptr<DoxDocument> parse(std::span<const uint8_t> data, const DoxDocumentOptions& options = {});
    static bool try_parse(std::span<const uint8_t> data, std::unique_ptr<DoxDocument>& out, const DoxDocumentOptions& options = {}) noexcept;

    static std::vector<uint8_t> encode(DElement element, const DoxWriteOptions& options = {});

    std::string decode_string(DToken token) const override;
    std::string_view decode_raw_utf8(DToken token, bool& ok) const override;
    int64_t decode_integer(DToken token) const override;
    uint64_t decode_uinteger(DToken token) const override;
    double decode_float(DToken token) const override;
    std::vector<uint8_t> decode_bytes(DToken token) const override;
    Timestamp decode_timestamp(DToken token) const override;

private:
    DoxDocument() = default;
    void read_impl(std::span<const uint8_t> data);

    std::vector<uint8_t> source_;
};

} // namespace redox

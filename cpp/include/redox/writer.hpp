// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace redox {

class Utf8TextWriter {
public:
    explicit Utf8TextWriter(size_t initial_capacity = 256);

    void write_char(char c) { buffer_.push_back(c); }
    void write_raw(std::string_view text) { buffer_.append(text); }
    void write_bytes(std::span<const uint8_t> bytes);
    void write_indent(int depth, std::string_view indent_unit);
    void write_newline(std::string_view newline = "\n") { buffer_.append(newline); }

    void write_int64(int64_t value);
    void write_uint64(uint64_t value);
    void write_double(double value);

    const std::string& str() const noexcept { return buffer_; }
    std::string take_string() noexcept { return std::move(buffer_); }
    void clear() noexcept { buffer_.clear(); }
    size_t size() const noexcept { return buffer_.size(); }

private:
    std::string buffer_;
};

class BinaryWriter {
public:
    explicit BinaryWriter(size_t initial_capacity = 256);

    void write_u8(uint8_t v) { buffer_.push_back(v); }
    void write_u16_le(uint16_t v);
    void write_u32_le(uint32_t v);
    void write_u64_le(uint64_t v);
    void write_i64_le(int64_t v);
    void write_f32_le(float v);
    void write_f64_le(double v);

    void write_u16_be(uint16_t v);
    void write_u32_be(uint32_t v);
    void write_u64_be(uint64_t v);
    void write_f32_be(float v);
    void write_f64_be(double v);

    void write_bytes(std::span<const uint8_t> data);
    void write_string(std::string_view text);

    std::vector<uint8_t>& buffer() noexcept { return buffer_; }
    const std::vector<uint8_t>& buffer() const noexcept { return buffer_; }
    std::vector<uint8_t> take_bytes() noexcept { return std::move(buffer_); }
    size_t size() const noexcept { return buffer_.size(); }

private:
    std::vector<uint8_t> buffer_;
};

} // namespace redox

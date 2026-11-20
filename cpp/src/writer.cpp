// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#include "redox/writer.hpp"

#include <bit>
#include <charconv>
#include <cmath>
#include <cstring>

namespace redox {

Utf8TextWriter::Utf8TextWriter(size_t initial_capacity) {
    buffer_.reserve(initial_capacity);
}

void Utf8TextWriter::write_bytes(std::span<const uint8_t> bytes) {
    buffer_.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

void Utf8TextWriter::write_indent(int depth, std::string_view indent_unit) {
    for (int i = 0; i < depth; ++i) {
        buffer_.append(indent_unit);
    }
}

void Utf8TextWriter::write_int64(int64_t value) {
    char buf[32];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
    if (ec == std::errc{}) {
        buffer_.append(buf, static_cast<size_t>(ptr - buf));
    }
}

void Utf8TextWriter::write_uint64(uint64_t value) {
    char buf[32];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
    if (ec == std::errc{}) {
        buffer_.append(buf, static_cast<size_t>(ptr - buf));
    }
}

void Utf8TextWriter::write_double(double value) {
    if (std::isnan(value)) {
        buffer_.append("null");
        return;
    }
    if (std::isinf(value)) {
        buffer_.append("null");
        return;
    }
    if (value == 0.0) {
        if (std::signbit(value)) {
            buffer_.append("-0.0");
        } else {
            buffer_.append("0.0");
        }
        return;
    }
    char buf[64];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
    if (ec == std::errc{}) {
        std::string_view sv(buf, static_cast<size_t>(ptr - buf));
        buffer_.append(sv);
        // Ensure floating-point representation keeps decimal or exponent so re-parsing yields float
        if (sv.find('.') == std::string_view::npos &&
            sv.find('e') == std::string_view::npos &&
            sv.find('E') == std::string_view::npos) {
            buffer_.append(".0");
        }
    }
}

BinaryWriter::BinaryWriter(size_t initial_capacity) {
    buffer_.reserve(initial_capacity);
}

void BinaryWriter::write_u16_le(uint16_t v) {
    buffer_.push_back(static_cast<uint8_t>(v & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

void BinaryWriter::write_u32_le(uint32_t v) {
    buffer_.push_back(static_cast<uint8_t>(v & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

void BinaryWriter::write_u64_le(uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        buffer_.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
    }
}

void BinaryWriter::write_i64_le(int64_t v) {
    write_u64_le(static_cast<uint64_t>(v));
}

void BinaryWriter::write_f32_le(float v) {
    write_u32_le(std::bit_cast<uint32_t>(v));
}

void BinaryWriter::write_f64_le(double v) {
    write_u64_le(std::bit_cast<uint64_t>(v));
}

void BinaryWriter::write_u16_be(uint16_t v) {
    buffer_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>(v & 0xFF));
}

void BinaryWriter::write_u32_be(uint32_t v) {
    buffer_.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>(v & 0xFF));
}

void BinaryWriter::write_u64_be(uint64_t v) {
    for (int i = 7; i >= 0; --i) {
        buffer_.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
    }
}

void BinaryWriter::write_f32_be(float v) {
    write_u32_be(std::bit_cast<uint32_t>(v));
}

void BinaryWriter::write_f64_be(double v) {
    write_u64_be(std::bit_cast<uint64_t>(v));
}

void BinaryWriter::write_bytes(std::span<const uint8_t> data) {
    buffer_.insert(buffer_.end(), data.begin(), data.end());
}

void BinaryWriter::write_string(std::string_view text) {
    buffer_.insert(buffer_.end(),
                   reinterpret_cast<const uint8_t*>(text.data()),
                   reinterpret_cast<const uint8_t*>(text.data() + text.size()));
}

} // namespace redox

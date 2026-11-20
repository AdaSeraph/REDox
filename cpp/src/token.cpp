// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#include "redox/token.hpp"

#include <sstream>

namespace redox {

std::string DToken::to_debug_string() const {
    std::ostringstream oss;
    if (kind() == DTokenKind::Control) {
        if (is_empty()) {
            if (is_empty_token()) {
                oss << "Empty next: " << empty_id();
            } else {
                oss << "EmptyExt id: " << extend_id() << " next: " << empty_id();
            }
            return oss.str();
        }
        if (raw == 0) {
            return "Nop";
        }
        oss << "Jump link: " << jump_id();
        return oss.str();
    }

    if (is_extended()) {
        if (is_container()) {
            oss << (type() == DTokenType::Map ? "Map*" : "Array*")
                << " id: " << extend_id() << " link: " << link_id();
            return oss.str();
        }
        if (kind() == DTokenKind::InlineFloat) {
            oss << "InlineFloat* value: " << decode_inline_float(*this);
            return oss.str();
        }
        if (kind() == DTokenKind::Boolean || kind() == DTokenKind::Null) {
            oss << "Literal* variant=" << static_cast<int>(variant());
            if (trivia_id() != 0) {
                oss << " trivia: " << trivia_id();
            }
            return oss.str();
        }
        if (is_extend_inline_literal()) {
            if (kind() == DTokenKind::Integer) {
                oss << "IntegerInline* value: " << decode_inline_integer(*this);
                return oss.str();
            }
            if (kind() == DTokenKind::Float) {
                oss << "FloatInline* value: " << decode_inline_single(*this);
                return oss.str();
            }
        }
        oss << "Extended* variant=" << static_cast<int>(variant()) << " id: " << extend_id();
        if (trivia_id() != 0) {
            oss << " trivia: " << trivia_id();
        }
        return oss.str();
    }

    if (is_container()) {
        oss << (type() == DTokenType::Map ? "Map" : "Array")
            << " count: " << count() << " link: " << link_id();
        return oss.str();
    }

    if (kind() == DTokenKind::InlineFloat) {
        oss << "InlineFloat value: " << decode_inline_float(*this);
        return oss.str();
    }

    if (kind() == DTokenKind::Boolean || kind() == DTokenKind::Null) {
        oss << "Literal variant=" << static_cast<int>(variant());
        return oss.str();
    }

    auto [length, offset] = decode_length_offset(*this);
    oss << "Token variant=" << static_cast<int>(variant())
        << " (length: " << length << ", offset: " << offset << ")";
    return oss.str();
}

} // namespace redox

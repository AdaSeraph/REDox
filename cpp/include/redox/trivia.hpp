// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "redox/token.hpp"

#include <string>
#include <vector>

namespace redox {

struct DTrivia {
    TriviaKind kind = TriviaKind::Default;
    std::string text;

    bool operator==(const DTrivia& other) const noexcept = default;
};

struct TriviaList {
    std::vector<DTrivia> leading;
    std::vector<DTrivia> trailing;

    bool empty() const noexcept { return leading.empty() && trailing.empty(); }
};

} // namespace redox

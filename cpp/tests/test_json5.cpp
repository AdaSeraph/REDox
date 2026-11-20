// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#include "redox/redox.hpp"
#include "test_framework.hpp"

#include <cmath>

using namespace redox;

static void test_json5_syntax_and_trivia_editing() {
    const std::string input = R"(
// Top-level player config
{
  // Character display name
  name: 'Leon',
  /* Hex HP value */
  hp: 0x64,
  ratio: .5,
  inf: Infinity,
  negInf: -Infinity,
  notNum: NaN,
  items: [
    // Primary weapon
    'Handgun',
    'Green Herb',
  ],
}
)";

    auto doc = Json5Document::parse(input);
    REDOX_CHECK_EQ(doc->root_element()["name"].get_string(), "Leon");
    REDOX_CHECK_EQ(doc->root_element()["hp"].get_int64(), 100);
    REDOX_CHECK_NEAR(doc->root_element()["ratio"].get_double(), 0.5, 1e-12);
    REDOX_CHECK(std::isinf(doc->root_element()["inf"].get_double()));
    REDOX_CHECK(std::isinf(doc->root_element()["negInf"].get_double()));
    REDOX_CHECK(std::isnan(doc->root_element()["notNum"].get_double()));

    // Edit in place while preserving comments
    auto& root = doc->root_element().as_object();
    root.set("name", "Claire");
    root.set("hp", 150);

    std::string out = doc->root_element().to_json5_string();
    REDOX_CHECK(out.find("// Top-level player config") != std::string::npos);
    REDOX_CHECK(out.find("// Character display name") != std::string::npos);
    REDOX_CHECK(out.find("/* Hex HP value */") != std::string::npos);
    REDOX_CHECK(out.find("// Primary weapon") != std::string::npos);

    // Re-parse edited JSON5
    auto reparsed = Json5Document::parse(out);
    REDOX_CHECK_EQ(reparsed->root_element()["name"].get_string(), "Claire");
    REDOX_CHECK_EQ(reparsed->root_element()["hp"].get_int64(), 150);
    REDOX_CHECK_EQ(reparsed->root_element()["items"].count(), 2u);
}

int main() {
    test_json5_syntax_and_trivia_editing();
    return redox::test::finish("test_json5");
}

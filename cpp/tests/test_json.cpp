// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#include "redox/redox.hpp"
#include "test_framework.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace redox;

static void test_json_readme_example() {
    const std::string input = R"({"Name":"Leon","Level":42,"Items":["Handgun","Green Herb"]})";
    auto doc = JsonDocument::parse(input);

    auto& root = doc->root_element().as_object();
    root.set("Name", "Claire");
    root.add("Hp", 100);
    root.remove("Level");

    auto& items = root["Items"].as_array();
    items.add("First Aid Spray");
    items.insert(0, "Knife");
    items.remove_at(1);

    std::string edited = doc->root_element().to_json_string();
    auto reparsed = JsonDocument::parse(edited);
    REDOX_CHECK_EQ(reparsed->root_element()["Name"].get_string(), "Claire");
    REDOX_CHECK_EQ(reparsed->root_element()["Hp"].get_int64(), 100);
    REDOX_CHECK(!reparsed->root_element().contains_key("Level"));
    REDOX_CHECK_EQ(reparsed->root_element()["Items"].count(), 3u);
    REDOX_CHECK_EQ(reparsed->root_element()["Items"][0].get_string(), "Knife");
    REDOX_CHECK_EQ(reparsed->root_element()["Items"][2].get_string(), "First Aid Spray");
}

static void test_json_escapes_and_unicode() {
    const std::string input = R"({"emoji":"\uD83D\uDE80","escaped":"a\nb\tc\"\\\/","cn":"卡普空"})";
    auto doc = JsonDocument::parse(input);
    REDOX_CHECK_EQ(doc->root_element()["emoji"].get_string(), "\xF0\x9F\x9A\x80");
    REDOX_CHECK_EQ(doc->root_element()["escaped"].get_string(), "a\nb\tc\"\\/");
    REDOX_CHECK_EQ(doc->root_element()["cn"].get_string(), "卡普空");

    JsonWriteOptions opts;
    opts.indented = true;
    std::string pretty = doc->root_element().to_json_string(opts);
    auto reparsed = JsonDocument::parse(pretty);
    REDOX_CHECK_EQ(reparsed->root_element()["emoji"].get_string(), "\xF0\x9F\x9A\x80");
    REDOX_CHECK_EQ(reparsed->root_element()["cn"].get_string(), "卡普空");
}

static void test_json_strict_errors_and_options() {
    REDOX_CHECK_THROWS(JsonDocument::parse(""), DocumentParseException);
    REDOX_CHECK_THROWS(JsonDocument::parse("{1:2}"), DocumentParseException);
    REDOX_CHECK_THROWS(JsonDocument::parse("[1,2,]"), DocumentParseException);
    REDOX_CHECK_THROWS(JsonDocument::parse("01"), DocumentParseException);
    REDOX_CHECK_THROWS(JsonDocument::parse("\"unterminated"), DocumentParseException);
    REDOX_CHECK_THROWS(JsonDocument::parse("true false"), DocumentParseException);

    // Trailing comma option
    JsonDocumentOptions opts;
    opts.allow_trailing_commas = true;
    opts.comment_handling = JsonCommentHandling::Skip;
    auto doc = JsonDocument::parse("/* ok */ [1, 2, 3,]", opts);
    REDOX_CHECK_EQ(doc->root_element().count(), 3u);
    REDOX_CHECK_EQ(doc->root_element()[2].get_int64(), 3);

    // NDJSON
    auto nd = JsonDocument::parse_ndjson("{\"id\":1}\n{\"id\":2}\n\n{\"id\":3}\n");
    REDOX_CHECK_EQ(nd.size(), 3u);
    REDOX_CHECK_EQ(nd[0]->root_element()["id"].get_int64(), 1);
    REDOX_CHECK_EQ(nd[2]->root_element()["id"].get_int64(), 3);
}

#ifdef REDOX_JSON_TEST_SUITE_DIR
static void test_external_json_test_suite() {
    namespace fs = std::filesystem;
    fs::path dir(REDOX_JSON_TEST_SUITE_DIR);
    if (!fs::exists(dir)) return;

    int y_pass = 0, n_pass = 0;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        std::string name = entry.path().filename().string();
        std::ifstream ifs(entry.path(), std::ios::binary);
        std::ostringstream oss;
        oss << ifs.rdbuf();
        std::string content = oss.str();

        std::unique_ptr<JsonDocument> doc;
        bool ok = JsonDocument::try_parse(content, doc);
        if (name.rfind("y_", 0) == 0) {
            REDOX_CHECK(ok);
            if (ok) ++y_pass;
        } else if (name.rfind("n_", 0) == 0) {
            REDOX_CHECK(!ok);
            if (!ok) ++n_pass;
        }
    }
    std::cout << "[INFO] JSONTestSuite: y_pass=" << y_pass << ", n_pass=" << n_pass << "\n";
}
#endif

int main() {
    test_json_readme_example();
    test_json_escapes_and_unicode();
    test_json_strict_errors_and_options();
#ifdef REDOX_JSON_TEST_SUITE_DIR
    test_external_json_test_suite();
#endif
    return redox::test::finish("test_json");
}

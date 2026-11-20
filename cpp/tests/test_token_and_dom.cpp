// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#include "redox/redox.hpp"
#include "test_framework.hpp"

#include <limits>

using namespace redox;

static void test_dtoken_bit_layout() {
    static_assert(sizeof(DToken) == 8);

    // Inline 53-bit signed integers
    for (int64_t v : {int64_t{0}, int64_t{1}, int64_t{-1}, int64_t{42}, int64_t{-123456789},
                      DToken::kMinInlineInteger, DToken::kMaxInlineInteger}) {
        DToken t = DToken::make_extend_inline_integer(IntegerKind::Default, v);
        REDOX_CHECK(t.is_extended());
        REDOX_CHECK(t.kind() == DTokenKind::Integer);
        REDOX_CHECK(t.is_inline_payload());
        REDOX_CHECK_EQ(DToken::decode_inline_integer(t), v);
    }

    // Inline float
    for (double d : {0.0, -0.0, 1.0, -3.141592653589793, 1e9, -1e-9}) {
        int64_t payload = DToken::encode_inline_float(d);
        REDOX_CHECK(payload >= 0);
        DToken t = DToken::make_extend_inline_float(payload);
        REDOX_CHECK(t.is_extended());
        REDOX_CHECK(t.kind() == DTokenKind::InlineFloat);
        REDOX_CHECK_EQ(DToken::decode_inline_float(t), d);
    }

    // Inline single
    DToken sf = DToken::make_extend_inline_single(FloatKind::Single, 2.5f);
    REDOX_CHECK(sf.is_extended());
    REDOX_CHECK(sf.is_inline_payload());
    REDOX_CHECK_NEAR(DToken::decode_inline_single(sf), 2.5f, 1e-6);

    // Container tokens
    DToken arr = DToken::make_array(123);
    arr.set_link_id(456);
    REDOX_CHECK(arr.is_container());
    REDOX_CHECK(!arr.is_extended());
    REDOX_CHECK_EQ(arr.count(), 123u);
    REDOX_CHECK_EQ(arr.link_id(), 456u);

    DToken ext_map = DToken::make_extend_container(DTokenType::Map, 77, 888);
    REDOX_CHECK(ext_map.is_container());
    REDOX_CHECK(ext_map.is_extended());
    REDOX_CHECK_EQ(ext_map.extend_id(), 77u);
    REDOX_CHECK_EQ(ext_map.link_id(), 888u);

    // Empty / Jump control tokens
    DToken emp = DToken::make_empty(99);
    REDOX_CHECK(emp.is_empty());
    REDOX_CHECK(emp.is_empty_token());
    REDOX_CHECK_EQ(emp.empty_id(), 99u);

    DToken jmp = DToken::make_jump(321);
    REDOX_CHECK(!jmp.is_extended());
    REDOX_CHECK_EQ(jmp.jump_id(), 321u);

    // Length/offset codec
    int64_t lo = DToken::encode_length_offset(12345, 67890);
    DToken str_tok = DToken::make(DTokenVariant::StringDoubleQuote, lo);
    auto [len, off] = DToken::decode_length_offset(str_tok);
    REDOX_CHECK_EQ(len, 12345u);
    REDOX_CHECK_EQ(off, 67890u);
}

static void test_mutable_dom_and_freelist() {
    auto doc = MemoryDocument::create_object();
    auto& root = doc->root_element().as_object();
    root.add("Name", "Leon");
    root.add("Level", 42);
    root.add("BigSigned", std::numeric_limits<int64_t>::max());
    root.add("BigUnsigned", std::numeric_limits<uint64_t>::max());
    root.add("Pi", 3.141592653589793);
    root.add("Active", true);
    root.add("Empty", nullptr);

    auto& items = root.add_array("Items");
    items.add("Handgun");
    items.add("Green Herb");

    REDOX_CHECK_EQ(doc->root_element()["Name"].get_string(), "Leon");
    REDOX_CHECK_EQ(doc->root_element()["Level"].get_int64(), 42);
    REDOX_CHECK_EQ(doc->root_element()["BigSigned"].get_int64(), std::numeric_limits<int64_t>::max());
    REDOX_CHECK_EQ(doc->root_element()["BigUnsigned"].get_uint64(), std::numeric_limits<uint64_t>::max());
    REDOX_CHECK_NEAR(doc->root_element()["Pi"].get_double(), 3.141592653589793, 1e-12);
    REDOX_CHECK_EQ(doc->root_element()["Active"].get_bool(), true);
    REDOX_CHECK(doc->root_element()["Empty"].is_null());

    // Mutate in place (just like RE:Dox README 30-second example)
    root.set("Name", "Claire");
    root.add("Hp", 100);
    REDOX_CHECK(root.remove("Level"));
    REDOX_CHECK(!root.contains_key("Level"));

    items.add("First Aid Spray");
    items.insert(0, "Knife");
    items.remove_at(1); // removes "Handgun"

    REDOX_CHECK_EQ(items.count(), 3u);
    REDOX_CHECK_EQ(items[0].get_string(), "Knife");
    REDOX_CHECK_EQ(items[1].get_string(), "Green Herb");
    REDOX_CHECK_EQ(items[2].get_string(), "First Aid Spray");

    // Deep duplicate isolation
    auto copy = doc->duplicate();
    root.set("Name", "Ada");
    REDOX_CHECK_EQ(doc->root_element()["Name"].get_string(), "Ada");
    REDOX_CHECK_EQ(copy->root_element()["Name"].get_string(), "Claire");
}

int main() {
    test_dtoken_bit_layout();
    test_mutable_dom_and_freelist();
    return redox::test::finish("test_token_and_dom");
}

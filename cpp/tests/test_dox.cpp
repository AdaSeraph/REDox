// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#include "redox/redox.hpp"
#include "test_framework.hpp"

#include <limits>

using namespace redox;

static void test_dox_roundtrip_and_wire_header() {
    auto mem = MemoryDocument::create_object();
    auto& root = mem->root_element().as_object();
    root.add("empty_str", "");
    root.add("short_str", "RE:Dox C++20");
    root.add("long_str", std::string(5000, 'X'));
    root.add("small_int", -42);
    root.add("max_int", std::numeric_limits<int64_t>::max());
    root.add("max_uint", std::numeric_limits<uint64_t>::max());
    root.add("dbl", 3.141592653589793);
    root.add("flag", true);
    root.add("nil", nullptr);
    root.add("bin", std::vector<uint8_t>{0xDE, 0xAD, 0xBE, 0xEF});

    Timestamp ts;
    REDOX_CHECK(Timestamp::try_parse_iso8601("2026-11-20T09:30:00Z", ts));
    root.add("ts", ts);

    auto& nested = root.add_array("arr");
    nested.add(1);
    nested.add("two");
    nested.add(false);

    std::vector<uint8_t> dox_bytes = DoxDocument::encode(mem->root_element());
    REDOX_CHECK(dox_bytes.size() >= 16);

    // Verify DOX v1.0 wire header ("DOX\0" + version 0x00010000 LE)
    REDOX_CHECK_EQ(dox_bytes[0], static_cast<uint8_t>('D'));
    REDOX_CHECK_EQ(dox_bytes[1], static_cast<uint8_t>('O'));
    REDOX_CHECK_EQ(dox_bytes[2], static_cast<uint8_t>('X'));
    REDOX_CHECK_EQ(dox_bytes[3], static_cast<uint8_t>(0));
    REDOX_CHECK_EQ(dox_bytes[4], static_cast<uint8_t>(0));
    REDOX_CHECK_EQ(dox_bytes[5], static_cast<uint8_t>(0));
    REDOX_CHECK_EQ(dox_bytes[6], static_cast<uint8_t>(1));
    REDOX_CHECK_EQ(dox_bytes[7], static_cast<uint8_t>(0));

    auto parsed = DoxDocument::parse(dox_bytes);
    auto r = parsed->root_element();
    REDOX_CHECK_EQ(r["empty_str"].get_string(), "");
    REDOX_CHECK_EQ(r["short_str"].get_string(), "RE:Dox C++20");
    REDOX_CHECK_EQ(r["long_str"].get_string().size(), 5000u);
    REDOX_CHECK_EQ(r["small_int"].get_int64(), -42);
    REDOX_CHECK_EQ(r["max_int"].get_int64(), std::numeric_limits<int64_t>::max());
    REDOX_CHECK_EQ(r["max_uint"].get_uint64(), std::numeric_limits<uint64_t>::max());
    REDOX_CHECK_NEAR(r["dbl"].get_double(), 3.141592653589793, 1e-12);
    REDOX_CHECK_EQ(r["flag"].get_bool(), true);
    REDOX_CHECK(r["nil"].is_null());

    auto bin = r["bin"].get_bytes();
    REDOX_CHECK_EQ(bin.size(), 4u);
    REDOX_CHECK_EQ(bin[0], 0xDE);
    REDOX_CHECK_EQ(bin[3], 0xEF);

    REDOX_CHECK_EQ(r["ts"].get_timestamp().unix_seconds, ts.unix_seconds);
    REDOX_CHECK_EQ(r["arr"].count(), 3u);
    REDOX_CHECK_EQ(r["arr"][1].get_string(), "two");

    // Mutate parsed DOX document in place
    r.as_object().set("short_str", "Edited");
    REDOX_CHECK_EQ(r["short_str"].get_string(), "Edited");

    // Invalid DOX rejected
    std::vector<uint8_t> bad = {'B', 'A', 'D', 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    REDOX_CHECK_THROWS(DoxDocument::parse(bad), DocumentParseException);
}

int main() {
    test_dox_roundtrip_and_wire_header();
    return redox::test::finish("test_dox");
}

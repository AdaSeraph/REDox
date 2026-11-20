// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#include "redox/redox.hpp"
#include "test_framework.hpp"

using namespace redox;

static void test_cross_format_pipeline() {
    const std::string json_in =
        R"({"engine":"REX","version":2,"pi":3.14159,"active":true,"empty":null,"tags":["json","cbor","msgpack","dox"]})";

    // JSON -> DOX -> CBOR -> MessagePack -> JSON
    auto jdoc = JsonDocument::parse(json_in);
    auto dox_bytes = jdoc->root_element().to_dox();
    auto ddoc = DoxDocument::parse(dox_bytes);

    auto cbor_bytes = ddoc->root_element().to_cbor();
    auto cdoc = CborDocument::parse(cbor_bytes);

    auto mp_bytes = cdoc->root_element().to_messagepack();
    auto mdoc = MessagePackDocument::parse(mp_bytes);

    auto r = mdoc->root_element();
    REDOX_CHECK_EQ(r["engine"].get_string(), "REX");
    REDOX_CHECK_EQ(r["version"].get_int64(), 2);
    REDOX_CHECK_NEAR(r["pi"].get_double(), 3.14159, 1e-12);
    REDOX_CHECK_EQ(r["active"].get_bool(), true);
    REDOX_CHECK(r["empty"].is_null());
    REDOX_CHECK_EQ(r["tags"].count(), 4u);
    REDOX_CHECK_EQ(r["tags"][3].get_string(), "dox");

    // Binary & Timestamp round-trip in CBOR and MessagePack
    auto mem = MemoryDocument::create_object();
    auto& root = mem->root_element().as_object();
    root.add("neg", -123456);
    root.add("raw", std::vector<uint8_t>{1, 2, 3, 4, 5});
    Timestamp ts;
    REDOX_CHECK(Timestamp::try_parse_iso8601("2026-01-15T12:00:00Z", ts));
    root.add("when", ts);

    auto c2 = CborDocument::parse(mem->root_element().to_cbor());
    REDOX_CHECK_EQ(c2->root_element()["neg"].get_int64(), -123456);
    REDOX_CHECK_EQ(c2->root_element()["raw"].get_bytes().size(), 5u);
    REDOX_CHECK_EQ(c2->root_element()["when"].get_timestamp().unix_seconds, ts.unix_seconds);

    auto m2 = MessagePackDocument::parse(mem->root_element().to_messagepack());
    REDOX_CHECK_EQ(m2->root_element()["neg"].get_int64(), -123456);
    REDOX_CHECK_EQ(m2->root_element()["raw"].get_bytes().size(), 5u);
    REDOX_CHECK_EQ(m2->root_element()["when"].get_timestamp().unix_seconds, ts.unix_seconds);
}

static void test_ini_roundtrip_and_comments() {
    const std::string ini_in = R"(
; Global engine settings
title = REX Engine

; Graphics section
[graphics]
width = 1920
height = 1080
fullscreen = true
)";

    auto doc = IniDocument::parse(ini_in);
    REDOX_CHECK_EQ(doc->root_element()["title"].get_string(), "REX Engine");
    REDOX_CHECK_EQ(doc->root_element()["graphics"]["width"].get_int64(), 1920);
    REDOX_CHECK_EQ(doc->root_element()["graphics"]["fullscreen"].get_bool(), true);

    doc->root_element()["graphics"].as_object().set("width", 2560);
    std::string ini_out = doc->root_element().to_ini_string();
    REDOX_CHECK(ini_out.find("; Graphics section") != std::string::npos);

    auto reparsed = IniDocument::parse(ini_out);
    REDOX_CHECK_EQ(reparsed->root_element()["graphics"]["width"].get_int64(), 2560);
    REDOX_CHECK_EQ(reparsed->root_element()["graphics"]["height"].get_int64(), 1080);
}

int main() {
    test_cross_format_pipeline();
    test_ini_roundtrip_and_comments();
    return redox::test::finish("test_formats");
}

// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "redox/document.hpp"
#include "redox/token.hpp"
#include "redox/trivia.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace redox {

struct JsonWriteOptions;
struct Json5WriteOptions;
struct DoxWriteOptions;
struct CborWriteOptions;
struct MessagePackWriteOptions;
struct IniWriteOptions;

// Lightweight non-owning handle to a node inside a Document.
class DElement {
public:
    DElement() noexcept = default;
    DElement(Document* doc, uint32_t id) noexcept : doc_(doc), id_(id) {}

    bool is_valid() const noexcept { return doc_ != nullptr && id_ > 0; }
    explicit operator bool() const noexcept { return is_valid(); }

    Document* document() const noexcept { return doc_; }
    uint32_t id() const noexcept { return id_; }
    DToken token() const;

    DTokenType type() const;
    DTokenKind kind() const;
    DTokenVariant variant() const;

    bool is_null() const;
    bool is_bool() const;
    bool is_integer() const;
    bool is_float() const;
    bool is_number() const;
    bool is_string() const;
    bool is_bytes() const;
    bool is_timestamp() const;
    bool is_array() const;
    bool is_map() const;
    bool is_object() const { return is_map(); }

    // Throwing getters
    bool get_bool() const;
    int64_t get_int64() const;
    uint64_t get_uint64() const;
    double get_double() const;
    std::string get_string() const;
    std::vector<uint8_t> get_bytes() const;
    Timestamp get_timestamp() const;

    // Non-throwing getters
    bool try_get_bool(bool& out) const noexcept;
    bool try_get_int64(int64_t& out) const noexcept;
    bool try_get_uint64(uint64_t& out) const noexcept;
    bool try_get_double(double& out) const noexcept;
    bool try_get_string(std::string& out) const noexcept;
    bool try_get_bytes(std::vector<uint8_t>& out) const noexcept;
    bool try_get_timestamp(Timestamp& out) const noexcept;

    // Container access
    size_t count() const;
    DElement operator[](size_t index) const;
    DElement operator[](std::string_view key) const;
    bool try_get_property(std::string_view key, DElement& out) const;
    bool contains_key(std::string_view key) const;

    std::vector<DElement> array_elements() const;
    std::vector<std::pair<std::string, DElement>> object_properties() const;
    std::vector<std::pair<DElement, DElement>> map_entries() const;

    // Mutable container upgrades (materializes tape container on demand)
    DObject& as_object() const;
    DArray& as_array() const;
    DMap& as_map() const;

    // Trivia access
    std::vector<DTrivia> leading_trivia() const;
    std::vector<DTrivia> trailing_trivia() const;

    // Format encoders
    std::string to_json_string() const;
    std::string to_json_string(const JsonWriteOptions& options) const;
    std::string to_json5_string() const;
    std::string to_json5_string(const Json5WriteOptions& options) const;
    std::vector<uint8_t> to_dox() const;
    std::vector<uint8_t> to_cbor() const;
    std::vector<uint8_t> to_messagepack() const;
    std::string to_ini_string() const;

    bool operator==(const DElement& other) const noexcept {
        return doc_ == other.doc_ && id_ == other.id_;
    }

private:
    Document* doc_ = nullptr;
    uint32_t id_ = 0;
};

// Owning generic value used for convenient insertion/assignment into mutable containers.
class DValue {
public:
    using Storage = std::variant<
        std::monostate,
        bool,
        int64_t,
        uint64_t,
        double,
        std::string,
        std::vector<uint8_t>,
        Timestamp,
        DElement
    >;

    DValue() noexcept : data_(std::monostate{}) {}
    DValue(std::nullptr_t) noexcept : data_(std::monostate{}) {}
    DValue(bool v) noexcept : data_(v) {}
    DValue(int32_t v) noexcept : data_(static_cast<int64_t>(v)) {}
    DValue(uint32_t v) noexcept : data_(static_cast<uint64_t>(v)) {}
    DValue(int64_t v) noexcept : data_(v) {}
    DValue(uint64_t v) noexcept : data_(v) {}
    DValue(float v) noexcept : data_(static_cast<double>(v)) {}
    DValue(double v) noexcept : data_(v) {}
    DValue(const char* v) : data_(std::string(v ? v : "")) {}
    DValue(std::string_view v) : data_(std::string(v)) {}
    DValue(std::string v) noexcept : data_(std::move(v)) {}
    DValue(std::vector<uint8_t> v) noexcept : data_(std::move(v)) {}
    DValue(Timestamp v) noexcept : data_(v) {}
    DValue(DElement v) noexcept : data_(v) {}

    const Storage& storage() const noexcept { return data_; }
    bool is_null() const noexcept { return std::holds_alternative<std::monostate>(data_); }

    static DValue make_object_marker();
    static DValue make_array_marker();

    bool is_empty_object_marker() const noexcept { return marker_ == Marker::EmptyObject; }
    bool is_empty_array_marker() const noexcept { return marker_ == Marker::EmptyArray; }

private:
    enum class Marker : uint8_t { None, EmptyObject, EmptyArray };
    Storage data_;
    Marker marker_ = Marker::None;
};

} // namespace redox

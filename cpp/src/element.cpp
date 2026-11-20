// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#include "redox/element.hpp"
#include "redox/cbor.hpp"
#include "redox/container.hpp"
#include "redox/dox.hpp"
#include "redox/ini.hpp"
#include "redox/json.hpp"
#include "redox/messagepack.hpp"

#include <stdexcept>

namespace redox {

DToken DElement::token() const {
    if (!is_valid()) throw std::runtime_error("Invalid DElement");
    return doc_->token_at(id_);
}

DTokenType DElement::type() const { return token().type(); }
DTokenKind DElement::kind() const { return token().kind(); }
DTokenVariant DElement::variant() const { return token().variant(); }

bool DElement::is_null() const { return is_valid() && kind() == DTokenKind::Null; }
bool DElement::is_bool() const { return is_valid() && kind() == DTokenKind::Boolean; }
bool DElement::is_integer() const { return is_valid() && kind() == DTokenKind::Integer; }
bool DElement::is_float() const {
    if (!is_valid()) return false;
    auto k = kind();
    return k == DTokenKind::Float || k == DTokenKind::InlineFloat;
}
bool DElement::is_number() const { return is_valid() && token().is_numeric(); }
bool DElement::is_string() const {
    if (!is_valid()) return false;
    auto k = kind();
    return k == DTokenKind::String || k == DTokenKind::Symbol;
}
bool DElement::is_bytes() const { return is_valid() && kind() == DTokenKind::ByteString; }
bool DElement::is_timestamp() const { return is_valid() && kind() == DTokenKind::Timestamp; }
bool DElement::is_array() const { return is_valid() && type() == DTokenType::Array; }
bool DElement::is_map() const { return is_valid() && type() == DTokenType::Map; }

bool DElement::get_bool() const { return doc_->resolve_bool(id_); }
int64_t DElement::get_int64() const { return doc_->resolve_int64(id_); }
uint64_t DElement::get_uint64() const { return doc_->resolve_uint64(id_); }
double DElement::get_double() const { return doc_->resolve_double(id_); }
std::string DElement::get_string() const { return doc_->resolve_string(id_); }
std::vector<uint8_t> DElement::get_bytes() const { return doc_->resolve_bytes(id_); }
Timestamp DElement::get_timestamp() const { return doc_->resolve_timestamp(id_); }

bool DElement::try_get_bool(bool& out) const noexcept {
    if (!is_bool()) return false;
    try { out = get_bool(); return true; } catch (...) { return false; }
}

bool DElement::try_get_int64(int64_t& out) const noexcept {
    if (!is_number()) return false;
    try { out = get_int64(); return true; } catch (...) { return false; }
}

bool DElement::try_get_uint64(uint64_t& out) const noexcept {
    if (!is_integer()) return false;
    try { out = get_uint64(); return true; } catch (...) { return false; }
}

bool DElement::try_get_double(double& out) const noexcept {
    if (!is_number()) return false;
    try { out = get_double(); return true; } catch (...) { return false; }
}

bool DElement::try_get_string(std::string& out) const noexcept {
    if (!is_string()) return false;
    try { out = get_string(); return true; } catch (...) { return false; }
}

bool DElement::try_get_bytes(std::vector<uint8_t>& out) const noexcept {
    if (!is_bytes()) return false;
    try { out = get_bytes(); return true; } catch (...) { return false; }
}

bool DElement::try_get_timestamp(Timestamp& out) const noexcept {
    if (!is_timestamp()) return false;
    try { out = get_timestamp(); return true; } catch (...) { return false; }
}

size_t DElement::count() const {
    return doc_->container_count(id_);
}

DElement DElement::operator[](size_t index) const {
    auto elems = doc_->array_elements(id_);
    if (index >= elems.size()) {
        throw std::out_of_range("Array index out of range");
    }
    return DElement(doc_, elems[index]);
}

bool DElement::try_get_property(std::string_view key, DElement& out) const {
    if (!is_map()) return false;
    auto entries = doc_->map_entries(id_);
    for (auto [kid, vid] : entries) {
        bool direct = false;
        auto sv = doc_->resolve_string_view_or_empty(kid, direct);
        if (direct) {
            if (sv == key) {
                out = DElement(doc_, vid);
                return true;
            }
        } else {
            if (doc_->resolve_string(kid) == key) {
                out = DElement(doc_, vid);
                return true;
            }
        }
    }
    return false;
}

bool DElement::contains_key(std::string_view key) const {
    DElement dummy;
    return try_get_property(key, dummy);
}

DElement DElement::operator[](std::string_view key) const {
    DElement out;
    if (!try_get_property(key, out)) {
        throw std::out_of_range("Property not found: " + std::string(key));
    }
    return out;
}

std::vector<DElement> DElement::array_elements() const {
    auto ids = doc_->array_elements(id_);
    std::vector<DElement> out;
    out.reserve(ids.size());
    for (uint32_t cid : ids) out.emplace_back(doc_, cid);
    return out;
}

std::vector<std::pair<std::string, DElement>> DElement::object_properties() const {
    auto entries = doc_->map_entries(id_);
    std::vector<std::pair<std::string, DElement>> out;
    out.reserve(entries.size());
    for (auto [kid, vid] : entries) {
        out.emplace_back(doc_->resolve_string(kid), DElement(doc_, vid));
    }
    return out;
}

std::vector<std::pair<DElement, DElement>> DElement::map_entries() const {
    auto entries = doc_->map_entries(id_);
    std::vector<std::pair<DElement, DElement>> out;
    out.reserve(entries.size());
    for (auto [kid, vid] : entries) {
        out.emplace_back(DElement(doc_, kid), DElement(doc_, vid));
    }
    return out;
}

DObject& DElement::as_object() const { return doc_->materialize_object(id_); }
DArray& DElement::as_array() const { return doc_->materialize_array(id_); }
DMap& DElement::as_map() const { return doc_->materialize_map(id_); }

std::vector<DTrivia> DElement::leading_trivia() const {
    std::vector<DTrivia> out;
    if (is_valid()) doc_->collect_leading_trivia(id_, out);
    return out;
}

std::vector<DTrivia> DElement::trailing_trivia() const {
    std::vector<DTrivia> out;
    if (is_valid()) doc_->collect_trailing_trivia(id_, out);
    return out;
}

std::string DElement::to_json_string() const {
    return JsonWriter::write_json(*this, JsonWriteOptions{});
}

std::string DElement::to_json_string(const JsonWriteOptions& options) const {
    return JsonWriter::write_json(*this, options);
}

std::string DElement::to_json5_string() const {
    return JsonWriter::write_json5(*this, Json5WriteOptions{});
}

std::string DElement::to_json5_string(const Json5WriteOptions& options) const {
    return JsonWriter::write_json5(*this, options);
}

std::vector<uint8_t> DElement::to_dox() const {
    return DoxDocument::encode(*this);
}

std::vector<uint8_t> DElement::to_cbor() const {
    return CborDocument::encode(*this);
}

std::vector<uint8_t> DElement::to_messagepack() const {
    return MessagePackDocument::encode(*this);
}

std::string DElement::to_ini_string() const {
    return IniDocument::encode(*this);
}

DValue DValue::make_object_marker() {
    DValue v;
    v.marker_ = Marker::EmptyObject;
    return v;
}

DValue DValue::make_array_marker() {
    DValue v;
    v.marker_ = Marker::EmptyArray;
    return v;
}

} // namespace redox

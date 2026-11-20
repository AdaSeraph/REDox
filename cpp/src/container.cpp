// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#include "redox/container.hpp"

#include <stdexcept>

namespace redox {

// ============================================================================
// DArray
// ============================================================================

DArray::DArray(Document* doc, uint32_t token_id, std::vector<uint32_t> element_ids)
    : doc_(doc), token_id_(token_id), items_(std::move(element_ids)) {}

DElement DArray::operator[](size_t index) const {
    if (index >= items_.size()) {
        throw std::out_of_range("DArray index out of range");
    }
    return DElement(doc_, items_[index]);
}

DElement DArray::add(const DValue& value) {
    uint32_t id = doc_->create_value_token(value);
    items_.push_back(id);
    return DElement(doc_, id);
}

DElement DArray::insert(size_t index, const DValue& value) {
    if (index > items_.size()) {
        throw std::out_of_range("DArray insert index out of range");
    }
    uint32_t id = doc_->create_value_token(value);
    items_.insert(items_.begin() + static_cast<std::ptrdiff_t>(index), id);
    return DElement(doc_, id);
}

DElement DArray::set(size_t index, const DValue& value) {
    if (index >= items_.size()) {
        throw std::out_of_range("DArray set index out of range");
    }
    uint32_t old_id = items_[index];
    std::vector<DTrivia> leading, trailing;
    doc_->collect_leading_trivia(old_id, leading);
    doc_->collect_trailing_trivia(old_id, trailing);

    uint32_t new_id = doc_->create_value_token(value);
    if (!leading.empty() || !trailing.empty()) {
        auto& tl = doc_->ensure_trivia(new_id);
        tl.leading = std::move(leading);
        tl.trailing = std::move(trailing);
    }
    items_[index] = new_id;
    doc_->free_token(old_id);
    return DElement(doc_, new_id);
}

void DArray::remove_at(size_t index) {
    if (index >= items_.size()) {
        throw std::out_of_range("DArray remove_at index out of range");
    }
    uint32_t old_id = items_[index];
    items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(index));
    doc_->free_token(old_id);
}

void DArray::clear() {
    for (uint32_t id : items_) {
        doc_->free_token(id);
    }
    items_.clear();
}

DObject& DArray::add_object() {
    uint32_t id = doc_->create_empty_object_token();
    items_.push_back(id);
    return doc_->materialize_object(id);
}

DArray& DArray::add_array() {
    uint32_t id = doc_->create_empty_array_token();
    items_.push_back(id);
    return doc_->materialize_array(id);
}

// ============================================================================
// DObject
// ============================================================================

DObject::DObject(Document* doc, uint32_t token_id, std::vector<Entry> entries)
    : doc_(doc), token_id_(token_id), entries_(std::move(entries)) {}

size_t DObject::find_index(std::string_view key) const {
    for (size_t i = 0; i < entries_.size(); ++i) {
        bool direct = false;
        auto sv = doc_->resolve_string_view_or_empty(entries_[i].key_id, direct);
        if (direct) {
            if (sv == key) return i;
        } else {
            if (doc_->resolve_string(entries_[i].key_id) == key) return i;
        }
    }
    return entries_.size();
}

bool DObject::contains_key(std::string_view key) const {
    return find_index(key) < entries_.size();
}

bool DObject::try_get(std::string_view key, DElement& out) const {
    size_t idx = find_index(key);
    if (idx >= entries_.size()) return false;
    out = DElement(doc_, entries_[idx].value_id);
    return true;
}

DElement DObject::operator[](std::string_view key) const {
    DElement out;
    if (!try_get(key, out)) {
        throw std::out_of_range("DObject key not found: " + std::string(key));
    }
    return out;
}

DElement DObject::set(std::string_view key, const DValue& value) {
    size_t idx = find_index(key);
    if (idx < entries_.size()) {
        uint32_t old_val = entries_[idx].value_id;
        std::vector<DTrivia> leading, trailing;
        doc_->collect_leading_trivia(old_val, leading);
        doc_->collect_trailing_trivia(old_val, trailing);

        uint32_t new_val = doc_->create_value_token(value);
        if (!leading.empty() || !trailing.empty()) {
            auto& tl = doc_->ensure_trivia(new_val);
            tl.leading = std::move(leading);
            tl.trailing = std::move(trailing);
        }
        entries_[idx].value_id = new_val;
        doc_->free_token(old_val);
        return DElement(doc_, new_val);
    }
    uint32_t kid = doc_->create_string_token(std::string(key));
    uint32_t vid = doc_->create_value_token(value);
    entries_.push_back({kid, vid});
    return DElement(doc_, vid);
}

DElement DObject::add(std::string_view key, const DValue& value) {
    return set(key, value);
}

bool DObject::remove(std::string_view key) {
    size_t idx = find_index(key);
    if (idx >= entries_.size()) return false;
    uint32_t kid = entries_[idx].key_id;
    uint32_t vid = entries_[idx].value_id;
    entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(idx));
    doc_->free_token(kid);
    doc_->free_token(vid);
    return true;
}

void DObject::clear() {
    for (const auto& e : entries_) {
        doc_->free_token(e.key_id);
        doc_->free_token(e.value_id);
    }
    entries_.clear();
}

DObject& DObject::add_object(std::string_view key) {
    DElement elem = set(key, DValue::make_object_marker());
    return elem.as_object();
}

DArray& DObject::add_array(std::string_view key) {
    DElement elem = set(key, DValue::make_array_marker());
    return elem.as_array();
}

// ============================================================================
// DMap
// ============================================================================

DMap::DMap(Document* doc, uint32_t token_id, std::vector<Entry> entries)
    : doc_(doc), token_id_(token_id), entries_(std::move(entries)) {}

void DMap::add(const DValue& key, const DValue& value) {
    uint32_t kid = doc_->create_value_token(key);
    uint32_t vid = doc_->create_value_token(value);
    entries_.push_back({kid, vid});
}

void DMap::remove_at(size_t index) {
    if (index >= entries_.size()) {
        throw std::out_of_range("DMap remove_at index out of range");
    }
    uint32_t kid = entries_[index].key_id;
    uint32_t vid = entries_[index].value_id;
    entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(index));
    doc_->free_token(kid);
    doc_->free_token(vid);
}

void DMap::clear() {
    for (const auto& e : entries_) {
        doc_->free_token(e.key_id);
        doc_->free_token(e.value_id);
    }
    entries_.clear();
}

} // namespace redox

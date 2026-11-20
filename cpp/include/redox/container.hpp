// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "redox/element.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace redox {

class DArray {
public:
    DArray(Document* doc, uint32_t token_id, std::vector<uint32_t> element_ids);

    Document* document() const noexcept { return doc_; }
    uint32_t token_id() const noexcept { return token_id_; }
    size_t count() const noexcept { return items_.size(); }
    bool empty() const noexcept { return items_.empty(); }

    DElement operator[](size_t index) const;
    const std::vector<uint32_t>& item_ids() const noexcept { return items_; }

    DElement add(const DValue& value);
    DElement insert(size_t index, const DValue& value);
    DElement set(size_t index, const DValue& value);
    void remove_at(size_t index);
    void clear();

    DObject& add_object();
    DArray& add_array();

private:
    Document* doc_;
    uint32_t token_id_;
    std::vector<uint32_t> items_;
};

class DObject {
public:
    struct Entry {
        uint32_t key_id;
        uint32_t value_id;
    };

    DObject(Document* doc, uint32_t token_id, std::vector<Entry> entries);

    Document* document() const noexcept { return doc_; }
    uint32_t token_id() const noexcept { return token_id_; }
    size_t count() const noexcept { return entries_.size(); }
    bool empty() const noexcept { return entries_.empty(); }

    const std::vector<Entry>& entries() const noexcept { return entries_; }

    bool contains_key(std::string_view key) const;
    bool try_get(std::string_view key, DElement& out) const;
    DElement operator[](std::string_view key) const;

    // Replace existing key's value or append if not found
    DElement set(std::string_view key, const DValue& value);
    // Append a new key-value pair (replaces if key already exists)
    DElement add(std::string_view key, const DValue& value);
    bool remove(std::string_view key);
    void clear();

    DObject& add_object(std::string_view key);
    DArray& add_array(std::string_view key);

private:
    size_t find_index(std::string_view key) const;

    Document* doc_;
    uint32_t token_id_;
    std::vector<Entry> entries_;
};

class DMap {
public:
    struct Entry {
        uint32_t key_id;
        uint32_t value_id;
    };

    DMap(Document* doc, uint32_t token_id, std::vector<Entry> entries);

    Document* document() const noexcept { return doc_; }
    uint32_t token_id() const noexcept { return token_id_; }
    size_t count() const noexcept { return entries_.size(); }
    bool empty() const noexcept { return entries_.empty(); }

    const std::vector<Entry>& entries() const noexcept { return entries_; }

    void add(const DValue& key, const DValue& value);
    void remove_at(size_t index);
    void clear();

private:
    Document* doc_;
    uint32_t token_id_;
    std::vector<Entry> entries_;
};

} // namespace redox

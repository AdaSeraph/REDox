// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "redox/token.hpp"
#include "redox/trivia.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace redox {

class DElement;
class DValue;
class DObject;
class DArray;
class DMap;

class DocumentParseException : public std::runtime_error {
public:
    DocumentParseException(const std::string& message, size_t offset = 0, size_t line = 0, size_t column = 0)
        : std::runtime_error(message), offset_(offset), line_(line), column_(column) {}

    size_t offset() const noexcept { return offset_; }
    size_t line() const noexcept { return line_; }
    size_t column() const noexcept { return column_; }

private:
    size_t offset_;
    size_t line_;
    size_t column_;
};

class FormatException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct Timestamp {
    int64_t unix_seconds = 0;
    int32_t nanoseconds = 0;
    int32_t offset_minutes = 0; // UTC offset in minutes
    bool has_offset = false;
    TimestampKind kind = TimestampKind::Default;

    bool operator==(const Timestamp& other) const noexcept = default;

    std::string to_iso8601() const;
    static bool try_parse_iso8601(std::string_view text, Timestamp& out) noexcept;
};

// Polymorphic storage slot in Document::extends_ (replaces C# object?[] with zero GC).
using ExtendValue = std::variant<
    std::monostate,
    std::string,
    std::vector<uint8_t>,
    int64_t,
    uint64_t,
    double,
    Timestamp,
    std::unique_ptr<DObject>,
    std::unique_ptr<DArray>,
    std::unique_ptr<DMap>,
    TriviaList
>;

class Document {
public:
    Document();
    virtual ~Document();

    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;
    Document(Document&&) noexcept = default;
    Document& operator=(Document&&) noexcept = default;

    bool is_valid() const noexcept { return root_id_ > 0 && root_id_ < tokens_.size(); }
    uint32_t root_id() const noexcept { return root_id_; }
    uint32_t version() const noexcept { return version_; }

    DElement root_element() const noexcept;
    std::span<const DToken> tokens() const noexcept { return tokens_; }

    // Create a deep independent snapshot copy of this document.
    std::unique_ptr<Document> duplicate() const;

    // --- Internal / low-level token & extend pool API ---
    DToken token_at(uint32_t id) const;
    void set_token(uint32_t id, DToken token);

    uint32_t alloc_token(DToken token);
    uint32_t alloc_empty_token(DToken token);
    void free_token(uint32_t token_id);

    uint32_t alloc_extend(ExtendValue value);
    void free_extend(uint32_t extend_id);
    ExtendValue& extend_at(uint32_t extend_id);
    const ExtendValue& extend_at(uint32_t extend_id) const;

    // Value creation helpers in this document
    uint32_t create_value_token(const DValue& value);
    uint32_t create_null_token(NullKind kind = NullKind::Default);
    uint32_t create_bool_token(bool value);
    uint32_t create_int64_token(int64_t value, IntegerKind kind = IntegerKind::Default);
    uint32_t create_uint64_token(uint64_t value);
    uint32_t create_double_token(double value, FloatKind kind = FloatKind::Default);
    uint32_t create_string_token(std::string value, StringKind kind = StringKind::Default);
    uint32_t create_bytes_token(std::vector<uint8_t> bytes, ByteStringKind kind = ByteStringKind::Default);
    uint32_t create_timestamp_token(Timestamp ts);
    uint32_t create_empty_object_token();
    uint32_t create_empty_array_token();
    uint32_t create_empty_map_token();

    // Clone a subtree from another (or the same) document into this document.
    uint32_t clone_subtree(const Document& src, uint32_t src_token_id);

    // Resolve value from any token id (extended or format source-backed)
    bool resolve_bool(uint32_t token_id) const;
    int64_t resolve_int64(uint32_t token_id) const;
    uint64_t resolve_uint64(uint32_t token_id) const;
    double resolve_double(uint32_t token_id) const;
    std::string resolve_string(uint32_t token_id) const;
    std::string_view resolve_string_view_or_empty(uint32_t token_id, bool& is_direct_view) const;
    std::vector<uint8_t> resolve_bytes(uint32_t token_id) const;
    Timestamp resolve_timestamp(uint32_t token_id) const;

    // Trivia management
    TriviaList* get_trivia_mut(uint32_t token_id);
    const TriviaList* get_trivia(uint32_t token_id) const;
    TriviaList& ensure_trivia(uint32_t token_id);
    void collect_leading_trivia(uint32_t token_id, std::vector<DTrivia>& out) const;
    void collect_trailing_trivia(uint32_t token_id, std::vector<DTrivia>& out) const;

    // Container traversal (works for both tape containers and materialized extended containers)
    size_t container_count(uint32_t token_id) const;
    std::vector<uint32_t> array_elements(uint32_t token_id) const;
    std::vector<std::pair<uint32_t, uint32_t>> map_entries(uint32_t token_id) const;

    // Container materialization (upgrades tape container to mutable extended container on first edit)
    DObject& materialize_object(uint32_t token_id);
    DArray& materialize_array(uint32_t token_id);
    DMap& materialize_map(uint32_t token_id);

    // Tape navigation helpers
    uint32_t next_token_id(uint32_t token_id) const;
    uint32_t skip_trivia(uint32_t token_id) const;
    uint32_t parent_of(uint32_t token_id) const;

    // Format-specific decoding hooks (overridden by JsonDocument, Json5Document, DoxDocument, etc.)
    virtual std::string decode_string(DToken token) const;
    virtual std::string_view decode_raw_utf8(DToken token, bool& ok) const;
    virtual int64_t decode_integer(DToken token) const;
    virtual uint64_t decode_uinteger(DToken token) const;
    virtual double decode_float(DToken token) const;
    virtual std::vector<uint8_t> decode_bytes(DToken token) const;
    virtual Timestamp decode_timestamp(DToken token) const;
    virtual std::string decode_trivia(DToken token) const;

protected:
    void reset();
    void set_root_id(uint32_t id) noexcept { root_id_ = id; }
    void invalidate_parent_table() noexcept;
    void build_parent_table() const;

    std::vector<DToken> tokens_;
    std::vector<ExtendValue> extends_;
    uint32_t root_id_ = 0;
    uint32_t empty_token_id_ = 0;
    uint32_t empty_extend_id_ = 0;
    uint32_t version_ = 0;
    mutable std::vector<uint32_t> parent_table_;
};

// Concrete in-memory document for constructing or mutating trees directly in C++.
class MemoryDocument final : public Document {
public:
    MemoryDocument();
    static std::unique_ptr<MemoryDocument> create_object();
    static std::unique_ptr<MemoryDocument> create_array();
    static std::unique_ptr<MemoryDocument> create_map();
    static std::unique_ptr<MemoryDocument> create_value(const DValue& value);
};

} // namespace redox

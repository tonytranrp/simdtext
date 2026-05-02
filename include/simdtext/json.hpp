#pragma once

/// @file json.hpp
/// @brief Zero-allocation JSON tokenization.

#include "export.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace simdtext {

/// JSON token type.
enum class JsonType : uint8_t {
    Null,
    True,
    False,
    Number,
    String,
    ArrayOpen,    // [
    ArrayClose,   // ]
    ObjectOpen,   // {
    ObjectClose,  // }
    Colon,        // :
    Comma,        // ,
    Error,
    End,
};

/// A JSON token — type + value (string_view into original buffer).
struct JsonToken {
    JsonType type = JsonType::Error;
    std::string_view value;    // Slice of the original input
    size_t offset = 0;         // Byte offset in original input
};

/// Zero-allocation JSON tokenizer.
/// Usage:
///   JsonTokenizer tok(json_string);
///   while (tok.next().type != JsonType::End) {
///       auto& t = tok.token();
///       ...
///   }
class SIMDTEXT_API JsonTokenizer {
public:
    explicit JsonTokenizer(std::string_view input) noexcept;

    /// Advance to the next token. Returns the token.
    SIMDTEXT_NODISCARD const JsonToken& next() noexcept;

    /// Get the current token.
    SIMDTEXT_NODISCARD const JsonToken& token() const noexcept { return token_; }

    /// Check if there was an error.
    SIMDTEXT_NODISCARD bool has_error() const noexcept { return token_.type == JsonType::Error; }

    /// Reset the tokenizer.
    void reset(std::string_view input) noexcept;

    /// Get the remaining unprocessed input.
    SIMDTEXT_NODISCARD std::string_view remaining() const noexcept;

private:
    std::string_view input_;
    size_t pos_;
    JsonToken token_;

    void skip_whitespace() noexcept;
    char peek() const noexcept;
    char advance() noexcept;
};

/// Quick check if a string looks like valid JSON (starts with { or [).
SIMDTEXT_NODISCARD SIMDTEXT_API bool looks_like_json(std::string_view input) noexcept;

/// Quick check if a string is valid JSON number.
SIMDTEXT_NODISCARD SIMDTEXT_API bool is_json_number(std::string_view input) noexcept;

/// Unescape a JSON string in-place. Returns the length of the unescaped string.
/// The output buffer must be at least as large as the input.
SIMDTEXT_API size_t json_unescape_inplace(char* data, size_t len) noexcept;

} // namespace simdtext

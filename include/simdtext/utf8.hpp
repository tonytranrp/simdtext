#pragma once

/// @file utf8.hpp
/// @brief UTF-8 validation and processing.

#include "export.hpp"
#include <cstddef>
#include <cstdint>
#include <span>

namespace simdtext {

/// Validate UTF-8 encoding.
SIMDTEXT_NODISCARD SIMDTEXT_API bool valid_utf8(std::span<const char> input);

/// Streaming UTF-8 validator for chunked input.
/// Handles state across chunk boundaries for multi-byte sequences.
class SIMDTEXT_API Utf8Validator {
public:
    Utf8Validator() : state_(0) {}

    /// Validate a chunk. Returns false if invalid UTF-8 is found.
    /// Call finalize() after the last chunk to check trailing state.
    SIMDTEXT_NODISCARD bool validate(std::string_view chunk) noexcept;

    /// Finalize validation. Returns false if a multi-byte sequence was incomplete.
    SIMDTEXT_NODISCARD bool finalize() noexcept;

    /// Reset validator state for reuse.
    void reset() noexcept { state_ = 0; }

private:
    uint8_t state_;  // 0 = expecting new sequence, 1-3 = continuation bytes remaining
};

/// Count the number of Unicode code points in valid UTF-8.
SIMDTEXT_NODISCARD SIMDTEXT_API size_t count_code_points(std::string_view input) noexcept;

/// Count the number of UTF-8 characters (same as count_code_points).
SIMDTEXT_NODISCARD SIMDTEXT_API size_t utf8_length(std::string_view input) noexcept;

} // namespace simdtext

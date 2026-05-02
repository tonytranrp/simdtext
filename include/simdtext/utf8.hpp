#pragma once

/// @file utf8.hpp
/// @brief UTF-8 validation and processing.

#include "export.hpp"
#include <cstddef>
#include <cstdint>
#include <span>

namespace simdtext {

/// Validate UTF-8 encoding.
/// @param input The byte buffer to validate.
/// @return true if all bytes form valid UTF-8.
/// @note SIMD-accelerated via Highway. ASCII-only: ~14 GB/s. Mixed UTF-8: ~10 GB/s.
SIMDTEXT_NODISCARD SIMDTEXT_API bool valid_utf8(std::span<const char> input);

/// UTF-8 validation result with error location.
struct Utf8Result {
    bool valid = true;           ///< true if valid UTF-8
    size_t error_offset = 0;     ///< Byte offset of first error (0 if valid)
    uint8_t error_byte = 0;      ///< The invalid byte at error_offset
    std::string_view error_desc; ///< Human-readable error description
};

/// Validate UTF-8 and return detailed error information.
/// @param input The string to validate.
/// @return Utf8Result with valid flag, error offset, error byte, and description.
SIMDTEXT_NODISCARD SIMDTEXT_API Utf8Result validate_utf8_detailed(std::string_view input) noexcept;

/// Streaming UTF-8 validator for chunked input.
/// Handles state across chunk boundaries for multi-byte sequences.
class SIMDTEXT_API Utf8Validator {
public:
    Utf8Validator() : state_(0), saved_lead_(0) {}

    /// Validate a chunk. Returns false if invalid UTF-8 is found.
    /// Call finalize() after the last chunk to check trailing state.
    SIMDTEXT_NODISCARD bool validate(std::string_view chunk) noexcept;

    /// Finalize validation. Returns false if a multi-byte sequence was incomplete.
    SIMDTEXT_NODISCARD bool finalize() noexcept;

    /// Reset validator state for reuse.
    void reset() noexcept { state_ = 0; saved_lead_ = 0; }

private:
    uint8_t state_;      // 0 = expecting new sequence, 1-3 = continuation bytes remaining
    uint8_t saved_lead_; // Lead byte of current multi-byte sequence (for overlong/surrogate checks)
};

/// Count the number of Unicode code points in valid UTF-8.
/// @param input Valid UTF-8 string. Behavior is undefined if input is invalid UTF-8.
/// @return Number of Unicode code points (not bytes).
/// @note SIMD-accelerated. Counts non-continuation bytes.
SIMDTEXT_NODISCARD SIMDTEXT_API size_t count_code_points(std::string_view input) noexcept;

/// Count the number of UTF-8 characters (same as count_code_points).
/// @param input Valid UTF-8 string.
/// @return Number of Unicode code points.
SIMDTEXT_NODISCARD SIMDTEXT_API size_t utf8_length(std::string_view input) noexcept;

} // namespace simdtext

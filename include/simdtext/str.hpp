#pragma once

/// @file str.hpp
/// @brief String utility functions with SIMD acceleration.

#include "export.hpp"
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace simdtext {

// ── Trim ────────────────────────────────────────────────────

/// Trim leading whitespace (space, tab, newline, CR, formfeed, vertical tab).
SIMDTEXT_NODISCARD SIMDTEXT_API std::string_view trim_left(std::string_view s) noexcept;

/// Trim trailing whitespace.
SIMDTEXT_NODISCARD SIMDTEXT_API std::string_view trim_right(std::string_view s) noexcept;

/// Trim both leading and trailing whitespace.
SIMDTEXT_NODISCARD SIMDTEXT_API std::string_view trim(std::string_view s) noexcept;

// ── Replace ─────────────────────────────────────────────────

/// Replace all occurrences of `needle` with `replacement`.
/// Returns a new string. Uses SIMD-accelerated find_byte for single-char needles.
SIMDTEXT_NODISCARD SIMDTEXT_API std::string replace_all(
    std::string_view input, char needle, char replacement);

/// Replace all occurrences of `needle` substring with `replacement`.
SIMDTEXT_NODISCARD SIMDTEXT_API std::string replace_all(
    std::string_view input, std::string_view needle, std::string_view replacement);

// ── Field splitting ──────────────────────────────────────────

/// Split by any whitespace, skipping empty segments (like Python's str.split()).
/// Returns a vector of non-empty string_views.
SIMDTEXT_NODISCARD SIMDTEXT_API std::vector<std::string_view> fields(std::string_view input);

/// Split by delimiter, including empty segments (like Python's str.split(sep)).
SIMDTEXT_NODISCARD SIMDTEXT_API std::vector<std::string_view> split_vec(
    std::string_view input, char delimiter);

/// Split by delimiter, including empty segments, into a pre-allocated vector.
/// Returns the number of segments written.
SIMDTEXT_API size_t split_into(
    std::string_view input, char delimiter,
    std::string_view* out, size_t capacity) noexcept;

// ── Starts/Ends With ────────────────────────────────────────

/// Check if input starts with prefix.
SIMDTEXT_NODISCARD SIMDTEXT_API bool starts_with(std::string_view input, std::string_view prefix) noexcept;

/// Check if input ends with suffix.
SIMDTEXT_NODISCARD SIMDTEXT_API bool ends_with(std::string_view input, std::string_view suffix) noexcept;

// ── Contains ────────────────────────────────────────────────

/// Check if input contains the given character. SIMD-accelerated.
SIMDTEXT_NODISCARD SIMDTEXT_API bool contains_char(std::string_view input, char needle) noexcept;

} // namespace simdtext

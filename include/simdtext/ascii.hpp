#pragma once

/// @file ascii.hpp
/// @brief ASCII classification and case conversion operations.
///
/// All functions are SIMD-accelerated where available.
/// Case conversion operates in-place for zero allocation.

#include "export.hpp"
#include <span>
#include <string_view>

namespace simdtext {

/// Check if all bytes in the input are ASCII (0x00–0x7F).
/// @param input The byte buffer to check.
/// @return true if every byte has its high bit clear.
/// @note SIMD-accelerated. Throughput: ~14 GB/s on AVX2.
SIMDTEXT_NODISCARD SIMDTEXT_API bool is_ascii(std::span<const char> input);

/// Lowercase ASCII bytes in-place (A–Z → a–z). Non-ASCII bytes unchanged.
/// @param input The buffer to modify in-place.
/// @note SIMD-accelerated. Operates in-place, zero allocation.
SIMDTEXT_API void lowercase_ascii_inplace(std::span<char> input);

/// Uppercase ASCII bytes in-place (a–z → A–Z). Non-ASCII bytes unchanged.
/// @param input The buffer to modify in-place.
/// @note SIMD-accelerated. Operates in-place, zero allocation.
SIMDTEXT_API void uppercase_ascii_inplace(std::span<char> input);

/// Trim leading and trailing ASCII whitespace (space, tab, CR, LF).
/// @param input The string to trim.
/// @return A string_view into the original buffer with whitespace removed.
/// @note Zero-allocation. The returned view is valid as long as `input` is.
SIMDTEXT_NODISCARD SIMDTEXT_API std::string_view trim_ascii(std::string_view input);

} // namespace simdtext

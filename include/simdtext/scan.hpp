#pragma once

/// @file scan.hpp
/// @brief Fast scanning operations: counting, searching, finding.
///
/// All functions are SIMD-accelerated where available (SSE2, AVX2, AVX-512,
/// NEON, Highway) with SWAR scalar fallback. Hot paths are zero-allocation.

#include "export.hpp"
#include <cstddef>
#include <span>
#include <string_view>

namespace simdtext {

/// Count occurrences of a byte in the input buffer.
/// @param input The byte buffer to scan.
/// @param byte The byte value to count.
/// @return Number of occurrences of `byte` in `input`.
/// @note SIMD-accelerated. Throughput: ~12 GB/s on AVX2 for large buffers.
SIMDTEXT_NODISCARD SIMDTEXT_API size_t count_byte(std::span<const char> input, char byte);

/// Count newline characters ('\n') in the input buffer.
/// @param input The byte buffer to scan.
/// @return Number of '\n' characters in `input`.
/// @note Equivalent to `count_byte(input, '\n')` but may be optimized separately.
SIMDTEXT_NODISCARD SIMDTEXT_API size_t count_newlines(std::span<const char> input);

/// Check if the input contains the given needle.
/// @param input The haystack to search.
/// @param needle The substring to find.
/// @return true if `needle` is found within `input`.
/// @note Uses SIMD-accelerated find_byte for single-char needles.
SIMDTEXT_NODISCARD SIMDTEXT_API bool contains(std::string_view input, std::string_view needle);

/// Find the first occurrence of a byte in [begin, end).
/// @param begin Start of the range (inclusive).
/// @param end End of the range (exclusive).
/// @param byte The byte to find.
/// @return Pointer to the first occurrence, or `end` if not found.
SIMDTEXT_NODISCARD SIMDTEXT_API const char* find_byte(const char* begin, const char* end, char byte);

} // namespace simdtext

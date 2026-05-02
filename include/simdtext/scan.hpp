#pragma once

/// @file scan.hpp
/// @brief Fast scanning operations: counting, searching, finding.

#include "export.hpp"
#include <cstddef>
#include <span>
#include <string_view>

namespace simdtext {

/// Count occurrences of a byte in the input buffer.
SIMDTEXT_NODISCARD SIMDTEXT_API size_t count_byte(std::span<const char> input, char byte);

/// Count newline characters ('\n') in the input buffer.
SIMDTEXT_NODISCARD SIMDTEXT_API size_t count_newlines(std::span<const char> input);

/// Check if the input contains the given needle.
SIMDTEXT_NODISCARD SIMDTEXT_API bool contains(std::string_view input, std::string_view needle);

/// Find the first occurrence of a byte in [begin, end). Returns end if not found.
SIMDTEXT_NODISCARD SIMDTEXT_API const char* find_byte(const char* begin, const char* end, char byte);

} // namespace simdtext

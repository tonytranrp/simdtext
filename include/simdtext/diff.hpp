#pragma once

/// @file diff.hpp
/// @brief Text diffing and comparison utilities.

#include "export.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace simdtext {

/// A single diff operation.
struct DiffOp {
    enum Type : uint8_t {
        Equal,    // Same in both
        Insert,   // In b only
        Delete,   // In a only
    };
    Type type;
    std::string_view a_text;  // Text from a (for Equal/Delete)
    std::string_view b_text;  // Text from b (for Equal/Insert)
    size_t a_line;            // Line number in a (1-based)
    size_t b_line;            // Line number in b (1-based)
};

/// Line-by-line diff between two texts.
/// Uses a simple LCS (Longest Common Subsequence) algorithm.
SIMDTEXT_NODISCARD SIMDTEXT_API std::vector<DiffOp> line_diff(
    std::string_view a, std::string_view b);

/// Count the number of differing lines between two texts.
SIMDTEXT_NODISCARD SIMDTEXT_API size_t count_diff_lines(
    std::string_view a, std::string_view b);

/// Check if two texts are identical.
SIMDTEXT_NODISCARD SIMDTEXT_API bool text_equal(std::string_view a, std::string_view b) noexcept;

/// Find the first differing byte position. Returns the length of the common prefix.
SIMDTEXT_NODISCARD SIMDTEXT_API size_t common_prefix_length(
    std::string_view a, std::string_view b) noexcept;

/// Find the length of the common suffix.
SIMDTEXT_NODISCARD SIMDTEXT_API size_t common_suffix_length(
    std::string_view a, std::string_view b) noexcept;

} // namespace simdtext

#pragma once

/// @file ascii.hpp
/// @brief ASCII classification and case conversion operations.

#include <span>
#include <string_view>

namespace simdtext {

/// Check if all bytes in the input are ASCII (0x00–0x7F).
bool is_ascii(std::span<const char> input);

/// Lowercase ASCII bytes in-place (A–Z → a–z). Non-ASCII bytes unchanged.
void lowercase_ascii_inplace(std::span<char> input);

/// Uppercase ASCII bytes in-place (a–z → A–Z). Non-ASCII bytes unchanged.
void uppercase_ascii_inplace(std::span<char> input);

/// Trim leading and trailing ASCII whitespace (space, tab, CR, LF).
std::string_view trim_ascii(std::string_view input);

} // namespace simdtext

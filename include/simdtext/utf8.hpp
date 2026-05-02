#pragma once

/// @file utf8.hpp
/// @brief UTF-8 validation.

#include <cstddef>
#include <span>

namespace simdtext {

/// Validate UTF-8 encoding.
bool valid_utf8(std::span<const char> input);

} // namespace simdtext

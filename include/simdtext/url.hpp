#pragma once

/// @file url.hpp
/// @brief URL encoding/decoding and query string parsing.

#include "export.hpp"
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

namespace simdtext {

/// URL-encode a string. Returns bytes written, 0 on error.
SIMDTEXT_NODISCARD SIMDTEXT_API size_t url_encode_to(std::string_view input, std::span<char> output);

/// URL-encode a string.
SIMDTEXT_NODISCARD SIMDTEXT_API std::string url_encode(std::string_view input);

/// URL-decode a string. Returns bytes written, 0 on error.
SIMDTEXT_NODISCARD SIMDTEXT_API size_t url_decode_to(std::string_view input, std::span<char> output);

/// URL-decode a string.
SIMDTEXT_NODISCARD SIMDTEXT_API std::string url_decode(std::string_view input);

/// Parse a query string into key-value pairs.
/// e.g. "name=tony&age=18" → {"name": "tony", "age": "18"}
SIMDTEXT_NODISCARD SIMDTEXT_API std::unordered_map<std::string, std::string> parse_query(std::string_view query);

} // namespace simdtext

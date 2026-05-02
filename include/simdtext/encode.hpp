#pragma once

/// @file encode.hpp
/// @brief Hex and Base64 encoding/decoding operations.

#include "export.hpp"
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <simdtext/types.hpp>

namespace simdtext {

// ── Hex Encode/Decode ──────────────────────────────────────

/// Encode bytes to hexadecimal. Returns bytes written, 0 on error.
SIMDTEXT_NODISCARD SIMDTEXT_API size_t hex_encode_to(std::span<const std::byte> input, std::span<char> output);

/// Encode bytes to hexadecimal string.
SIMDTEXT_NODISCARD SIMDTEXT_API std::string hex_encode(std::span<const std::byte> input);

/// Decode hexadecimal to bytes.
SIMDTEXT_NODISCARD SIMDTEXT_API DecodeResult hex_decode_to(std::string_view input, std::span<std::byte> output);

/// Decode hexadecimal to char buffer.
SIMDTEXT_NODISCARD SIMDTEXT_API DecodeResult hex_decode_to(std::string_view input, std::span<char> output);

/// Decode hexadecimal string to byte vector.
SIMDTEXT_NODISCARD SIMDTEXT_API std::vector<std::byte> hex_decode(std::string_view input);

// ── Base64 Encode/Decode ───────────────────────────────────

/// Encode bytes to Base64. Returns bytes written, 0 on error.
SIMDTEXT_NODISCARD SIMDTEXT_API size_t base64_encode_to(std::span<const std::byte> input, std::span<char> output);

/// Encode bytes to Base64 string.
SIMDTEXT_NODISCARD SIMDTEXT_API std::string base64_encode(std::span<const std::byte> input);

/// Decode Base64 to bytes.
SIMDTEXT_NODISCARD SIMDTEXT_API DecodeResult base64_decode_to(std::string_view input, std::span<std::byte> output);

/// Decode Base64 to char buffer.
SIMDTEXT_NODISCARD SIMDTEXT_API DecodeResult base64_decode_to(std::string_view input, std::span<char> output);

/// Decode Base64 string to byte vector.
SIMDTEXT_NODISCARD SIMDTEXT_API std::vector<std::byte> base64_decode(std::string_view input);

// ── Internal ───────────────────────────────────────────────

/// Hex digit value (internal helper).
SIMDTEXT_NODISCARD SIMDTEXT_API int hex_val(char c) noexcept;

} // namespace simdtext

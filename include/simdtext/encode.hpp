#pragma once

/// @file encode.hpp
/// @brief Hex and Base64 encoding/decoding operations.

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <simdtext/types.hpp>

namespace simdtext {

// ── Hex Encode/Decode ──────────────────────────────────────

/// Encode bytes to hexadecimal. Returns bytes written, 0 on error.
size_t hex_encode_to(std::span<const std::byte> input, std::span<char> output);

/// Encode bytes to hexadecimal string.
std::string hex_encode(std::span<const std::byte> input);

/// Decode hexadecimal to bytes.
DecodeResult hex_decode_to(std::string_view input, std::span<std::byte> output);

/// Decode hexadecimal to char buffer.
DecodeResult hex_decode_to(std::string_view input, std::span<char> output);

/// Decode hexadecimal string to byte vector.
std::vector<std::byte> hex_decode(std::string_view input);

// ── Base64 Encode/Decode ───────────────────────────────────

/// Encode bytes to Base64. Returns bytes written, 0 on error.
size_t base64_encode_to(std::span<const std::byte> input, std::span<char> output);

/// Encode bytes to Base64 string.
std::string base64_encode(std::span<const std::byte> input);

/// Decode Base64 to bytes.
DecodeResult base64_decode_to(std::string_view input, std::span<std::byte> output);

/// Decode Base64 to char buffer.
DecodeResult base64_decode_to(std::string_view input, std::span<char> output);

/// Decode Base64 string to byte vector.
std::vector<std::byte> base64_decode(std::string_view input);

// ── Internal ───────────────────────────────────────────────

/// Hex digit value (internal helper).
int hex_val(char c) noexcept;

} // namespace simdtext

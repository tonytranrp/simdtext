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

/// Encode bytes to lowercase hexadecimal.
/// @param input Source bytes to encode.
/// @param output Destination buffer. Must be at least `input.size() * 2` bytes.
/// @return Number of bytes written to `output`, or 0 if buffer too small.
/// @note SIMD-accelerated via Highway. Uses pshufb for nibble→hex lookup.
SIMDTEXT_NODISCARD SIMDTEXT_API size_t hex_encode_to(std::span<const std::byte> input, std::span<char> output);

/// Encode bytes to lowercase hexadecimal string (allocating convenience wrapper).
/// @param input Source bytes to encode.
/// @return Hex-encoded string of length `input.size() * 2`.
SIMDTEXT_NODISCARD SIMDTEXT_API std::string hex_encode(std::span<const std::byte> input);

/// Decode hexadecimal string to bytes.
/// @param input Hex string to decode. Must have even length.
/// @param output Destination buffer. Must be at least `input.size() / 2` bytes.
/// @return DecodeResult with count of bytes written and success flag.
SIMDTEXT_NODISCARD SIMDTEXT_API DecodeResult hex_decode_to(std::string_view input, std::span<std::byte> output) noexcept;

/// Decode hexadecimal string to char buffer.
/// @param input Hex string to decode. Must have even length.
/// @param output Destination buffer. Must be at least `input.size() / 2` bytes.
/// @return DecodeResult with count of bytes written and success flag.
SIMDTEXT_NODISCARD SIMDTEXT_API DecodeResult hex_decode_to(std::string_view input, std::span<char> output) noexcept;

/// Decode hexadecimal string to byte vector (allocating convenience wrapper).
/// @param input Hex string to decode. Must have even length.
/// @return Decoded byte vector.
SIMDTEXT_NODISCARD SIMDTEXT_API std::vector<std::byte> hex_decode(std::string_view input);

// ── Base64 Encode/Decode ───────────────────────────────────

/// Encode bytes to Base64 (RFC 4648).
/// @param input Source bytes to encode.
/// @param output Destination buffer. Must be at least `4 * ((input.size() + 2) / 3)` bytes.
/// @return Number of bytes written to `output`, or 0 if buffer too small.
/// @note SIMD-accelerated via Highway for inputs >= 12 bytes.
SIMDTEXT_NODISCARD SIMDTEXT_API size_t base64_encode_to(std::span<const std::byte> input, std::span<char> output);

/// Encode bytes to Base64 string (allocating convenience wrapper).
/// @param input Source bytes to encode.
/// @return Base64-encoded string with `=` padding.
SIMDTEXT_NODISCARD SIMDTEXT_API std::string base64_encode(std::span<const std::byte> input);

/// Decode Base64 string to bytes.
/// @param input Base64 string to decode. May contain `=` padding.
/// @param output Destination buffer.
/// @return DecodeResult with count of bytes written and success flag.
SIMDTEXT_NODISCARD SIMDTEXT_API DecodeResult base64_decode_to(std::string_view input, std::span<std::byte> output) noexcept;

/// Decode Base64 string to char buffer.
/// @param input Base64 string to decode. May contain `=` padding.
/// @param output Destination buffer.
/// @return DecodeResult with count of bytes written and success flag.
SIMDTEXT_NODISCARD SIMDTEXT_API DecodeResult base64_decode_to(std::string_view input, std::span<char> output) noexcept;

/// Decode Base64 string to byte vector (allocating convenience wrapper).
/// @param input Base64 string to decode.
/// @return Decoded byte vector.
SIMDTEXT_NODISCARD SIMDTEXT_API std::vector<std::byte> base64_decode(std::string_view input);

// ── Internal ───────────────────────────────────────────────

/// Hex digit value (internal helper).
/// @param c Hex character ('0'-'9', 'a'-'f', 'A'-'F').
/// @return Integer value 0-15, or -1 if not a hex digit.
SIMDTEXT_NODISCARD SIMDTEXT_API int hex_val(char c) noexcept;

} // namespace simdtext

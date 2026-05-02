#pragma once

/// @file pattern.hpp
/// @brief SIMD-accelerated byte pattern scanning in memory buffers.
///
/// Supports exact byte sequences, wildcard patterns (e.g. "48 8B 05 ? ? ? ?"),
/// and masked searches. Inspired by libhat's find_pattern.

#include "export.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>
#include <optional>

namespace simdtext {

/// A compiled byte pattern with optional wildcards.
/// Zero-allocation: the pattern references the caller's storage.
class SIMDTEXT_API BytePattern {
public:
    /// Build from a hex string like "48 8B 05 ? ? ? ?" or "48 8B 05 ?? ?? ?? ??".
    /// '?' or '??' means wildcard (match any byte).
    static std::optional<BytePattern> parse(std::string_view hex_pattern);

    /// Build from raw bytes (no wildcards).
    static BytePattern from_bytes(std::span<const uint8_t> bytes);

    /// Build from raw bytes with a mask (0xFF = must match, 0x00 = wildcard).
    static BytePattern from_masked(std::span<const uint8_t> bytes, std::span<const uint8_t> mask);

    BytePattern() = default;

    /// Number of bytes in the pattern.
    [[nodiscard]] size_t size() const noexcept { return bytes_.size(); }
    [[nodiscard]] bool empty() const noexcept { return bytes_.empty(); }

    /// Access individual byte and mask.
    [[nodiscard]] uint8_t byte(size_t i) const noexcept { return bytes_[i]; }
    [[nodiscard]] uint8_t mask(size_t i) const noexcept { return mask_[i]; }

    [[nodiscard]] std::span<const uint8_t> bytes() const noexcept { return bytes_; }
    [[nodiscard]] std::span<const uint8_t> masks() const noexcept { return mask_; }

private:
    // Small-buffer storage; heap-allocates only for patterns > 64 bytes.
    static constexpr size_t SBO_SIZE = 64;
    std::vector<uint8_t> bytes_;
    std::vector<uint8_t> mask_;
};

/// Find the first occurrence of pattern in [data, data+length).
/// Returns pointer to match, or nullptr if not found.
SIMDTEXT_NODISCARD SIMDTEXT_API const uint8_t* find_pattern(
    const uint8_t* data, size_t length, const BytePattern& pattern);

/// Convenience overload for std::span.
SIMDTEXT_NODISCARD inline const uint8_t* find_pattern(
    std::span<const uint8_t> data, const BytePattern& pattern) {
    return find_pattern(data.data(), data.size(), pattern);
}

/// Find the first occurrence of a hex pattern string (e.g. "48 8B 05 ? ? ? ?").
/// Returns pointer to match, or nullptr if not found or pattern is invalid.
SIMDTEXT_NODISCARD SIMDTEXT_API const uint8_t* find_pattern(
    const uint8_t* data, size_t length, std::string_view hex_pattern);

/// Find all occurrences of pattern in [data, data+length).
/// Returns vector of pointers to each match.
SIMDTEXT_NODISCARD SIMDTEXT_API std::vector<const uint8_t*> find_all_patterns(
    const uint8_t* data, size_t length, const BytePattern& pattern);

/// Convenience overload for std::span.
SIMDTEXT_NODISCARD inline std::vector<const uint8_t*> find_all_patterns(
    std::span<const uint8_t> data, const BytePattern& pattern) {
    return find_all_patterns(data.data(), data.size(), pattern);
}

} // namespace simdtext

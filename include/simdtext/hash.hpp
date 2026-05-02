#pragma once

/// @file hash.hpp
/// @brief Compile-time and runtime string hashing utilities.

#include "export.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace simdtext {

/// Compile-time FNV-1a hash for string literals.
/// Usage: constexpr uint64_t h = fnv1a("hello");
constexpr uint64_t fnv1a(std::string_view s) noexcept {
    uint64_t hash = 14695981039346656037ULL;
    for (char c : s) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= 1099511628211ULL;
    }
    return hash;
}

/// Compile-time FNV-1a hash from a string literal (template parameter).
template <size_t N>
constexpr uint64_t fnv1a(const char (&s)[N]) noexcept {
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < N - 1; ++i) { // N-1 to skip null terminator
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(s[i]));
        hash *= 1099511628211ULL;
    }
    return hash;
}

/// Runtime CRC32 using hardware instruction if available, software fallback otherwise.
SIMDTEXT_NODISCARD SIMDTEXT_API uint32_t crc32(std::string_view data) noexcept;

/// Runtime CRC32C (Castagnoli) using hardware instruction if available.
SIMDTEXT_NODISCARD SIMDTEXT_API uint32_t crc32c(std::string_view data) noexcept;

/// Runtime xxHash-64 — fast non-cryptographic hash.
SIMDTEXT_NODISCARD SIMDTEXT_API uint64_t xxhash64(std::string_view data) noexcept;

/// Runtime Wyhash — extremely fast non-cryptographic hash.
SIMDTEXT_NODISCARD SIMDTEXT_API uint64_t wyhash(std::string_view data) noexcept;

} // namespace simdtext

/// Convenience macro for switch-case string matching:
///   switch(fnv1a(str)) { case SIMDTEXT_HASH("hello"): ... }
#define SIMDTEXT_HASH(s) (::simdtext::fnv1a(s))

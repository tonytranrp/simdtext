#pragma once

/// @file parallel.hpp
/// @brief Parallel text processing using multiple threads.

#include "export.hpp"
#include "scan.hpp"
#include <cstddef>
#include <functional>
#include <span>
#include <string_view>

namespace simdtext {

/// Parallel text processing options.
struct SIMDTEXT_API ParallelOptions {
    /// Number of threads to use. 0 = auto-detect (hardware concurrency).
    unsigned int num_threads = 0;
    /// Minimum chunk size per thread (bytes). Prevents overhead on small inputs.
    size_t min_chunk_size = 64 * 1024; // 64 KB
};

/// Count occurrences of a byte in parallel across the input.
/// @param data Input data
/// @param byte Byte to count
/// @param opts Parallel options
/// @return Count of byte occurrences
SIMDTEXT_NODISCARD SIMDTEXT_API size_t parallel_count_byte(
    std::string_view data, char byte, const ParallelOptions& opts = {});

/// Check if data is pure ASCII in parallel.
/// @param data Input data
/// @param opts Parallel options
/// @return true if all bytes are < 0x80
SIMDTEXT_NODISCARD SIMDTEXT_API bool parallel_is_ascii(
    std::string_view data, const ParallelOptions& opts = {});

/// Count newlines in parallel.
/// @param data Input data
/// @param opts Parallel options
/// @return Number of '\n' characters
SIMDTEXT_NODISCARD SIMDTEXT_API size_t parallel_count_newlines(
    std::string_view data, const ParallelOptions& opts = {});

/// Find first occurrence of a byte in parallel.
/// Uses a two-phase approach: threads scan chunks in parallel, then the
/// earliest result is selected. Best for large inputs (>1MB).
/// @param data Input data
/// @param byte Byte to find
/// @param opts Parallel options
/// @return Pointer to first occurrence, or data+size if not found
SIMDTEXT_NODISCARD SIMDTEXT_API const char* parallel_find_byte(
    std::string_view data, char byte, const ParallelOptions& opts = {});

/// Validate UTF-8 in parallel.
/// @param data Input data
/// @param opts Parallel options
/// @return true if valid UTF-8
SIMDTEXT_NODISCARD SIMDTEXT_API bool parallel_valid_utf8(
    std::string_view data, const ParallelOptions& opts = {});

/// Process a large buffer in parallel chunks.
/// @param data Input data
/// @param callback Called for each chunk (may be called from any thread)
/// @param opts Parallel options
SIMDTEXT_API void parallel_for_each_chunk(
    std::string_view data,
    std::function<void(std::string_view chunk, size_t offset)> callback,
    const ParallelOptions& opts = {});

} // namespace simdtext

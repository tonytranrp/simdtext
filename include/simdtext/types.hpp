#pragma once

/// @file types.hpp
/// @brief Core error types and convenience aliases for simdtext.

#include "export.hpp"
#include <cstddef>

namespace simdtext {

// ── Error handling ─────────────────────────────────────────

/// Error codes returned by decode operations.
enum class ErrorCode {
    Ok = 0,
    InvalidChar,
    InvalidLength,
    OutputTooSmall,
};

/// Result of a decode operation.
struct DecodeResult {
    size_t bytes_written = 0;
    size_t error_offset = 0;
    ErrorCode error = ErrorCode::Ok;

    [[nodiscard]] bool ok() const noexcept { return error == ErrorCode::Ok; }
};

} // namespace simdtext

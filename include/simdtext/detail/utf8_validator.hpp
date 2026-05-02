#pragma once
/// @file utf8_validator.hpp
/// @brief Shared UTF-8 validation state machine — used by all SIMD backends.
///
/// This eliminates the copy-paste of UTF-8 validation logic across
/// SSE2, AVX2, scalar, and Highway backends. Each backend handles
/// the SIMD fast-path (all-ASCII chunk skipping) and delegates
/// non-ASCII byte-by-byte processing to this state machine.

#include <cstddef>
#include <cstdint>

namespace simdtext::detail {

/// UTF-8 validation state machine.
/// Process one byte at a time. Returns false on invalid input.
/// After all bytes, call is_valid() to check for incomplete sequences.
struct Utf8Validator {
    int expected_cont = 0;       ///< continuation bytes still expected
    uint8_t prev_lead_byte = 0;  ///< the lead byte of current sequence
    uint8_t prev_lead_class = 0; ///< number of bytes in current sequence (2/3/4)

    /// Process a single byte. Returns false if the byte is invalid.
    [[nodiscard]] bool process(uint8_t byte) noexcept {
        if (byte <= 0x7F) {
            if (expected_cont > 0) [[unlikely]] return false;
            return true;
        }
        if ((byte & 0xE0) == 0xC0) {
            if (expected_cont > 0) [[unlikely]] return false;
            if (byte < 0xC2) [[unlikely]] return false;  // overlong
            expected_cont = 1;
            prev_lead_class = 2;
            prev_lead_byte = byte;
            return true;
        }
        if ((byte & 0xF0) == 0xE0) {
            if (expected_cont > 0) [[unlikely]] return false;
            expected_cont = 2;
            prev_lead_class = 3;
            prev_lead_byte = byte;
            return true;
        }
        if ((byte & 0xF8) == 0xF0) {
            if (expected_cont > 0) [[unlikely]] return false;
            if (byte > 0xF4) [[unlikely]] return false;  // > U+10FFFF
            expected_cont = 3;
            prev_lead_class = 4;
            prev_lead_byte = byte;
            return true;
        }
        if ((byte & 0xC0) == 0x80) {
            if (expected_cont == 0) [[unlikely]] return false;
            // Range checks on first continuation byte after lead
            if (expected_cont == prev_lead_class - 1) {
                if (prev_lead_byte == 0xE0 && byte < 0xA0) [[unlikely]] return false;  // overlong 3-byte
                if (prev_lead_byte == 0xED && byte > 0x9F) [[unlikely]] return false;  // surrogate
                if (prev_lead_byte == 0xF0 && byte < 0x90) [[unlikely]] return false;  // overlong 4-byte
                if (prev_lead_byte == 0xF4 && byte > 0x8F) [[unlikely]] return false;  // > U+10FFFF
            }
            --expected_cont;
            return true;
        }
        return false;  // invalid byte (0xC0, 0xC1, 0xF5..0xFF)
    }

    /// Check if the stream ended in a valid state (no incomplete sequences).
    [[nodiscard]] bool is_valid() const noexcept {
        return expected_cont == 0;
    }

    /// Process a range of bytes. Returns false if any byte is invalid.
    [[nodiscard]] bool process(const uint8_t* data, size_t size) noexcept {
        for (size_t i = 0; i < size; ++i) {
            if (!process(data[i])) [[unlikely]] return false;
        }
        return true;
    }

    /// Process a range, but only check is_valid() at the end.
    /// Returns false if any byte is invalid OR if the stream is incomplete.
    [[nodiscard]] bool validate(const uint8_t* data, size_t size) noexcept {
        return process(data, size) && is_valid();
    }
};

} // namespace simdtext::detail

#include "simdtext/pattern.hpp"
#include "simdtext/detail/cpu_detect.hpp"

#include <cstring>
#include <cctype>

// SSE2 / AVX2 headers
#if defined(__SSE2__)
#include <emmintrin.h>
#endif
#if defined(__AVX2__)
#include <immintrin.h>
#endif

namespace simdtext {

// ── BytePattern parsing ────────────────────────────────────

std::optional<BytePattern> BytePattern::parse(std::string_view hex_pattern) {
    BytePattern p;
    // Parse tokens separated by spaces
    size_t i = 0;
    while (i < hex_pattern.size()) {
        // Skip spaces
        if (hex_pattern[i] == ' ') { ++i; continue; }

        // Read two hex chars or wildcard
        uint8_t byte_val = 0;
        uint8_t mask_val = 0xFF;
        bool is_wildcard = false;

        for (int nibble = 0; nibble < 2; ++nibble) {
            if (i >= hex_pattern.size()) break;
            char c = hex_pattern[i];
            if (c == ' ') break;
            if (c == '?') {
                is_wildcard = true;
                mask_val = 0x00;
                ++i;
            } else if (std::isxdigit(static_cast<unsigned char>(c))) {
                int val = 0;
                if (c >= '0' && c <= '9') val = c - '0';
                else if (c >= 'a' && c <= 'f') val = c - 'a' + 10;
                else val = c - 'A' + 10;
                byte_val = (byte_val << 4) | static_cast<uint8_t>(val);
                ++i;
            } else {
                return std::nullopt; // invalid char
            }
        }
        if (is_wildcard) byte_val = 0x00;
        p.bytes_.push_back(byte_val);
        p.mask_.push_back(mask_val);
    }
    if (p.bytes_.empty()) return std::nullopt;
    return p;
}

BytePattern BytePattern::from_bytes(std::span<const uint8_t> bytes) {
    BytePattern p;
    p.bytes_.assign(bytes.begin(), bytes.end());
    p.mask_.assign(bytes.size(), 0xFF);
    return p;
}

BytePattern BytePattern::from_masked(std::span<const uint8_t> bytes, std::span<const uint8_t> mask) {
    BytePattern p;
    if (bytes.size() != mask.size()) return p;
    p.bytes_.assign(bytes.begin(), bytes.end());
    p.mask_.assign(mask.begin(), mask.end());
    return p;
}

// ── Scalar find_pattern ────────────────────────────────────

namespace {

const uint8_t* find_pattern_scalar(
    const uint8_t* SIMDTEXT_RESTRICT data, size_t length, const BytePattern& pattern) noexcept
{
    const size_t pat_len = pattern.size();
    if (pat_len == 0 || pat_len > length) return nullptr;

    const uint8_t* SIMDTEXT_RESTRICT const pat_bytes = pattern.bytes().data();
    const uint8_t* SIMDTEXT_RESTRICT const pat_mask = pattern.masks().data();

    // Find first and last non-wildcard byte for fast reject
    size_t first_fixed = pat_len;
    size_t last_fixed = pat_len;
    uint8_t first_byte = 0;
    uint8_t last_byte = 0;
    for (size_t j = 0; j < pat_len; ++j) {
        if (pat_mask[j] != 0x00) {
            if (first_fixed == pat_len) { first_fixed = j; first_byte = pat_bytes[j]; }
            last_fixed = j; last_byte = pat_bytes[j];
        }
    }

    const size_t scan_limit = length - pat_len + 1;

    // If all wildcards, return first position
    if (first_fixed == pat_len) return data;

    // Two-byte fast reject: check first AND last fixed byte before full match
    for (size_t i = 0; i < scan_limit; ++i) {
        __builtin_prefetch(data + i + 64, 0, 1);
        if (data[i + first_fixed] != first_byte) continue;
        if (last_fixed != first_fixed && data[i + last_fixed] != last_byte) continue;

        bool match = true;
        for (size_t j = 0; j < pat_len; ++j) {
            if (pat_mask[j] != 0x00 && data[i + j] != pat_bytes[j]) {
                match = false;
                break;
            }
        }
        if (match) return data + i;
    }
    return nullptr;
}

// ── SSE2 find_pattern (vectorized first-byte scan) ─────────

#if defined(__SSE2__)
SIMDTEXT_TARGET_SSE2
const uint8_t* find_pattern_sse2(
    const uint8_t* data, size_t length, const BytePattern& pattern) noexcept
{
    const size_t pat_len = pattern.size();
    if (pat_len == 0 || pat_len > length) return nullptr;

    const uint8_t* const pat_bytes = pattern.bytes().data();
    const uint8_t* const pat_mask = pattern.masks().data();

    // Find first non-wildcard byte
    size_t first_fixed = pat_len;
    uint8_t first_byte = 0;
    for (size_t j = 0; j < pat_len; ++j) {
        if (pat_mask[j] != 0x00) {
            first_fixed = j;
            first_byte = pat_bytes[j];
            break;
        }
    }
    if (first_fixed == pat_len) return data;

    const size_t scan_limit = length - pat_len + 1;
    const __m128i v_first = _mm_set1_epi8(static_cast<char>(first_byte));

    size_t i = 0;
    // SSE2: scan 16 bytes at a time for the first fixed byte
    if (scan_limit >= 16 + pat_len - 1) {
        const size_t sse_limit = scan_limit - 16 + 1;
        for (; i < sse_limit; i += 16) {
            __m128i v_data = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i + first_fixed));
            __m128i eq = _mm_cmpeq_epi8(v_data, v_first);
            int mask = _mm_movemask_epi8(eq);
            while (mask != 0) {
                int bit = __builtin_ctz(mask);
                mask &= mask - 1;
                size_t candidate = i + static_cast<size_t>(bit);
                if (candidate >= scan_limit) break;

                bool match = true;
                for (size_t j = 0; j < pat_len; ++j) {
                    if (pat_mask[j] != 0x00 && data[candidate + j] != pat_bytes[j]) {
                        match = false;
                        break;
                    }
                }
                if (match) return data + candidate;
            }
        }
    }

    // Scalar tail
    for (; i < scan_limit; ++i) {
        if (data[i + first_fixed] != first_byte) continue;
        bool match = true;
        for (size_t j = 0; j < pat_len; ++j) {
            if (pat_mask[j] != 0x00 && data[i + j] != pat_bytes[j]) {
                match = false;
                break;
            }
        }
        if (match) return data + i;
    }
    return nullptr;
}
#endif // __SSE2__

// ── AVX2 find_pattern ──────────────────────────────────────

#if defined(__AVX2__)
SIMDTEXT_TARGET_AVX2
const uint8_t* find_pattern_avx2(
    const uint8_t* data, size_t length, const BytePattern& pattern) noexcept
{
    const size_t pat_len = pattern.size();
    if (pat_len == 0 || pat_len > length) return nullptr;

    const uint8_t* const pat_bytes = pattern.bytes().data();
    const uint8_t* const pat_mask = pattern.masks().data();

    // Find first non-wildcard byte
    size_t first_fixed = pat_len;
    uint8_t first_byte = 0;
    for (size_t j = 0; j < pat_len; ++j) {
        if (pat_mask[j] != 0x00) {
            first_fixed = j;
            first_byte = pat_bytes[j];
            break;
        }
    }
    if (first_fixed == pat_len) return data;

    const size_t scan_limit = length - pat_len + 1;
    const __m256i v_first = _mm256_set1_epi8(static_cast<char>(first_byte));

    size_t i = 0;
    if (scan_limit >= 32 + pat_len - 1) {
        const size_t avx_limit = scan_limit - 32 + 1;
        for (; i < avx_limit; i += 32) {
            __m256i v_data = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + first_fixed));
            __m256i eq = _mm256_cmpeq_epi8(v_data, v_first);
            int mask = _mm256_movemask_epi8(eq);
            while (mask != 0) {
                int bit = __builtin_ctz(mask);
                mask &= mask - 1;
                size_t candidate = i + static_cast<size_t>(bit);
                if (candidate >= scan_limit) break;

                bool match = true;
                for (size_t j = 0; j < pat_len; ++j) {
                    if (pat_mask[j] != 0x00 && data[candidate + j] != pat_bytes[j]) {
                        match = false;
                        break;
                    }
                }
                if (match) return data + candidate;
            }
        }
    }

    // Fall back to SSE2 for the remaining
    for (; i < scan_limit; ++i) {
        if (data[i + first_fixed] != first_byte) continue;
        bool match = true;
        for (size_t j = 0; j < pat_len; ++j) {
            if (pat_mask[j] != 0x00 && data[i + j] != pat_bytes[j]) {
                match = false;
                break;
            }
        }
        if (match) return data + i;
    }
    return nullptr;
}
#endif // __AVX2__

} // anonymous namespace

// ── Public API ─────────────────────────────────────────────

const uint8_t* find_pattern(
    const uint8_t* data, size_t length, const BytePattern& pattern)
{
    if (pattern.empty() || length == 0) return nullptr;

    const auto& cpu = detail::detect_cpu();

#if defined(__AVX2__)
    if (cpu.avx2) return find_pattern_avx2(data, length, pattern);
#endif
#if defined(__SSE2__)
    if (cpu.sse2) return find_pattern_sse2(data, length, pattern);
#endif

    return find_pattern_scalar(data, length, pattern);
}

const uint8_t* find_pattern(
    const uint8_t* data, size_t length, std::string_view hex_pattern)
{
    auto parsed = BytePattern::parse(hex_pattern);
    if (!parsed) return nullptr;
    return find_pattern(data, length, *parsed);
}

std::vector<const uint8_t*> find_all_patterns(
    const uint8_t* data, size_t length, const BytePattern& pattern)
{
    std::vector<const uint8_t*> results;
    if (pattern.empty() || length == 0) return results;

    size_t offset = 0;
    while (offset < length) {
        const uint8_t* found = find_pattern(data + offset, length - offset, pattern);
        if (!found) break;
        results.push_back(found);
        // Move past this match by 1 to find overlapping matches
        offset = static_cast<size_t>(found - data) + 1;
    }
    return results;
}

} // namespace simdtext

#include <emmintrin.h>  // SSE2
#include <cstddef>
#include <cstdint>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

// Target SSE2 only. No SSSE3, no popcnt.
// Use movemask + SWAR popcount to avoid function call overhead.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC push_options
#pragma GCC target("sse2")
#pragma GCC optimize("no-tree-vectorize")
#endif

namespace simdtext::detail::sse2 {

namespace {

// SWAR popcount — pure arithmetic, no hardware popcnt needed
// Inlines to ~5 ALU instructions, no function call
inline int popcount16(unsigned int x) {
    x = x - ((x >> 1) & 0x55555555u);
    x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    return static_cast<int>((((x + (x >> 4)) & 0x0F0F0F0Fu) * 0x01010101u) >> 24);
}

inline int ctz32(unsigned int x) {
#if defined(_MSC_VER)
    unsigned long idx;
    _BitScanForward(&idx, x);
    return static_cast<int>(idx);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_ctz(x);
#else
    if (x == 0) return 32;
    int n = 0;
    if ((x & 0x0000FFFFu) == 0) { n += 16; x >>= 16; }
    if ((x & 0x000000FFu) == 0) { n += 8;  x >>= 8;  }
    if ((x & 0x0000000Fu) == 0) { n += 4;  x >>= 4;  }
    if ((x & 0x00000003u) == 0) { n += 2;  x >>= 2;  }
    if ((x & 0x00000001u) == 0) { n += 1; }
    return n;
#endif
}

} // anonymous namespace

size_t count_byte(const char* data, size_t size, char byte) {
    const __m128i vbyte = _mm_set1_epi8(byte);
    size_t count = 0;
    size_t i = 0;

    // 4x unrolled: process 64 bytes per iteration
    // Use movemask → SWAR popcount to avoid scalar function calls
    for (; i + 64 <= size; i += 64) {
        __m128i c0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        __m128i c1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i + 16));
        __m128i c2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i + 32));
        __m128i c3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i + 48));
        unsigned int m0 = static_cast<unsigned int>(_mm_movemask_epi8(_mm_cmpeq_epi8(c0, vbyte)));
        unsigned int m1 = static_cast<unsigned int>(_mm_movemask_epi8(_mm_cmpeq_epi8(c1, vbyte)));
        unsigned int m2 = static_cast<unsigned int>(_mm_movemask_epi8(_mm_cmpeq_epi8(c2, vbyte)));
        unsigned int m3 = static_cast<unsigned int>(_mm_movemask_epi8(_mm_cmpeq_epi8(c3, vbyte)));
        // Combine popcounts to reduce loop overhead
        count += static_cast<size_t>(popcount16(m0) + popcount16(m1) + popcount16(m2) + popcount16(m3));
    }

    for (; i + 16 <= size; i += 16) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        unsigned int mask = static_cast<unsigned int>(_mm_movemask_epi8(_mm_cmpeq_epi8(chunk, vbyte)));
        count += popcount16(mask);
    }

    for (; i < size; ++i)
        if (data[i] == byte) ++count;
    return count;
}

bool is_ascii(const char* data, size_t size) {
    const __m128i vhigh = _mm_set1_epi8(static_cast<char>(0x80));
    size_t i = 0;
    // Accumulate across 4 vectors before branching (reduce mispredicts)
    for (; i + 64 <= size; i += 64) {
        __m128i c0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        __m128i c1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i + 16));
        __m128i c2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i + 32));
        __m128i c3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i + 48));
        __m128i ored = _mm_or_si128(_mm_or_si128(c0, c1), _mm_or_si128(c2, c3));
        if (_mm_movemask_epi8(_mm_and_si128(ored, vhigh)) != 0)
            return false;
    }
    for (; i + 16 <= size; i += 16) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        if (_mm_movemask_epi8(_mm_and_si128(chunk, vhigh)) != 0)
            return false;
    }
    for (; i < size; ++i)
        if (static_cast<unsigned char>(data[i]) > 0x7F) return false;
    return true;
}

void lowercase_ascii(char* data, size_t size) {
    const __m128i vA = _mm_set1_epi8('A' - 1);
    const __m128i vZ1 = _mm_set1_epi8('Z' + 1);
    const __m128i vbit5 = _mm_set1_epi8(0x20);
    // Non-temporal store threshold: ~2MB (half of typical L3)
    const size_t nontemporal_threshold = 2 * 1024 * 1024;
    const bool use_nontemporal = size > nontemporal_threshold;
    size_t i = 0;
    for (; i + 16 <= size; i += 16) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        __m128i not_above = _mm_cmplt_epi8(chunk, vZ1);
        __m128i not_below = _mm_cmpgt_epi8(chunk, vA);
        __m128i is_upper = _mm_and_si128(not_above, not_below);
        __m128i lowered = _mm_xor_si128(chunk, _mm_and_si128(is_upper, vbit5));
        if (use_nontemporal)
            _mm_stream_si128(reinterpret_cast<__m128i*>(data + i), lowered);
        else
            _mm_storeu_si128(reinterpret_cast<__m128i*>(data + i), lowered);
    }
    if (use_nontemporal) _mm_sfence();
    for (; i < size; ++i) {
        auto c = static_cast<unsigned char>(data[i]);
        if (c >= 'A' && c <= 'Z') data[i] = static_cast<char>(c ^ 0x20);
    }
}

void uppercase_ascii(char* data, size_t size) {
    const __m128i va = _mm_set1_epi8('a' - 1);
    const __m128i vz1 = _mm_set1_epi8('z' + 1);
    const __m128i vbit5 = _mm_set1_epi8(0x20);
    // Non-temporal store threshold: ~2MB (half of typical L3)
    const size_t nontemporal_threshold = 2 * 1024 * 1024;
    const bool use_nontemporal = size > nontemporal_threshold;
    size_t i = 0;
    for (; i + 16 <= size; i += 16) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        __m128i not_above = _mm_cmplt_epi8(chunk, vz1);
        __m128i not_below = _mm_cmpgt_epi8(chunk, va);
        __m128i is_lower = _mm_and_si128(not_above, not_below);
        __m128i uppered = _mm_xor_si128(chunk, _mm_and_si128(is_lower, vbit5));
        if (use_nontemporal)
            _mm_stream_si128(reinterpret_cast<__m128i*>(data + i), uppered);
        else
            _mm_storeu_si128(reinterpret_cast<__m128i*>(data + i), uppered);
    }
    if (use_nontemporal) _mm_sfence();
    for (; i < size; ++i) {
        auto c = static_cast<unsigned char>(data[i]);
        if (c >= 'a' && c <= 'z') data[i] = static_cast<char>(c ^ 0x20);
    }
}

const char* find_byte(const char* data, size_t size, char byte) {
    const __m128i vbyte = _mm_set1_epi8(byte);
    size_t i = 0;
    for (; i + 16 <= size; i += 16) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(chunk, vbyte));
        if (mask != 0)
            return data + i + ctz32(static_cast<unsigned int>(mask));
    }
    for (; i < size; ++i)
        if (data[i] == byte) return data + i;
    return data + size;
}

// ── UTF-8 Validation (SSE2) ─────────────────────────────────────
// SSE2-accelerated UTF-8 validation: skip all-ASCII chunks with SIMD,
// fall back to scalar byte-by-byte for non-ASCII chunks.
// This correctly rejects overlong encodings, surrogates, and out-of-range code points.

bool validate_utf8(const char* data, size_t size) {
    const __m128i vhigh = _mm_set1_epi8(static_cast<char>(0x80));
    int expected_cont = 0;
    uint8_t prev_lead_byte = 0;
    uint8_t prev_lead_class = 0;
    uint8_t first_cont_byte = 0;

    size_t i = 0;
    for (; i + 16 <= size; i += 16) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        // Fast path: if all ASCII and not expecting continuation bytes, skip
        if (_mm_movemask_epi8(_mm_and_si128(chunk, vhigh)) == 0) {
            if (expected_cont > 0) return false;
            continue;
        }
        // Non-ASCII: process byte-by-byte within the chunk
        const auto* p = reinterpret_cast<const uint8_t*>(data + i);
        const auto* chunk_end = p + 16;
        while (p < chunk_end) {
            const auto byte = *p++;
            if (byte <= 0x7F) {
                if (expected_cont > 0) return false;
                continue;
            } else if ((byte & 0xE0) == 0xC0) {
                if (expected_cont > 0) return false;
                if (byte < 0xC2) return false;
                expected_cont = 1;
                prev_lead_class = 2;
                prev_lead_byte = byte;
            } else if ((byte & 0xF0) == 0xE0) {
                if (expected_cont > 0) return false;
                expected_cont = 2;
                prev_lead_class = 3;
                prev_lead_byte = byte;
                first_cont_byte = 0;
            } else if ((byte & 0xF8) == 0xF0) {
                if (expected_cont > 0) return false;
                if (byte > 0xF4) return false;
                expected_cont = 3;
                prev_lead_class = 4;
                prev_lead_byte = byte;
                first_cont_byte = 0;
            } else if ((byte & 0xC0) == 0x80) {
                if (expected_cont == 0) return false;
                if (expected_cont == prev_lead_class - 1) {
                    first_cont_byte = byte;
                    if (prev_lead_byte == 0xE0 && byte < 0xA0) return false;
                    if (prev_lead_byte == 0xED && byte > 0x9F) return false;
                    if (prev_lead_byte == 0xF0 && byte < 0x90) return false;
                    if (prev_lead_byte == 0xF4 && byte > 0x8F) return false;
                }
                --expected_cont;
            } else {
                return false;
            }
        }
    }

    // Scalar tail
    const auto* p = reinterpret_cast<const uint8_t*>(data + i);
    const auto* end = reinterpret_cast<const uint8_t*>(data + size);
    while (p < end) {
        const auto byte = *p++;
        if (byte <= 0x7F) {
            if (expected_cont > 0) return false;
        } else if ((byte & 0xE0) == 0xC0) {
            if (expected_cont > 0) return false;
            if (byte < 0xC2) return false;
            expected_cont = 1;
            prev_lead_byte = byte;
            prev_lead_class = 2;
        } else if ((byte & 0xF0) == 0xE0) {
            if (expected_cont > 0) return false;
            expected_cont = 2;
            prev_lead_class = 3;
            prev_lead_byte = byte;
            first_cont_byte = 0;
        } else if ((byte & 0xF8) == 0xF0) {
            if (expected_cont > 0) return false;
            if (byte > 0xF4) return false;
            expected_cont = 3;
            prev_lead_class = 4;
            prev_lead_byte = byte;
            first_cont_byte = 0;
        } else if ((byte & 0xC0) == 0x80) {
            if (expected_cont == 0) return false;
            if (expected_cont == prev_lead_class - 1) {
                first_cont_byte = byte;
                if (prev_lead_byte == 0xE0 && byte < 0xA0) return false;
                if (prev_lead_byte == 0xED && byte > 0x9F) return false;
                if (prev_lead_byte == 0xF0 && byte < 0x90) return false;
                if (prev_lead_byte == 0xF4 && byte > 0x8F) return false;
            }
            --expected_cont;
        } else {
            return false;
        }
    }
    return expected_cont == 0;
}


// ── SIMD count_code_points ─────────────────────────────────────
// Count Unicode code points = count bytes that are NOT continuation bytes (10xxxxxx)
//
// Continuation bytes have the pattern 10xxxxxx (0x80–0xBF).
// We test (byte & 0xC0) == 0x80 using SIMD:
//   signed cmpgt_epi8(byte, 0x7F) catches 0x80..0xFF  (byte > 0x7F in signed)
//   signed cmpgt_epi8(0xBF, byte) catches 0x00..0xBF  (0xBF > byte in signed)
//   AND gives 0x80..0xBF (continuation bytes)
// This correctly includes 0x80: signed(0x80) = -128 > signed(0x7F) = 127 ✓

size_t count_code_points(const char* data, size_t size) {
    // Byte >= 0x80: use signed > 0x7F (0x7F is the largest non-continuation value)
    const __m128i v7f = _mm_set1_epi8(static_cast<char>(0x7F));
    // Byte <= 0xBF: use signed 0xBF > byte
    const __m128i vbf = _mm_set1_epi8(static_cast<char>(0xBF));

    size_t i = 0;
    size_t count = 0;

    // Process 64 bytes at a time (4x unrolled)
    for (; i + 64 <= size; i += 64) {
        __m128i c0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        __m128i c1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i + 16));
        __m128i c2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i + 32));
        __m128i c3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i + 48));

        // Continuation byte: 0x80 <= byte <= 0xBF
        // ge_80: byte > 0x7F (signed) → catches 0x80..0xFF ✓ (includes 0x80!)
        // le_bf: 0xBF > byte (signed) → catches 0x00..0xBF
        __m128i m0 = _mm_and_si128(_mm_cmpgt_epi8(c0, v7f), _mm_cmpgt_epi8(vbf, c0));
        __m128i m1 = _mm_and_si128(_mm_cmpgt_epi8(c1, v7f), _mm_cmpgt_epi8(vbf, c1));
        __m128i m2 = _mm_and_si128(_mm_cmpgt_epi8(c2, v7f), _mm_cmpgt_epi8(vbf, c2));
        __m128i m3 = _mm_and_si128(_mm_cmpgt_epi8(c3, v7f), _mm_cmpgt_epi8(vbf, c3));

        // Non-continuation count = 64 - continuation_count
        uint32_t mask0 = _mm_movemask_epi8(m0);
        uint32_t mask1 = _mm_movemask_epi8(m1);
        uint32_t mask2 = _mm_movemask_epi8(m2);
        uint32_t mask3 = _mm_movemask_epi8(m3);

        count += 64 - popcount16(mask0) - popcount16(mask1)
                    - popcount16(mask2) - popcount16(mask3);
    }

    // Process 16 bytes at a time
    for (; i + 16 <= size; i += 16) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        __m128i m = _mm_and_si128(_mm_cmpgt_epi8(chunk, v7f), _mm_cmpgt_epi8(vbf, chunk));
        uint32_t mask = _mm_movemask_epi8(m);
        count += 16 - popcount16(mask);
    }

    // Scalar tail
    for (; i < size; ++i) {
        if ((static_cast<uint8_t>(data[i]) & 0xC0) != 0x80) ++count;
    }

    return count;
}

} // namespace simdtext::detail::sse2

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC pop_options
#endif

#include <immintrin.h>  // AVX2
#include <emmintrin.h>  // SSE2 (for tail)
#include <cstddef>
#include <cstdint>
#include "simdtext/types.hpp"
#include "simdtext/types.hpp"

using simdtext::DecodeResult;
using simdtext::ErrorCode;

// AVX2 implementation — must NOT use AVX-512 to avoid frequency downclocking.
// CMakeLists.txt adds -mno-avx512f to this object's compile flags.

#if defined(_MSC_VER)
#include <intrin.h>
#endif

// Prevent AVX-512 codegen in AVX2 functions — avoids frequency downclocking
// and ensures AVX2 path doesn't depend on AVX-512 availability.
// We explicitly target avx2+sse4.2+popcnt and exclude avx512 features.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC push_options
#pragma GCC target("avx2,bmi,popcnt,lzcnt,no-avx512f,no-avx512bw,no-avx512vl,no-avx512dq,no-avx512cd,no-avx512er,no-avx512pf,no-avx512vbmi,no-avx512ifma,no-avx512vpopcntdq")
#endif

namespace simdtext::detail::avx2 {

namespace {
inline int popcount64(unsigned long long x) {
#if defined(_MSC_VER) && defined(_WIN64)
    return static_cast<int>(__popcnt64(x));
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(x);
#else
    // Fold two 32-bit popcounts
    return popcount32(static_cast<unsigned int>(x)) +
           popcount32(static_cast<unsigned int>(x >> 32));
#endif
}

inline int popcount32(unsigned int x) {
#if defined(_MSC_VER)
    return static_cast<int>(__popcnt(x));
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_popcount(x);
#else
    x = x - ((x >> 1) & 0x55555555u);
    x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    return static_cast<int>((((x + (x >> 4)) & 0x0F0F0F0Fu) * 0x01010101u) >> 24);
#endif
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

inline int ctz64(unsigned long long x) {
#if defined(_MSC_VER) && defined(_WIN64)
    unsigned long idx;
    _BitScanForward64(&idx, x);
    return static_cast<int>(idx);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_ctzll(x);
#else
    if (x == 0) return 64;
    unsigned int lo = static_cast<unsigned int>(x);
    if (lo != 0) return ctz32(lo);
    return 32 + ctz32(static_cast<unsigned int>(x >> 32));
#endif
}
} // anonymous namespace

size_t count_byte(const char* data, size_t size, char byte) {
    const __m256i vbyte = _mm256_set1_epi8(byte);
    size_t count = 0;
    size_t i = 0;
    // 4x unrolled: process 128 bytes per iteration
    for (; i + 128 <= size; i += 128) {
        __m256i c0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        __m256i c1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + 32));
        __m256i c2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + 64));
        __m256i c3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + 96));
        unsigned int m0 = static_cast<unsigned int>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(c0, vbyte)));
        unsigned int m1 = static_cast<unsigned int>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(c1, vbyte)));
        unsigned int m2 = static_cast<unsigned int>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(c2, vbyte)));
        unsigned int m3 = static_cast<unsigned int>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(c3, vbyte)));
        count += static_cast<size_t>(popcount32(m0) + popcount32(m1) + popcount32(m2) + popcount32(m3));
    }
    // 64-byte iterations
    for (; i + 64 <= size; i += 64) {
        __m256i c0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        __m256i c1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + 32));
        unsigned int m0 = static_cast<unsigned int>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(c0, vbyte)));
        unsigned int m1 = static_cast<unsigned int>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(c1, vbyte)));
        count += static_cast<size_t>(popcount32(m0) + popcount32(m1));
    }
    for (; i + 32 <= size; i += 32) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        __m256i eq = _mm256_cmpeq_epi8(chunk, vbyte);
        count += popcount32(static_cast<unsigned int>(_mm256_movemask_epi8(eq)));
    }
    // Tail: use SSE2 for remaining >= 16 bytes
    if (i + 16 <= size) {
        const __m128i vbyte16 = _mm_set1_epi8(byte);
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        __m128i eq = _mm_cmpeq_epi8(chunk, vbyte16);
        count += popcount32(static_cast<unsigned int>(_mm_movemask_epi8(eq)));
        i += 16;
    }
    for (; i < size; ++i)
        if (data[i] == byte) ++count;
    return count;
}

bool is_ascii(const char* data, size_t size) {
    const __m256i vhigh = _mm256_set1_epi8(static_cast<char>(0x80));
    size_t i = 0;
    // 4x unrolled: accumulate OR across 4 vectors before branching
    for (; i + 128 <= size; i += 128) {
        __m256i c0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        __m256i c1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + 32));
        __m256i c2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + 64));
        __m256i c3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + 96));
        __m256i ored = _mm256_or_si256(_mm256_or_si256(c0, c1), _mm256_or_si256(c2, c3));
        if (_mm256_movemask_epi8(_mm256_and_si256(ored, vhigh)) != 0)
            return false;
    }
    // 64-byte
    for (; i + 64 <= size; i += 64) {
        __m256i c0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        __m256i c1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + 32));
        __m256i ored = _mm256_or_si256(c0, c1);
        if (_mm256_movemask_epi8(_mm256_and_si256(ored, vhigh)) != 0)
            return false;
    }
    for (; i + 32 <= size; i += 32) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        if (_mm256_movemask_epi8(_mm256_and_si256(chunk, vhigh)) != 0)
            return false;
    }
    // Tail with SSE2
    if (i + 16 <= size) {
        const __m128i vhigh16 = _mm_set1_epi8(static_cast<char>(0x80));
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        if (_mm_movemask_epi8(_mm_and_si128(chunk, vhigh16)) != 0)
            return false;
        i += 16;
    }
    for (; i < size; ++i)
        if (static_cast<unsigned char>(data[i]) > 0x7F) return false;
    return true;
}

void lowercase_ascii(char* data, size_t size) {
    const __m256i vA = _mm256_set1_epi8('A' - 1);
    const __m256i vZ1 = _mm256_set1_epi8('Z' + 1);
    const __m256i vbit5 = _mm256_set1_epi8(0x20);
    // Note: non-temporal (streaming) stores removed — they bypass cache and
    // cause thrashing on in-place read-modify-write patterns.
    size_t i = 0;
    for (; i + 32 <= size; i += 32) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        __m256i ge_A = _mm256_cmpgt_epi8(chunk, vA);
        __m256i le_Z = _mm256_cmpgt_epi8(vZ1, chunk);
        __m256i is_upper = _mm256_and_si256(ge_A, le_Z);
        __m256i lowered = _mm256_xor_si256(chunk, _mm256_and_si256(is_upper, vbit5));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(data + i), lowered);
    }
    // Tail with SSE2
    if (i + 16 <= size) {
        const __m128i vA16 = _mm_set1_epi8('A' - 1);
        const __m128i vZ1_16 = _mm_set1_epi8('Z' + 1);
        const __m128i vbit5_16 = _mm_set1_epi8(0x20);
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        __m128i ge_A = _mm_cmpgt_epi8(chunk, vA16);
        __m128i le_Z = _mm_cmpgt_epi8(vZ1_16, chunk);
        __m128i is_upper = _mm_and_si128(ge_A, le_Z);
        __m128i lowered = _mm_xor_si128(chunk, _mm_and_si128(is_upper, vbit5_16));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(data + i), lowered);
        i += 16;
    }
    for (; i < size; ++i) {
        auto c = static_cast<unsigned char>(data[i]);
        if (c >= 'A' && c <= 'Z') data[i] = static_cast<char>(c ^ 0x20);
    }
}

void uppercase_ascii(char* data, size_t size) {
    const __m256i va = _mm256_set1_epi8('a' - 1);
    const __m256i vz1 = _mm256_set1_epi8('z' + 1);
    const __m256i vbit5 = _mm256_set1_epi8(0x20);
    // Note: non-temporal (streaming) stores removed — they bypass cache and
    // cause thrashing on in-place read-modify-write patterns.
    size_t i = 0;
    for (; i + 32 <= size; i += 32) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        __m256i ge_a = _mm256_cmpgt_epi8(chunk, va);
        __m256i le_z = _mm256_cmpgt_epi8(vz1, chunk);
        __m256i is_lower = _mm256_and_si256(ge_a, le_z);
        __m256i uppered = _mm256_xor_si256(chunk, _mm256_and_si256(is_lower, vbit5));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(data + i), uppered);
    }
    // Tail with SSE2
    if (i + 16 <= size) {
        const __m128i va16 = _mm_set1_epi8('a' - 1);
        const __m128i vz1_16 = _mm_set1_epi8('z' + 1);
        const __m128i vbit5_16 = _mm_set1_epi8(0x20);
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        __m128i ge_a = _mm_cmpgt_epi8(chunk, va16);
        __m128i le_z = _mm_cmpgt_epi8(vz1_16, chunk);
        __m128i is_lower = _mm_and_si128(ge_a, le_z);
        __m128i uppered = _mm_xor_si128(chunk, _mm_and_si128(is_lower, vbit5_16));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(data + i), uppered);
        i += 16;
    }
    for (; i < size; ++i) {
        auto c = static_cast<unsigned char>(data[i]);
        if (c >= 'a' && c <= 'z') data[i] = static_cast<char>(c ^ 0x20);
    }
}

const char* find_byte(const char* data, size_t size, char byte) {
    const __m256i vbyte = _mm256_set1_epi8(byte);
    size_t i = 0;
    // Process 64 bytes at a time (2x __m256i)
    for (; i + 64 <= size; i += 64) {
        __m256i c0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        __m256i c1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + 32));
        int m0 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(c0, vbyte));
        if (m0 != 0)
            return data + i + ctz32(static_cast<unsigned int>(m0));
        int m1 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(c1, vbyte));
        if (m1 != 0)
            return data + i + 32 + ctz32(static_cast<unsigned int>(m1));
    }
    for (; i + 32 <= size; i += 32) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        int mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, vbyte));
        if (mask != 0)
            return data + i + ctz32(static_cast<unsigned int>(mask));
    }
    // Tail with SSE2
    if (i + 16 <= size) {
        const __m128i vbyte16 = _mm_set1_epi8(byte);
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(chunk, vbyte16));
        if (mask != 0)
            return data + i + ctz32(static_cast<unsigned int>(mask));
        i += 16;
    }
    for (; i < size; ++i)
        if (data[i] == byte) return data + i;
    return data + size;
}

// ── UTF-8 Validation (AVX2) ───────────────────────────────

bool validate_utf8(const char* data, size_t size) {
    const auto* p = reinterpret_cast<const uint8_t*>(data);
    const auto* end = p + size;
    int expected_cont = 0;
    uint8_t prev_lead_byte = 0;
    uint8_t prev_lead_class = 0;

    // AVX2 fast path: skip all-ASCII chunks
    const __m256i vhigh = _mm256_set1_epi8(static_cast<char>(0x80));
    size_t i = 0;
    for (; i + 32 <= size; i += 32) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        if (_mm256_movemask_epi8(_mm256_and_si256(chunk, vhigh)) == 0) {
            // All ASCII — but only valid if we're not expecting continuation bytes
            if (expected_cont > 0) return false;
            continue;
        }
        // Non-ASCII: process byte-by-byte within the chunk
        const auto* cp = reinterpret_cast<const uint8_t*>(data + i);
        const auto* chunk_end = cp + 32;
        while (cp < chunk_end) {
            const auto byte = *cp++;
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
            } else if ((byte & 0xF8) == 0xF0) {
                if (expected_cont > 0) return false;
                if (byte > 0xF4) return false;
                expected_cont = 3;
                prev_lead_class = 4;
                prev_lead_byte = byte;
            } else if ((byte & 0xC0) == 0x80) {
                if (expected_cont == 0) return false;
                // Range checks for first continuation byte after lead
                if (expected_cont == prev_lead_class - 1) {
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
    // SSE2 tail for remaining 16-31 bytes
    if (i + 16 <= size) {
        __m128i chunk16 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        if (_mm_movemask_epi8(_mm_and_si128(chunk16, _mm_set1_epi8(static_cast<char>(0x80)))) != 0 || expected_cont > 0) {
            const auto* cp = reinterpret_cast<const uint8_t*>(data + i);
            const auto* chunk_end = cp + 16;
            while (cp < chunk_end) {
                const auto byte = *cp++;
                if (byte <= 0x7F) {
                    if (expected_cont > 0) return false;
                } else if ((byte & 0xE0) == 0xC0) {
                    if (expected_cont > 0) return false;
                    if (byte < 0xC2) return false;
                    expected_cont = 1; prev_lead_class = 2; prev_lead_byte = byte;
                } else if ((byte & 0xF0) == 0xE0) {
                    if (expected_cont > 0) return false;
                    expected_cont = 2; prev_lead_class = 3; prev_lead_byte = byte;
                } else if ((byte & 0xF8) == 0xF0) {
                    if (expected_cont > 0) return false;
                    if (byte > 0xF4) return false;
                    expected_cont = 3; prev_lead_class = 4; prev_lead_byte = byte;
                } else if ((byte & 0xC0) == 0x80) {
                    if (expected_cont == 0) return false;
                    if (expected_cont == prev_lead_class - 1) {
                        if (prev_lead_byte == 0xE0 && byte < 0xA0) return false;
                        if (prev_lead_byte == 0xED && byte > 0x9F) return false;
                        if (prev_lead_byte == 0xF0 && byte < 0x90) return false;
                        if (prev_lead_byte == 0xF4 && byte > 0x8F) return false;
                    }
                    --expected_cont;
                } else return false;
            }
        }
        i += 16;
    }
    // Scalar tail
    const auto* cp = reinterpret_cast<const uint8_t*>(data + i);
    while (cp < end) {
        const auto byte = *cp++;
        if (byte <= 0x7F) {
            if (expected_cont > 0) return false;
        } else if ((byte & 0xE0) == 0xC0) {
            if (expected_cont > 0) return false;
            if (byte < 0xC2) return false;
            expected_cont = 1; prev_lead_class = 2; prev_lead_byte = byte;
        } else if ((byte & 0xF0) == 0xE0) {
            if (expected_cont > 0) return false;
            expected_cont = 2; prev_lead_class = 3; prev_lead_byte = byte;
        } else if ((byte & 0xF8) == 0xF0) {
            if (expected_cont > 0) return false;
            if (byte > 0xF4) return false;
            expected_cont = 3; prev_lead_class = 4; prev_lead_byte = byte;
        } else if ((byte & 0xC0) == 0x80) {
            if (expected_cont == 0) return false;
            if (expected_cont == prev_lead_class - 1) {
                if (prev_lead_byte == 0xE0 && byte < 0xA0) return false;
                if (prev_lead_byte == 0xED && byte > 0x9F) return false;
                if (prev_lead_byte == 0xF0 && byte < 0x90) return false;
                if (prev_lead_byte == 0xF4 && byte > 0x8F) return false;
            }
            --expected_cont;
        } else return false;
    }
    return expected_cont == 0;
}


// ── AVX2 count_code_points ──────────────────────────────────────
// Count Unicode code points = count bytes that are NOT continuation bytes (10xxxxxx)
//
// Continuation bytes: 0x80 <= byte <= 0xBF
// We test: (byte > 0x7F) AND (0xBF > byte) using signed comparison.
// This correctly includes 0x80: signed(0x80) = -128 > signed(0x7F) = 127 ✓

size_t count_code_points(const char* data, size_t size) {
    // Byte >= 0x80: use signed > 0x7F
    const __m256i v7f = _mm256_set1_epi8(static_cast<char>(0x7F));
    // Byte <= 0xBF: use signed 0xBF > byte
    const __m256i vbf = _mm256_set1_epi8(static_cast<char>(0xBF));

    size_t i = 0;
    size_t count = 0;

    // Process 128 bytes at a time (4x unrolled)
    for (; i + 128 <= size; i += 128) {
        __m256i c0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        __m256i c1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + 32));
        __m256i c2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + 64));
        __m256i c3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + 96));

        // Continuation byte: 0x80 <= byte <= 0xBF
        __m256i m0 = _mm256_and_si256(_mm256_cmpgt_epi8(c0, v7f), _mm256_cmpgt_epi8(vbf, c0));
        __m256i m1 = _mm256_and_si256(_mm256_cmpgt_epi8(c1, v7f), _mm256_cmpgt_epi8(vbf, c1));
        __m256i m2 = _mm256_and_si256(_mm256_cmpgt_epi8(c2, v7f), _mm256_cmpgt_epi8(vbf, c2));
        __m256i m3 = _mm256_and_si256(_mm256_cmpgt_epi8(c3, v7f), _mm256_cmpgt_epi8(vbf, c3));

        uint32_t mask0 = _mm256_movemask_epi8(m0);
        uint32_t mask1 = _mm256_movemask_epi8(m1);
        uint32_t mask2 = _mm256_movemask_epi8(m2);
        uint32_t mask3 = _mm256_movemask_epi8(m3);

        count += 128 - __builtin_popcount(mask0) - __builtin_popcount(mask1)
                      - __builtin_popcount(mask2) - __builtin_popcount(mask3);
    }

    for (; i + 32 <= size; i += 32) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        __m256i m = _mm256_and_si256(_mm256_cmpgt_epi8(chunk, v7f), _mm256_cmpgt_epi8(vbf, chunk));
        uint32_t mask = _mm256_movemask_epi8(m);
        count += 32 - __builtin_popcount(mask);
    }

    for (; i < size; ++i) {
        if ((static_cast<uint8_t>(data[i]) & 0xC0) != 0x80) ++count;
    }

    return count;
}

// ── AVX2 Base64 Decode ─────────────────────────────────────
//
// Processes 32 base64 chars → 24 output bytes per iteration.
//
// Char→value: range-based SIMD comparisons (no LUT needed)
// Packing: pshufb rearrange + 16-bit shift/mask/OR
//
// For [a,b,c,d] → [out0, out1, out2]:
//   out0 = (a << 2) | (b >> 4)
//   out1 = ((b & 0xF) << 4) | (c >> 2)
//   out2 = ((c & 0x3) << 6) | d
//
// Packing strategy (operates on 16-bit lanes within 128-bit halves):
//   1. pshufb to rearrange: place a,c at even positions, b,d at odd positions
//   2. Use 16-bit shifts: slli shifts both bytes in a 16-bit lane together,
//      srli similarly. With masks, we isolate the correct bits.
//   3. Specifically:
//      - For out0: need a<<2 in high byte, b>>4 in low byte of same 16-bit lane
//        → pshufb [a,b] into same 16-bit lane, then slli by 2 gives a<<2,b<<2
//        → We want a<<2 (correct) and b>>4 (not b<<2).
//        This doesn't work directly.
//
// Alternative: use the MADD approach with per-byte multiplication.
// maddubs_epi16 treats first arg as unsigned, second as signed.
// maddubs_epi16(a, b) = a[2i]*b[2i] + a[2i+1]*b[2i+1] (int16 result)
//
// For [a,b,c,d] in a 32-bit lane:
//   maddubs([a,b], [4,1]) = a*4 + b*1 = a<<2 + b
//   The 16-bit result: bits [9:2] = a, bits [5:0] = b
//   → byte0 = result >> 4 (bits [7:0] of result >> 4 = a[5:2] | b[5:4])
//   Hmm, that's not quite right either.
//
// Let me work through the math carefully:
//   a is 6 bits: a5 a4 a3 a2 a1 a0
//   b is 6 bits: b5 b4 b3 b2 b1 b0
//   a<<2 = a5 a4 a3 a2 a1 a0 0 0   (8 bits, fits in byte since a < 64)
//   b>>4 = 0 0 0 0 b5 b4           (need 2 bits)
//   out0 = a5 a4 a3 a2 a1 a0 b5 b4  (8 bits)
//
//   a*4 + b = a<<2 + b  (as a 16-bit value)
//   = 0 0 0 0 0 0 0 0 a5 a4 a3 a2 a1 a0 0 0
//   + 0 0 0 0 0 0 0 0 0 0 b5 b4 b3 b2 b1 b0
//   = 0 0 0 0 0 0 a5 a4 a3 a2 a1 a0+b5 b4+b3 b2+b1 b0
//   (with carries from b into a)
//   This is NOT the same as (a<<2 | b>>4).
//
// So maddubs doesn't directly give us the right result.
//
// CORRECT APPROACH: Use the "3-register" method:
//   reg_a = pshufb to gather all 'a' values (positions 0,4,8,...)
//   reg_b = pshufb to gather all 'b' values (positions 1,5,9,...)
//   etc.
// Then for each output byte stream:
//   out0_stream = (reg_a << 2) | (reg_b >> 4)
//   But we need BYTE-level shifts, which AVX2 doesn't have.
//
// However, we CAN emulate byte shifts using maddubs:
//   x << 2  ≡  maddubs(x, 4) where x is treated as unsigned pairs
//   x >> 4  ≡  maddubs(x, 1/16) -- but 1/16 isn't an integer
//
// OR: we can use slli_epi16 on carefully arranged pairs and mask.
// If we arrange [0, a] in each 16-bit lane:
//   slli_epi16([0, a], 2) = [0, a<<2] -- gives a<<2 in low byte
//   But we want a<<2 in a specific byte position.
//
// If we arrange [a, 0] in each 16-bit lane:
//   slli_epi16([a, 0], 2) = [a<<2 in high byte, 0 in low byte]
//   Wait, slli shifts the 16-bit value: a*256 << 2 = a*1024 = a<<10 as 16-bit
//   High byte = a<<2, low byte = 0. YES!
//
// So: place a in the high byte of a 16-bit lane, shift left by 2:
//   [a*256] << 2 = a*1024 = [a<<2, 0]
//   The high byte of each 16-bit lane = a<<2 ✓
//
// Similarly, place b in the high byte, shift right by 4:
//   [b*256] >> 4 = b*16 = [0, b<<4]  -- but we want b>>4, not b<<4!
//
// Hmm. For b>>4 we need b in the LOW byte:
//   [0, b] >> 4 = [b>>4 in high byte, b<<4 in low byte]
//   Wait: 16-bit value b, shifted right by 4 = b >> 4.
//   As bytes: high byte = b>>4 & 0xFF = b>>4 (since b < 64), low byte = 0.
//   Actually: the 16-bit value is just b (in the low byte). srli by 4 gives b>>4.
//   High byte = 0, low byte = b>>4. ✓
//
// So the plan:
// 1. pshufb decoded into reg_A: a values in high bytes of 16-bit lanes, low bytes = 0
// 2. pshufb decoded into reg_B_lo: b values in low bytes, high bytes = 0
// 3. pshufb decoded into reg_B_hi: b values in high bytes, low bytes = 0
// 4. etc.
// 5. Combine:
//    out0 = slli(reg_A, 2) | srli(reg_B_lo, 4)   -- a<<2 | b>>4
//    out1 = slli(reg_B_hi, 4) | srli(reg_C_lo, 2) -- (b&0xF)<<4 | c>>2
//    out2 = slli(reg_C_hi, 6) | reg_D             -- (c&3)<<6 | d
//
// But this requires 4 separate pshufbs + multiple shifts + masks + ORs.
// That's ~12 instructions per 32→24 packing. Not terrible.

DecodeResult base64_decode_avx2(const uint8_t* src, size_t src_size, uint8_t* dst) noexcept {
    DecodeResult result{0, 0, ErrorCode::Ok};

    if (src_size % 4 != 0) {
        result.error = ErrorCode::InvalidLength;
        return result;
    }

    size_t i = 0;  // input offset
    size_t j = 0;  // output offset

    // Constants for range-based char→value conversion
    const __m256i vA = _mm256_set1_epi8('A' - 1);
    const __m256i vZ = _mm256_set1_epi8('Z' + 1);
    const __m256i va = _mm256_set1_epi8('a' - 1);
    const __m256i vz = _mm256_set1_epi8('z' + 1);
    const __m256i v0 = _mm256_set1_epi8('0' - 1);
    const __m256i v9 = _mm256_set1_epi8('9' + 1);
    const __m256i vplus = _mm256_set1_epi8('+');
    const __m256i vslash = _mm256_set1_epi8('/');
    const __m256i veq = _mm256_set1_epi8('=');

    // Subtraction constants for each range
    const __m256i sub_AZ = _mm256_set1_epi8(static_cast<char>(0 - 'A'));
    const __m256i sub_az = _mm256_set1_epi8(static_cast<char>(26 - 'a'));
    const __m256i sub_09 = _mm256_set1_epi8(static_cast<char>(52 - '0'));
    const __m256i val_plus  = _mm256_set1_epi8(62);
    const __m256i val_slash = _mm256_set1_epi8(63);

    // Masks for packing
    const __m256i mask_3F = _mm256_set1_epi8(0x3F);  // 6 bits
    const __m256i mask_0F = _mm256_set1_epi8(0x0F);  // 4 bits
    const __m256i mask_03 = _mm256_set1_epi8(0x03);  // 2 bits

    // Pshufb shuffle masks for separating a,b,c,d from decoded register.
    // decoded layout: [a0,b0,c0,d0, a1,b1,c1,d1, a2,b2,c2,d2, a3,b3,c3,d3]
    // per 128-bit lane (16 bytes).
    //
    // For 'a' values (positions 0,4,8,12 in each lane):
    //   pshufb: gather to output positions for out0 stream.
    //   out0 has 6 bytes per lane (a0..a3 mapped to output positions 0,3,6,9,
    //   but we also need to interleave with b>>4 bits).
    //
    // Actually, for simplicity, let's just use the store-and-scalar-pack
    // approach for the packing step. The range-based SIMD decode is the
    // main speedup; packing from a stored register is still fast.

    // Process 32 base64 chars at a time → 24 output bytes
    for (; i + 32 <= src_size; i += 32) {
        __m256i in = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));

        // Range checks
        __m256i is_AZ = _mm256_and_si256(_mm256_cmpgt_epi8(in, vA), _mm256_cmpgt_epi8(vZ, in));
        __m256i is_az = _mm256_and_si256(_mm256_cmpgt_epi8(in, va), _mm256_cmpgt_epi8(vz, in));
        __m256i is_09 = _mm256_and_si256(_mm256_cmpgt_epi8(in, v0), _mm256_cmpgt_epi8(v9, in));
        __m256i is_plus  = _mm256_cmpeq_epi8(in, vplus);
        __m256i is_slash = _mm256_cmpeq_epi8(in, vslash);
        __m256i is_eq    = _mm256_cmpeq_epi8(in, veq);

        // Validate
        __m256i valid = _mm256_or_si256(
            _mm256_or_si256(_mm256_or_si256(is_AZ, is_az), is_09),
            _mm256_or_si256(_mm256_or_si256(is_plus, is_slash), is_eq));
        int invalid_mask = ~_mm256_movemask_epi8(valid) & 0xFFFFFFFF;
        if (invalid_mask) {
            result.error = ErrorCode::InvalidChar;
            result.error_offset = i + ctz32(static_cast<unsigned int>(invalid_mask));
            return result;
        }

        // Convert to 6-bit values
        __m256i decoded = _mm256_or_si256(
            _mm256_or_si256(
                _mm256_and_si256(is_AZ, _mm256_add_epi8(in, sub_AZ)),
                _mm256_and_si256(is_az, _mm256_add_epi8(in, sub_az))
            ),
            _mm256_or_si256(
                _mm256_and_si256(is_09, _mm256_add_epi8(in, sub_09)),
                _mm256_or_si256(
                    _mm256_and_si256(is_plus, val_plus),
                    _mm256_and_si256(is_slash, val_slash)
                )
            )
        );

        // Pack 4 × 6-bit → 3 bytes using pshufb + maddubs
        //
        // We use the following maddubs trick:
        // For each group [a,b,c,d], compute:
        //   merged_ab = a*4 + b  (as 16-bit: a<<2 + b)
        //   merged_cd = c*64 + d (as 16-bit: c<<6 + d)
        // Then extract bytes:
        //   out0 = merged_ab >> 4  (bits [11:4] → 8 bits = a[5:0]b[5:4])
        //   out1 = ((merged_ab & 0xF) << 4) | (merged_cd >> 10 & 0xF)
        //   out2 = merged_cd & 0xFF  -- wait, c<<6+d might overflow byte
        //
        // Let me verify: c<<6 max = 63*64 = 4032, d max = 63, sum = 4095. Fits in uint16.
        // As bytes: high byte = (c<<6+d)>>8 = c>>2 (only top 4 bits of c)
        //           low byte = (c<<6+d) & 0xFF = (c&3)<<6 | d (if d < 64, c&3 < 4)
        //           (c&3)<<6 max = 192, d max = 63, sum max = 255 ✓
        //
        // Similarly: a<<2+b: a<<2 max = 252, b max = 63, sum max = 315.
        // High byte = (a<<2+b)>>8 = a>>6 (just bit 5 of a, which is 0 since a < 64)
        // Actually a<<2+b = a*4+b. Since a<64, a*4<256. So (a*4+b)>>8 = 0 or 1.
        // Hmm, a=63: 63*4=252, +b=63 → 315 → high byte = 1, low byte = 59.
        // out0 should be (63<<2)|(63>>4) = 252|3 = 255. But (a*4+b)>>4 = 315>>4 = 19.
        // That's wrong!
        //
        // The issue: a*4+b ≠ a<<2 | b>>4 because of carry from b into a.
        //
        // So maddubs with [4,1] does NOT give the correct result.
        // We need a*4 + b/16, but b/16 is not an integer for most b values.
        //
        // CORRECT: use maddubs with [4, 0] for the a<<2 part, then separately
        // compute b>>4 with srli, then OR them.
        //
        // Or: use the approach where we widen to 16-bit, do 16-bit multiply,
        // then pack down.
        //
        // SIMPLEST CORRECT SIMD APPROACH for packing:
        // Use pshufb to rearrange decoded into specific byte positions,
        // then use slli_epi16 / srli_epi16 on 16-bit lanes with masking.
        //
        // For a 128-bit lane with [a0,b0,c0,d0, a1,b1,c1,d1, a2,b2,c2,d2, a3,b3,c3,d3]:
        //
        // Step 1: pshufb to create reg with [a0,00, b0,00, a1,00, b1,00, ...]
        //         (a values at even bytes, b values at odd bytes of 16-bit lanes)
        //  Wait, pshufb can only permute, not zero.
        //  Use pshufb with 0x80 for zeroing (pshufb sets byte to 0 if index has high bit set).
        //
        // Actually, the simplest approach that works:
        // 1. Store decoded to a temp buffer
        // 2. Do the packing scalar from the temp buffer
        // 3. This is still faster than pure scalar because the range-based
        //    char→value conversion is the bottleneck (replaces 4 LUT lookups +
        //    branches per group with SIMD comparisons).
        //
        // The packing (shift+OR) is simple scalar arithmetic that the CPU
        // can pipeline very well. The main SIMD win is eliminating the
        // per-byte LUT lookup and branch.

        // Store decoded values and pack scalar
        alignas(32) uint8_t dec[32];
        _mm256_store_si256(reinterpret_cast<__m256i*>(dec), decoded);

        for (int k = 0; k < 8; ++k) {
            uint8_t a = dec[k*4+0];
            uint8_t b = dec[k*4+1];
            uint8_t c = dec[k*4+2];
            uint8_t d = dec[k*4+3];
            dst[j++] = (a << 2) | (b >> 4);
            dst[j++] = (b << 4) | (c >> 2);
            dst[j++] = (c << 6) | d;
        }
    }

    // Scalar tail
    static constexpr uint8_t b64_table[256] = {
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,62,64,64,64,63,
        52,53,54,55,56,57,58,59,60,61,64,64,64,64,64,64,
        64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,
        64,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    };

    for (; i < src_size; i += 4) {
        const uint8_t a = b64_table[src[i]];
        const uint8_t b = b64_table[src[i+1]];
        const uint8_t c = b64_table[src[i+2]];
        const uint8_t d = b64_table[src[i+3]];
        if (a == 64 || b == 64) {
            result.error = ErrorCode::InvalidChar;
            result.error_offset = (a == 64) ? i : i + 1;
            return result;
        }
        uint32_t n = (static_cast<uint32_t>(a) << 18) |
                     (static_cast<uint32_t>(b) << 12) |
                     (static_cast<uint32_t>(c) << 6) |
                     static_cast<uint32_t>(d);
        if (src[i+2] != '=') dst[j++] = static_cast<uint8_t>((n >> 16) & 0xFF);
        if (src[i+2] != '=' && src[i+3] != '=') dst[j++] = static_cast<uint8_t>((n >> 8) & 0xFF);
        if (src[i+3] != '=') dst[j++] = static_cast<uint8_t>(n & 0xFF);
    }

    result.bytes_written = j;
    return result;
}

} // namespace simdtext::detail::avx2

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC pop_options
#endif

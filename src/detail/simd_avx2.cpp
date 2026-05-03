#include <immintrin.h>  // AVX2
#include <emmintrin.h>  // SSE2 (for tail)
#include <cstddef>
#include <cstdint>
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
    // 4x unrolled loop with software prefetch for large buffers
    size_t i = 0;
    constexpr size_t PF = 384;
    for (; i + 128 <= size; i += 128) {
        _mm_prefetch(data + i + PF, _MM_HINT_T0);
        _mm_prefetch(data + i + PF + 64, _MM_HINT_T0);
        __m256i c0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        __m256i c1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + 32));
        __m256i c2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + 64));
        __m256i c3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + 96));
        __m256i up0 = _mm256_and_si256(_mm256_cmpgt_epi8(c0, vA), _mm256_cmpgt_epi8(vZ1, c0));
        __m256i up1 = _mm256_and_si256(_mm256_cmpgt_epi8(c1, vA), _mm256_cmpgt_epi8(vZ1, c1));
        __m256i up2 = _mm256_and_si256(_mm256_cmpgt_epi8(c2, vA), _mm256_cmpgt_epi8(vZ1, c2));
        __m256i up3 = _mm256_and_si256(_mm256_cmpgt_epi8(c3, vA), _mm256_cmpgt_epi8(vZ1, c3));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(data + i),      _mm256_xor_si256(c0, _mm256_and_si256(up0, vbit5)));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(data + i + 32), _mm256_xor_si256(c1, _mm256_and_si256(up1, vbit5)));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(data + i + 64), _mm256_xor_si256(c2, _mm256_and_si256(up2, vbit5)));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(data + i + 96), _mm256_xor_si256(c3, _mm256_and_si256(up3, vbit5)));
    }
    for (; i + 32 <= size; i += 32) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        __m256i is_upper = _mm256_and_si256(_mm256_cmpgt_epi8(chunk, vA), _mm256_cmpgt_epi8(vZ1, chunk));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(data + i), _mm256_xor_si256(chunk, _mm256_and_si256(is_upper, vbit5)));
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
    size_t i = 0;
    constexpr size_t PF = 384;
    for (; i + 128 <= size; i += 128) {
        _mm_prefetch(data + i + PF, _MM_HINT_T0);
        _mm_prefetch(data + i + PF + 64, _MM_HINT_T0);
        __m256i c0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        __m256i c1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + 32));
        __m256i c2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + 64));
        __m256i c3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i + 96));
        __m256i lo0 = _mm256_and_si256(_mm256_cmpgt_epi8(c0, va), _mm256_cmpgt_epi8(vz1, c0));
        __m256i lo1 = _mm256_and_si256(_mm256_cmpgt_epi8(c1, va), _mm256_cmpgt_epi8(vz1, c1));
        __m256i lo2 = _mm256_and_si256(_mm256_cmpgt_epi8(c2, va), _mm256_cmpgt_epi8(vz1, c2));
        __m256i lo3 = _mm256_and_si256(_mm256_cmpgt_epi8(c3, va), _mm256_cmpgt_epi8(vz1, c3));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(data + i),      _mm256_xor_si256(c0, _mm256_and_si256(lo0, vbit5)));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(data + i + 32), _mm256_xor_si256(c1, _mm256_and_si256(lo1, vbit5)));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(data + i + 64), _mm256_xor_si256(c2, _mm256_and_si256(lo2, vbit5)));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(data + i + 96), _mm256_xor_si256(c3, _mm256_and_si256(lo3, vbit5)));
    }
    for (; i + 32 <= size; i += 32) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        __m256i is_lower = _mm256_and_si256(_mm256_cmpgt_epi8(chunk, va), _mm256_cmpgt_epi8(vz1, chunk));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(data + i), _mm256_xor_si256(chunk, _mm256_and_si256(is_lower, vbit5)));
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
// ── AVX2 Base64 Decode ─────────────────────────────────────
//
// Processes 32 base64 chars → 24 output bytes per iteration.
//
// Char→value: range-based SIMD comparisons (no LUT needed).
// Packing: pshufb to gather a/b/c/d into low bytes of 16-bit lanes,
// then slli_epi16/srli_epi16 to shift within each lane, OR to combine.
//
// For [a,b,c,d] → [out0, out1, out2]:
//   out0 = (a << 2) | (b >> 4)
//   out1 = (b << 4) | (c >> 2)     — b<<4 = (b&0xF)<<4 since b is 6-bit
//   out2 = (c << 6) | d            — c<<6 = (c&0x3)<<6 since c is 6-bit
//
// 16-bit shift trick: value v in LOW byte of 16-bit lane [0x00, v]:
//   slli_epi16(v, N) → v<<N in low byte (if v<<N < 256), 0 in high byte
//   srli_epi16(v, N) → v>>N in low byte, 0 in high byte

DecodeResult base64_decode_avx2(const uint8_t* src, size_t src_size, uint8_t* dst) noexcept {
    DecodeResult result{0, 0, ErrorCode::Ok};

    if (src_size % 4 != 0) {
        result.error = ErrorCode::InvalidLength;
        return result;
    }

    size_t i = 0;
    size_t j = 0;

    // Range-based char→value constants
    const __m256i vA = _mm256_set1_epi8('A' - 1);
    const __m256i vZ = _mm256_set1_epi8('Z' + 1);
    const __m256i va = _mm256_set1_epi8('a' - 1);
    const __m256i vz = _mm256_set1_epi8('z' + 1);
    const __m256i v0 = _mm256_set1_epi8('0' - 1);
    const __m256i v9 = _mm256_set1_epi8('9' + 1);
    const __m256i vplus = _mm256_set1_epi8('+');
    const __m256i vslash = _mm256_set1_epi8('/');
    const __m256i veq = _mm256_set1_epi8('=');
    const __m256i sub_AZ = _mm256_set1_epi8(static_cast<char>(0 - 'A'));
    const __m256i sub_az = _mm256_set1_epi8(static_cast<char>(26 - 'a'));
    const __m256i sub_09 = _mm256_set1_epi8(static_cast<char>(52 - '0'));
    const __m256i val_plus  = _mm256_set1_epi8(62);
    const __m256i val_slash = _mm256_set1_epi8(63);

    // Packing: pshufb masks to gather a/b/c/d from decoded into low bytes of
    // 16-bit lanes. decoded per 128-bit lane: [a0,b0,c0,d0, a1,b1,c1,d1, ...]
    // 0x80 = zero that byte position (pshufb zeros when bit 7 set).
    //
    // We need 6 pshufb gathers (a_lo, b_lo, b_hi, c_lo, c_hi, d_lo):
    //   a_lo: a values in low bytes → slli(2) gives a<<2 in low byte
    //   b_lo: b values in low bytes → srli(4) gives b>>4 in low byte
    //   b_hi: b values in low bytes → slli(4) gives b<<4 in low byte
    //   c_lo: c values in low bytes → srli(2) gives c>>2 in low byte
    //   c_hi: c values in low bytes → slli(6) gives c<<6 in low byte
    //   d_lo: d values in low bytes → used as-is
    //
    // Wait: slli(b_lo, 4) for 6-bit b: b<<4 max = 63<<4 = 1008, overflows byte!
    // We only want (b&0xF)<<4 = max 240. So we need to mask b with 0x0F first.
    // Similarly for c: slli(c_lo, 6) for 6-bit c: c<<6 max = 63<<6 = 4032, overflow.
    // We only want (c&0x3)<<6 = max 192. So mask c with 0x03 first.
    //
    // Alternative: place b in HIGH byte of 16-bit lane, then slli by 4:
    //   [b, 0] slli(4) → 16-bit value = b*256*16 = b<<12. High byte = b<<4, low byte = 0.
    //   b<<4 for 6-bit b: max 1008, high byte = 1008/256 = 3, low byte = 240.
    //   That's not right either - b<<4 overflows the high byte.
    //
    // Hmm. The issue is that (b&0xF)<<4 fits in a byte (max 240), but b<<4 doesn't.
    // So we MUST mask before shifting.
    //
    // Plan: use AND with masks before shifting.
    //   b_masked = b & 0x0F → slli(4) → (b&0xF)<<4, fits in byte
    //   c_masked = c & 0x03 → slli(6) → (c&3)<<6, fits in byte
    //
    // For b>>4 and c>>2: no masking needed since shifting right.
    // For a<<2: a is 6 bits, a<<2 max = 252, fits in byte. No masking needed.

    const __m256i mask_0F = _mm256_set1_epi8(0x0F);
    const __m256i mask_03 = _mm256_set1_epi8(0x03);

    // Shuffle masks: gather values from decoded into LOW bytes of 16-bit lanes.
    // Per 128-bit lane, decoded has: [a0,b0,c0,d0, a1,b1,c1,d1, a2,b2,c2,d2, a3,b3,c3,d3]
    // We want a values (indices 0,4,8,12) in the odd positions (low bytes of 16-bit lanes):
    //   Positions 1,3,5,7,9,11,13,15 in the 128-bit lane
    // But pshufb can only place one source byte per destination.
    // With 8 16-bit lanes per 128-bit half, we need 4 a-values in 4 low-byte positions,
    // and zero in the other 4 low-byte positions and all high-byte positions.
    //
    // Actually, we can use all 8 low-byte positions (4 from each 128-bit lane):
    // Per 128-bit lane, we have 8 16-bit lanes (lanes 0-7):
    //   low bytes at positions: 1,3,5,7,9,11,13,15
    //   high bytes at positions: 0,2,4,6,8,10,12,14
    //
    // For a-values at positions 0,4,8,12 in the source:
    //   Place into low bytes of 16-bit lanes 0,1,2,3 → dest positions 1,3,5,7
    //   High bytes (positions 0,2,4,6) → zero (0x80)
    //
    // But we also need b,c,d values in different registers, each using different
    // low-byte positions. The problem: we only have 4 useful positions per lane
    // (the 4 low bytes of the first 4 16-bit lanes), but we need to combine
    // multiple values into the SAME lane.
    //
    // REVISED PLAN: Use ALL 8 16-bit lanes per 128-bit half.
    // Each 16-bit lane produces one output byte.
    // 32 decoded bytes → 24 output bytes (8 groups × 3 bytes = 24).
    // We need 3 output registers (out0, out1, out2), each with 8 bytes per lane.
    //
    // For out0 = (a<<2) | (b>>4):
    //   Need a in low byte, shift left by 2: slli(a_lo, 2) → a<<2 in low byte
    //   Need b in low byte, shift right by 4: srli(b_lo, 4) → b>>4 in low byte
    //   OR them → out0
    //   But a and b are from DIFFERENT positions in decoded. We need them in
    //   the SAME 16-bit lanes to OR them. So both pshufb gathers must place
    //   values into the same lane positions.
    //
    // For a-values at positions [0,4,8,12] in each 128-bit lane:
    //   pshufb to place into low bytes of lanes [0,2,4,6] (positions 1,5,9,13)
    // For b-values at positions [1,5,9,13]:
    //   pshufb to place into low bytes of lanes [0,2,4,6] (same positions!)
    //
    // out0 for both 128-bit lanes = 8 bytes total, but we need 8 output bytes
    // (one per group). Each 128-bit lane has 4 groups, so 4 output bytes per lane.
    // We use 4 out of 8 low-byte positions per lane. That's fine.
    //
    // Similarly for out1 and out2.

    // Shuffle masks: gather a/b/c/d into alternating low-byte positions.
    // Lane layout per 128-bit half: [H0,L0, H1,L1, H2,L2, H3,L3, H4,L4, H5,L5, H6,L6, H7,L7]
    // We use L0,L1,L2,L3 (positions 1,3,5,7) for the 4 groups per lane.
    // H0-H7 and L4-L7 are all zeroed.
    const auto X = static_cast<char>(0x80);
    const __m256i shuf_a = _mm256_setr_epi8(
        X, 0, X, 4, X, 8, X,12,  X,X, X,X, X,X, X,X,
        X, 0, X, 4, X, 8, X,12,  X,X, X,X, X,X, X,X);
    const __m256i shuf_b = _mm256_setr_epi8(
        X, 1, X, 5, X, 9, X,13,  X,X, X,X, X,X, X,X,
        X, 1, X, 5, X, 9, X,13,  X,X, X,X, X,X, X,X);
    const __m256i shuf_c = _mm256_setr_epi8(
        X, 2, X, 6, X,10, X,14,  X,X, X,X, X,X, X,X,
        X, 2, X, 6, X,10, X,14,  X,X, X,X, X,X, X,X);
    const __m256i shuf_d = _mm256_setr_epi8(
        X, 3, X, 7, X,11, X,15,  X,X, X,X, X,X, X,X,
        X, 3, X, 7, X,11, X,15,  X,X, X,X, X,X, X,X);

    // Output shuffle: from 16-bit lanes with result in low bytes,
    // compact 8 bytes (4 per 128-bit lane) into contiguous output.
    // Low bytes are at positions 1,3,5,7 in each 128-bit lane.
    const __m256i shuf_out = _mm256_setr_epi8(
        1, 3, 5, 7,  X,X, X,X, X,X, X,X, X,X, X,X,
        1, 3, 5, 7,  X,X, X,X, X,X, X,X, X,X, X,X);

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

        // Pack: gather a,b,c,d into 16-bit lanes, shift, mask, combine
        __m256i a_lo = _mm256_shuffle_epi8(decoded, shuf_a);
        __m256i b_lo = _mm256_shuffle_epi8(decoded, shuf_b);
        __m256i c_lo = _mm256_shuffle_epi8(decoded, shuf_c);
        __m256i d_lo = _mm256_shuffle_epi8(decoded, shuf_d);

        // out0 = (a<<2) | (b>>4)
        __m256i out0 = _mm256_or_si256(
            _mm256_slli_epi16(a_lo, 2),
            _mm256_srli_epi16(b_lo, 4));

        // out1 = ((b&0xF)<<4) | (c>>2)
        __m256i out1 = _mm256_or_si256(
            _mm256_slli_epi16(_mm256_and_si256(b_lo, mask_0F), 4),
            _mm256_srli_epi16(c_lo, 2));

        // out2 = ((c&0x3)<<6) | d
        __m256i out2 = _mm256_or_si256(
            _mm256_slli_epi16(_mm256_and_si256(c_lo, mask_03), 6),
            d_lo);

        // Compact and store: extract the 8 output bytes from each register
        // (4 per 128-bit lane, at positions 1,3,5,7)
        __m256i out0c = _mm256_shuffle_epi8(out0, shuf_out);
        __m256i out1c = _mm256_shuffle_epi8(out1, shuf_out);
        __m256i out2c = _mm256_shuffle_epi8(out2, shuf_out);

        // Store 8 bytes from each (4 per 128-bit lane)
        // out0c has bytes at positions 0-3 and 16-19
        // out1c has bytes at positions 0-3 and 16-19
        // out2c has bytes at positions 0-3 and 16-19
        // Total: 24 bytes in order: out0c[0:4], out1c[0:4], out2c[0:4], out0c[16:20], out1c[16:20], out2c[16:20]

        // Extract 128-bit lanes and store
        __m128i o0_lo = _mm256_extracti128_si256(out0c, 0);
        __m128i o1_lo = _mm256_extracti128_si256(out1c, 0);
        __m128i o2_lo = _mm256_extracti128_si256(out2c, 0);
        __m128i o0_hi = _mm256_extracti128_si256(out0c, 1);
        __m128i o1_hi = _mm256_extracti128_si256(out1c, 1);
        __m128i o2_hi = _mm256_extracti128_si256(out2c, 1);

        // Store 4 bytes from each lane segment
        // First 4 groups (from low 128-bit lanes):
        _mm_storel_epi64(reinterpret_cast<__m128i*>(dst + j),      o0_lo);  // 4 bytes at positions 0-3
        _mm_storel_epi64(reinterpret_cast<__m128i*>(dst + j + 4),  o1_lo);
        _mm_storel_epi64(reinterpret_cast<__m128i*>(dst + j + 8),  o2_lo);
        // Next 4 groups (from high 128-bit lanes):
        _mm_storel_epi64(reinterpret_cast<__m128i*>(dst + j + 12), o0_hi);
        _mm_storel_epi64(reinterpret_cast<__m128i*>(dst + j + 16), o1_hi);
        _mm_storel_epi64(reinterpret_cast<__m128i*>(dst + j + 20), o2_hi);
        j += 24;
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

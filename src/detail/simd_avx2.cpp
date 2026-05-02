#include <immintrin.h>  // AVX2
#include <emmintrin.h>  // SSE2 (for tail)
#include <cstddef>

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
    // XOR case flip: bit 5 is the case bit
    const __m256i vupper = _mm256_set1_epi8('a' - 1);    // > 'A'-1 means >= 'A'
    const __m256i vupper_end = _mm256_set1_epi8('z' + 1); // < 'Z'+1 means <= 'Z' ... wait
    // Actually for lowercase: we want is_upper = (c >= 'A') & (c <= 'Z')
    // But AVX2 has only signed cmpgt. Use: not_above = cmpgt(vupper_end, c) means c < 'Z'+1 ↔ c <= 'Z'
    //  not_below = cmpgt(c, vupper) means c > 'A'-1 ↔ c >= 'A'
    // Wait, AVX2 doesn't have cmplt. We use: _mm256_cmpgt_epi8(a, b) → a > b (signed)
    // cmplt(chunk, vupper_end) → chunk < vupper_end → use cmpgt(vupper_end, chunk)
    const __m256i vA = _mm256_set1_epi8('A' - 1);
    const __m256i vZ1 = _mm256_set1_epi8('Z' + 1);
    const __m256i vbit5 = _mm256_set1_epi8(0x20);
    size_t i = 0;
    for (; i + 32 <= size; i += 32) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        // is_upper: chunk >= 'A' AND chunk <= 'Z'
        // chunk >= 'A' ↔ chunk > 'A'-1 ↔ cmpgt(chunk, vA)
        // chunk <= 'Z' ↔ NOT (chunk > 'Z') ↔ NOT cmpgt(chunk, vZ) ↔ cmpgt(vZ1, chunk)
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

} // namespace simdtext::detail::avx2

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC pop_options
#endif

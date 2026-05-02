#include <emmintrin.h>  // SSE2
#include <cstddef>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace simdtext::detail::sse2 {

namespace {
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
} // anonymous namespace

size_t count_byte(const char* data, size_t size, char byte) {
    const __m128i vbyte = _mm_set1_epi8(byte);
    size_t count = 0;
    size_t i = 0;
    // 4x unrolled loop to hide popcount latency
    for (; i + 64 <= size; i += 64) {
        __m128i c0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        __m128i c1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i + 16));
        __m128i c2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i + 32));
        __m128i c3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i + 48));
        unsigned int m0 = static_cast<unsigned int>(_mm_movemask_epi8(_mm_cmpeq_epi8(c0, vbyte)));
        unsigned int m1 = static_cast<unsigned int>(_mm_movemask_epi8(_mm_cmpeq_epi8(c1, vbyte)));
        unsigned int m2 = static_cast<unsigned int>(_mm_movemask_epi8(_mm_cmpeq_epi8(c2, vbyte)));
        unsigned int m3 = static_cast<unsigned int>(_mm_movemask_epi8(_mm_cmpeq_epi8(c3, vbyte)));
        count += popcount32(m0) + popcount32(m1) + popcount32(m2) + popcount32(m3);
    }
    for (; i + 16 <= size; i += 16) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        __m128i eq = _mm_cmpeq_epi8(chunk, vbyte);
        count += popcount32(static_cast<unsigned int>(_mm_movemask_epi8(eq)));
    }
    for (; i < size; ++i)
        if (data[i] == byte) ++count;
    return count;
}

bool is_ascii(const char* data, size_t size) {
    const __m128i vhigh = _mm_set1_epi8(static_cast<char>(0x80));
    size_t i = 0;
    // Accumulate across 4 vectors before branching
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
    // XOR-based case flip: bit 5 is the case bit
    // is_upper = (c >= 'A') & (c <= 'Z')  →  mask = 0xFF if upper, else 0x00
    // result = c ^ (mask & 0x20)
    const __m128i vupper = _mm_set1_epi8('A' - 1);  // compare > 'A'-1 means >= 'A'
    const __m128i vupper_end = _mm_set1_epi8('Z' + 1); // compare < 'Z'+1 means <= 'Z'
    const __m128i vbit5 = _mm_set1_epi8(0x20);
    size_t i = 0;
    for (; i + 16 <= size; i += 16) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        // Signed comparison: gt means strictly greater, lt means strictly less
        // is_upper = !(chunk > 'Z') & !(chunk < 'A') = (chunk <= 'Z') & (chunk >= 'A')
        __m128i not_above = _mm_cmplt_epi8(chunk, vupper_end);   // chunk < 'Z'+1 ↔ chunk <= 'Z'
        __m128i not_below = _mm_cmpgt_epi8(chunk, vupper);       // chunk > 'A'-1 ↔ chunk >= 'A'
        __m128i is_upper = _mm_and_si128(not_above, not_below);
        __m128i lowered = _mm_xor_si128(chunk, _mm_and_si128(is_upper, vbit5));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(data + i), lowered);
    }
    for (; i < size; ++i) {
        auto c = static_cast<unsigned char>(data[i]);
        if (c >= 'A' && c <= 'Z') data[i] = static_cast<char>(c ^ 0x20);
    }
}

void uppercase_ascii(char* data, size_t size) {
    const __m128i vlower = _mm_set1_epi8('a' - 1);
    const __m128i vlower_end = _mm_set1_epi8('z' + 1);
    const __m128i vbit5 = _mm_set1_epi8(0x20);
    size_t i = 0;
    for (; i + 16 <= size; i += 16) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        __m128i not_above = _mm_cmplt_epi8(chunk, vlower_end);
        __m128i not_below = _mm_cmpgt_epi8(chunk, vlower);
        __m128i is_lower = _mm_and_si128(not_above, not_below);
        __m128i uppered = _mm_xor_si128(chunk, _mm_and_si128(is_lower, vbit5));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(data + i), uppered);
    }
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
        __m128i eq = _mm_cmpeq_epi8(chunk, vbyte);
        int mask = _mm_movemask_epi8(eq);
        if (mask != 0) {
            return data + i + ctz32(static_cast<unsigned int>(mask));
        }
    }
    for (; i < size; ++i)
        if (data[i] == byte) return data + i;
    return data + size;
}

} // namespace simdtext::detail::sse2

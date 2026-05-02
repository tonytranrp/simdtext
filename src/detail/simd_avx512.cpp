#if defined(__AVX512BW__) && defined(__AVX512F__)

#include <immintrin.h>
#include <cstddef>

namespace simdtext::detail::avx512 {

namespace {
inline int ctz64(unsigned long long x) {
#if defined(_MSC_VER) && defined(_WIN64)
    unsigned long idx;
    _BitScanForward64(&idx, x);
    return static_cast<int>(idx);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_ctzll(x);
#else
    if (x == 0) return 64;
    int n = 0;
    while ((x & 1) == 0) { ++n; x >>= 1; }
    return n;
#endif
}
} // anonymous namespace

size_t count_byte(const char* data, size_t size, char byte) {
    const __m512i vbyte = _mm512_set1_epi8(byte);
    size_t count = 0;
    size_t i = 0;
    for (; i + 256 <= size; i += 256) {
        __m512i c0 = _mm512_loadu_si512(data + i);
        __m512i c1 = _mm512_loadu_si512(data + i + 64);
        __m512i c2 = _mm512_loadu_si512(data + i + 128);
        __m512i c3 = _mm512_loadu_si512(data + i + 192);
        __mmask64 m0 = _mm512_cmpeq_epi8_mask(c0, vbyte);
        __mmask64 m1 = _mm512_cmpeq_epi8_mask(c1, vbyte);
        __mmask64 m2 = _mm512_cmpeq_epi8_mask(c2, vbyte);
        __mmask64 m3 = _mm512_cmpeq_epi8_mask(c3, vbyte);
        count += static_cast<size_t>(_popcnt64(m0) + _popcnt64(m1) + _popcnt64(m2) + _popcnt64(m3));
    }
    for (; i + 64 <= size; i += 64) {
        __m512i chunk = _mm512_loadu_si512(data + i);
        __mmask64 mask = _mm512_cmpeq_epi8_mask(chunk, vbyte);
        count += static_cast<size_t>(_popcnt64(mask));
    }
    if (i < size) {
        size_t remaining = size - i;
        __mmask64 tail_mask = (1ULL << remaining) - 1;
        __m512i chunk = _mm512_maskz_loadu_epi8(tail_mask, data + i);
        __mmask64 eq_mask = _mm512_mask_cmpeq_epi8_mask(tail_mask, chunk, vbyte);
        count += static_cast<size_t>(_popcnt64(eq_mask));
    }
    return count;
}

bool is_ascii(const char* data, size_t size) {
    size_t i = 0;
    for (; i + 256 <= size; i += 256) {
        __m512i c0 = _mm512_loadu_si512(data + i);
        __m512i c1 = _mm512_loadu_si512(data + i + 64);
        __m512i c2 = _mm512_loadu_si512(data + i + 128);
        __m512i c3 = _mm512_loadu_si512(data + i + 192);
        __m512i ored = _mm512_or_si512(_mm512_or_si512(c0, c1), _mm512_or_si512(c2, c3));
        if (_mm512_movepi8_mask(ored) != 0)
            return false;
    }
    for (; i + 64 <= size; i += 64) {
        __m512i chunk = _mm512_loadu_si512(data + i);
        if (_mm512_movepi8_mask(chunk) != 0)
            return false;
    }
    if (i < size) {
        size_t remaining = size - i;
        __mmask64 tail_mask = (1ULL << remaining) - 1;
        __m512i chunk = _mm512_maskz_loadu_epi8(tail_mask, data + i);
        __mmask64 high_bits = _mm512_movepi8_mask(chunk);
        if ((high_bits & tail_mask) != 0)
            return false;
    }
    return true;
}

void lowercase_ascii(char* data, size_t size) {
    const __m512i vA = _mm512_set1_epi8('A' - 1);
    const __m512i vZ1 = _mm512_set1_epi8('Z' + 1);
    const __m512i vbit5 = _mm512_set1_epi8(0x20);
    size_t i = 0;
    for (; i + 64 <= size; i += 64) {
        __m512i chunk = _mm512_loadu_si512(data + i);
        __mmask64 is_upper = _mm512_cmpgt_epi8_mask(chunk, vA) & _mm512_cmpgt_epi8_mask(vZ1, chunk);
        __m512i xored = _mm512_xor_si512(chunk, vbit5);
        __m512i lowered = _mm512_mask_blend_epi8(is_upper, chunk, xored);
        _mm512_storeu_si512(data + i, lowered);
    }
    if (i < size) {
        size_t remaining = size - i;
        __mmask64 tail_mask = (1ULL << remaining) - 1;
        __m512i chunk = _mm512_maskz_loadu_epi8(tail_mask, data + i);
        __mmask64 is_upper = _mm512_mask_cmpgt_epi8_mask(tail_mask, chunk, vA) &
                             _mm512_mask_cmpgt_epi8_mask(tail_mask, vZ1, chunk);
        __m512i xored = _mm512_xor_si512(chunk, vbit5);
        __m512i lowered = _mm512_mask_blend_epi8(is_upper, chunk, xored);
        _mm512_mask_storeu_epi8(data + i, tail_mask, lowered);
    }
}

void uppercase_ascii(char* data, size_t size) {
    const __m512i va = _mm512_set1_epi8('a' - 1);
    const __m512i vz1 = _mm512_set1_epi8('z' + 1);
    const __m512i vbit5 = _mm512_set1_epi8(0x20);
    size_t i = 0;
    for (; i + 64 <= size; i += 64) {
        __m512i chunk = _mm512_loadu_si512(data + i);
        __mmask64 is_lower = _mm512_cmpgt_epi8_mask(chunk, va) & _mm512_cmpgt_epi8_mask(vz1, chunk);
        __m512i xored = _mm512_xor_si512(chunk, vbit5);
        __m512i uppered = _mm512_mask_blend_epi8(is_lower, chunk, xored);
        _mm512_storeu_si512(data + i, uppered);
    }
    if (i < size) {
        size_t remaining = size - i;
        __mmask64 tail_mask = (1ULL << remaining) - 1;
        __m512i chunk = _mm512_maskz_loadu_epi8(tail_mask, data + i);
        __mmask64 is_lower = _mm512_mask_cmpgt_epi8_mask(tail_mask, chunk, va) &
                             _mm512_mask_cmpgt_epi8_mask(tail_mask, vz1, chunk);
        __m512i xored = _mm512_xor_si512(chunk, vbit5);
        __m512i uppered = _mm512_mask_blend_epi8(is_lower, chunk, xored);
        _mm512_mask_storeu_epi8(data + i, tail_mask, uppered);
    }
}

const char* find_byte(const char* data, size_t size, char byte) {
    const __m512i vbyte = _mm512_set1_epi8(byte);
    size_t i = 0;
    for (; i + 64 <= size; i += 64) {
        __m512i chunk = _mm512_loadu_si512(data + i);
        __mmask64 mask = _mm512_cmpeq_epi8_mask(chunk, vbyte);
        if (mask != 0) {
            return data + i + ctz64(mask);
        }
    }
    if (i < size) {
        size_t remaining = size - i;
        __mmask64 tail_mask = (1ULL << remaining) - 1;
        __m512i chunk = _mm512_maskz_loadu_epi8(tail_mask, data + i);
        __mmask64 eq_mask = _mm512_mask_cmpeq_epi8_mask(tail_mask, chunk, vbyte);
        if (eq_mask != 0) {
            return data + i + ctz64(eq_mask);
        }
    }
    return data + size;
}

} // namespace simdtext::detail::avx512

#endif // __AVX512BW__

#include <immintrin.h>  // AVX2
#include <emmintrin.h>  // SSE2 (for tail)
#include <cstddef>

namespace simdtext::detail::avx2 {

size_t count_byte(const char* data, size_t size, char byte) {
    const __m256i vbyte = _mm256_set1_epi8(byte);
    size_t count = 0;
    size_t i = 0;
    for (; i + 32 <= size; i += 32) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        __m256i eq = _mm256_cmpeq_epi8(chunk, vbyte);
        count += __builtin_popcount(_mm256_movemask_epi8(eq));
    }
    // Tail: use SSE2 for remaining >= 16 bytes
    if (i + 16 <= size) {
        const __m128i vbyte16 = _mm_set1_epi8(byte);
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        __m128i eq = _mm_cmpeq_epi8(chunk, vbyte16);
        count += __builtin_popcount(_mm_movemask_epi8(eq));
        i += 16;
    }
    for (; i < size; ++i)
        if (data[i] == byte) ++count;
    return count;
}

bool is_ascii(const char* data, size_t size) {
    const __m256i vhigh = _mm256_set1_epi8(static_cast<char>(0x80));
    size_t i = 0;
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
    const __m256i vupper = _mm256_set1_epi8('A');
    const __m256i vupper_end = _mm256_set1_epi8('Z');
    const __m256i vdelta = _mm256_set1_epi8(32);
    const __m256i all_ones = _mm256_set1_epi8(static_cast<char>(0xFF));
    size_t i = 0;
    for (; i + 32 <= size; i += 32) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        __m256i is_upper = _mm256_andnot_si256(
            _mm256_or_si256(_mm256_cmpgt_epi8(chunk, vupper_end), _mm256_cmpgt_epi8(vupper, chunk)),
            all_ones);
        __m256i lowered = _mm256_xor_si256(chunk, _mm256_and_si256(vdelta, is_upper));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(data + i), lowered);
    }
    // Tail with SSE2
    if (i + 16 <= size) {
        const __m128i vupper16 = _mm_set1_epi8('A');
        const __m128i vupper_end16 = _mm_set1_epi8('Z');
        const __m128i vdelta16 = _mm_set1_epi8(32);
        const __m128i all_ones16 = _mm_set1_epi8(static_cast<char>(0xFF));
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        __m128i is_upper = _mm_andnot_si128(
            _mm_or_si128(_mm_cmpgt_epi8(chunk, vupper_end16), _mm_cmplt_epi8(chunk, vupper16)),
            all_ones16);
        __m128i lowered = _mm_add_epi8(chunk, _mm_and_si128(vdelta16, is_upper));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(data + i), lowered);
        i += 16;
    }
    for (; i < size; ++i) {
        auto c = static_cast<unsigned char>(data[i]);
        if (c >= 'A' && c <= 'Z') data[i] = static_cast<char>(c + 32);
    }
}

void uppercase_ascii(char* data, size_t size) {
    const __m256i vlower = _mm256_set1_epi8('a');
    const __m256i vlower_end = _mm256_set1_epi8('z');
    const __m256i vdelta = _mm256_set1_epi8(static_cast<char>(-32));
    const __m256i all_ones = _mm256_set1_epi8(static_cast<char>(0xFF));
    size_t i = 0;
    for (; i + 32 <= size; i += 32) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        __m256i is_lower = _mm256_andnot_si256(
            _mm256_or_si256(_mm256_cmpgt_epi8(chunk, vlower_end), _mm256_cmpgt_epi8(vlower, chunk)),
            all_ones);
        __m256i uppered = _mm256_xor_si256(chunk, _mm256_and_si256(vdelta, is_lower));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(data + i), uppered);
    }
    // Tail with SSE2
    if (i + 16 <= size) {
        const __m128i vlower16 = _mm_set1_epi8('a');
        const __m128i vlower_end16 = _mm_set1_epi8('z');
        const __m128i vdelta16 = _mm_set1_epi8(static_cast<char>(-32));
        const __m128i all_ones16 = _mm_set1_epi8(static_cast<char>(0xFF));
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        __m128i is_lower = _mm_andnot_si128(
            _mm_or_si128(_mm_cmpgt_epi8(chunk, vlower_end16), _mm_cmplt_epi8(chunk, vlower16)),
            all_ones16);
        __m128i uppered = _mm_add_epi8(chunk, _mm_and_si128(vdelta16, is_lower));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(data + i), uppered);
        i += 16;
    }
    for (; i < size; ++i) {
        auto c = static_cast<unsigned char>(data[i]);
        if (c >= 'a' && c <= 'z') data[i] = static_cast<char>(c - 32);
    }
}

const char* find_byte(const char* data, size_t size, char byte) {
    const __m256i vbyte = _mm256_set1_epi8(byte);
    size_t i = 0;
    for (; i + 32 <= size; i += 32) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        __m256i eq = _mm256_cmpeq_epi8(chunk, vbyte);
        int mask = _mm256_movemask_epi8(eq);
        if (mask != 0) {
            return data + i + __builtin_ctz(mask);
        }
    }
    // Tail with SSE2
    if (i + 16 <= size) {
        const __m128i vbyte16 = _mm_set1_epi8(byte);
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        __m128i eq = _mm_cmpeq_epi8(chunk, vbyte16);
        int mask = _mm_movemask_epi8(eq);
        if (mask != 0) {
            return data + i + __builtin_ctz(mask);
        }
        i += 16;
    }
    for (; i < size; ++i)
        if (data[i] == byte) return data + i;
    return data + size;
}

} // namespace simdtext::detail::avx2

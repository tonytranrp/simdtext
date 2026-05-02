#include <emmintrin.h>  // SSE2
#include <cstddef>

namespace simdtext::detail::sse2 {

size_t count_byte(const char* data, size_t size, char byte) {
    const __m128i vbyte = _mm_set1_epi8(byte);
    size_t count = 0;
    size_t i = 0;
    for (; i + 16 <= size; i += 16) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        __m128i eq = _mm_cmpeq_epi8(chunk, vbyte);
        count += __builtin_popcount(_mm_movemask_epi8(eq));
    }
    for (; i < size; ++i)
        if (data[i] == byte) ++count;
    return count;
}

bool is_ascii(const char* data, size_t size) {
    const __m128i vhigh = _mm_set1_epi8(static_cast<char>(0x80));
    size_t i = 0;
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
    const __m128i vupper = _mm_set1_epi8('A');
    const __m128i vupper_end = _mm_set1_epi8('Z');
    const __m128i vdelta = _mm_set1_epi8(32);
    size_t i = 0;
    for (; i + 16 <= size; i += 16) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        __m128i ge = _mm_cmpgt_epi8(chunk, vupper_end);
        __m128i lt = _mm_cmplt_epi8(chunk, vupper);
        __m128i is_upper = _mm_andnot_si128(_mm_or_si128(ge, lt), _mm_set1_epi8(static_cast<char>(0xFF)));
        __m128i lowered = _mm_add_epi8(chunk, _mm_and_si128(vdelta, is_upper));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(data + i), lowered);
    }
    for (; i < size; ++i) {
        auto c = static_cast<unsigned char>(data[i]);
        if (c >= 'A' && c <= 'Z') data[i] = static_cast<char>(c + 32);
    }
}

void uppercase_ascii(char* data, size_t size) {
    const __m128i vlower = _mm_set1_epi8('a');
    const __m128i vlower_end = _mm_set1_epi8('z');
    const __m128i vdelta = _mm_set1_epi8(static_cast<char>(-32));
    size_t i = 0;
    for (; i + 16 <= size; i += 16) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        __m128i ge = _mm_cmpgt_epi8(chunk, vlower_end);
        __m128i lt = _mm_cmplt_epi8(chunk, vlower);
        __m128i is_lower = _mm_andnot_si128(_mm_or_si128(ge, lt), _mm_set1_epi8(static_cast<char>(0xFF)));
        __m128i uppered = _mm_add_epi8(chunk, _mm_and_si128(vdelta, is_lower));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(data + i), uppered);
    }
    for (; i < size; ++i) {
        auto c = static_cast<unsigned char>(data[i]);
        if (c >= 'a' && c <= 'z') data[i] = static_cast<char>(c - 32);
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
            return data + i + __builtin_ctz(mask);
        }
    }
    for (; i < size; ++i)
        if (data[i] == byte) return data + i;
    return data + size;
}

} // namespace simdtext::detail::sse2

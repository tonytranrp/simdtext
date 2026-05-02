#include "simdtext/simdtext.hpp"
#include "simdtext/detail/cpu_detect.hpp"
#include <tmmintrin.h>  // SSSE3
#include <cstddef>
#include <cstring>

namespace simdtext::detail {

namespace {

alignas(16) static const char base64_enc_table[64] = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z',
    '0','1','2','3','4','5','6','7','8','9','+','/'
};

alignas(16) static const uint8_t base64_dec_table[256] = {
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

} // anonymous namespace

size_t base64_encode_ssse3(const uint8_t* src, size_t src_size, char* dst) {
    const __m128i vbase64 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(base64_enc_table));
    size_t i = 0, j = 0;

    // Process 12 bytes at a time → 16 base64 chars
    // Each group of 3 input bytes produces 4 output chars
    // We process 4 groups per iteration for better throughput
    for (; i + 12 <= src_size; i += 12) {
        // For each group of 3 bytes, extract 4 6-bit indices and use pshufb lookup
        // Group 0: bytes 0,1,2
        // Group 1: bytes 3,4,5
        // Group 2: bytes 6,7,8
        // Group 3: bytes 9,10,11
        for (int g = 0; g < 4; ++g) {
            size_t base = i + g * 3;
            uint32_t n = (static_cast<uint32_t>(src[base]) << 16) |
                         (static_cast<uint32_t>(src[base + 1]) << 8) |
                         static_cast<uint32_t>(src[base + 2]);

            // Pack 4 6-bit indices into a 16-byte vector for pshufb lookup
            __m128i vidx = _mm_setr_epi8(
                static_cast<char>((n >> 18) & 0x3F),
                static_cast<char>((n >> 12) & 0x3F),
                static_cast<char>((n >> 6) & 0x3F),
                static_cast<char>(n & 0x3F),
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
            );

            __m128i vout = _mm_shuffle_epi8(vbase64, vidx);
            // Extract first 4 bytes
            alignas(16) char out[16];
            _mm_store_si128(reinterpret_cast<__m128i*>(out), vout);
            dst[j++] = out[0];
            dst[j++] = out[1];
            dst[j++] = out[2];
            dst[j++] = out[3];
        }
    }

    // Scalar for remaining 3-byte groups
    for (; i + 3 <= src_size; i += 3) {
        const auto n = (static_cast<uint32_t>(src[i]) << 16) |
                       (static_cast<uint32_t>(src[i+1]) << 8) |
                       static_cast<uint32_t>(src[i+2]);
        dst[j++] = base64_enc_table[(n >> 18) & 0x3F];
        dst[j++] = base64_enc_table[(n >> 12) & 0x3F];
        dst[j++] = base64_enc_table[(n >> 6) & 0x3F];
        dst[j++] = base64_enc_table[n & 0x3F];
    }

    if (i < src_size) {
        auto n = static_cast<uint32_t>(src[i]) << 16;
        if (i + 1 < src_size) n |= static_cast<uint32_t>(src[i+1]) << 8;
        dst[j++] = base64_enc_table[(n >> 18) & 0x3F];
        dst[j++] = base64_enc_table[(n >> 12) & 0x3F];
        dst[j++] = (i + 1 < src_size) ? base64_enc_table[(n >> 6) & 0x3F] : '=';
        dst[j++] = '=';
    }

    return j;
}

DecodeResult base64_decode_ssse3(const uint8_t* src, size_t src_size, uint8_t* dst, size_t dst_capacity) noexcept {
    DecodeResult result{0, 0, ErrorCode::Ok};

    if (src_size % 4 != 0) {
        result.error = ErrorCode::InvalidLength;
        return result;
    }

    size_t padding = 0;
    if (src_size >= 1 && src[src_size - 1] == '=') ++padding;
    if (src_size >= 2 && src[src_size - 2] == '=') ++padding;
    const size_t expected = (src_size / 4) * 3 - padding;

    if (dst_capacity < expected) {
        result.error = ErrorCode::OutputTooSmall;
        return result;
    }

    size_t j = 0;
    const size_t full_chunks = src_size / 4;

    // Process 16 base64 chars (4 groups of 4) at a time using SSSE3 pshufb lookup
    const __m128i vdec_lo = _mm_loadu_si128(reinterpret_cast<const __m128i*>(base64_dec_table));
    const __m128i vdec_hi = _mm_loadu_si128(reinterpret_cast<const __m128i*>(base64_dec_table + 16));

    for (size_t chunk = 0; chunk + 4 <= full_chunks; chunk += 4) {
        const size_t i = chunk * 4;
        __m128i in = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));

        // Lookup 6-bit values: use pshufb with low and high nibble
        __m128i lo_nib = _mm_and_si128(in, _mm_set1_epi8(0x0F));
        __m128i hi_nib = _mm_and_si128(_mm_srli_epi16(in, 4), _mm_set1_epi8(0x0F));

        __m128i val_lo = _mm_shuffle_epi8(vdec_lo, lo_nib);
        __m128i val_hi = _mm_shuffle_epi8(vdec_hi, hi_nib);

        // For byte values 0-15: use val_lo; for 16+: use val_hi
        __m128i is_ge_16 = _mm_cmpgt_epi8(in, _mm_set1_epi8(15));
        __m128i values = _mm_or_si128(
            _mm_and_si128(val_hi, is_ge_16),
            _mm_andnot_si128(is_ge_16, val_lo)
        );

        // Check for invalid: pshufb sets high bit when index has bit 7 set
        // Also check if value > 63 (invalid char mapped to 64)
        __m128i invalid = _mm_cmpgt_epi8(values, _mm_set1_epi8(63));
        if (_mm_movemask_epi8(invalid) != 0) {
            // Fall back to scalar for this chunk
            for (size_t k = 0; k < 4; ++k) {
                const size_t ci = chunk + k;
                const size_t bi = ci * 4;
                const uint8_t a = base64_dec_table[src[bi]];
                const uint8_t b = base64_dec_table[src[bi+1]];
                const uint8_t c = base64_dec_table[src[bi+2]];
                const uint8_t d = base64_dec_table[src[bi+3]];
                if (a == 64 || b == 64) {
                    result.error = ErrorCode::InvalidChar;
                    result.error_offset = (a == 64) ? bi : bi + 1;
                    return result;
                }
                const uint32_t n = (static_cast<uint32_t>(a) << 18) |
                                   (static_cast<uint32_t>(b) << 12) |
                                   (static_cast<uint32_t>(c) << 6) |
                                   static_cast<uint32_t>(d);
                dst[j++] = static_cast<uint8_t>((n >> 16) & 0xFF);
                if (src[bi+2] != '=') dst[j++] = static_cast<uint8_t>((n >> 8) & 0xFF);
                if (src[bi+3] != '=') dst[j++] = static_cast<uint8_t>(n & 0xFF);
            }
            continue;
        }

        // Pack 6-bit values into bytes
        alignas(16) uint8_t vals[16];
        _mm_store_si128(reinterpret_cast<__m128i*>(vals), values);
        for (int g = 0; g < 4; ++g) {
            uint32_t a = vals[g*4+0], b = vals[g*4+1], c = vals[g*4+2], d = vals[g*4+3];
            uint32_t n = (a << 18) | (b << 12) | (c << 6) | d;
            dst[j++] = static_cast<uint8_t>((n >> 16) & 0xFF);
            dst[j++] = static_cast<uint8_t>((n >> 8) & 0xFF);
            dst[j++] = static_cast<uint8_t>(n & 0xFF);
        }
    }

    // Scalar remainder
    for (size_t chunk = (full_chunks / 4) * 4; chunk < full_chunks; ++chunk) {
        const size_t i = chunk * 4;
        const uint8_t a = base64_dec_table[src[i]];
        const uint8_t b = base64_dec_table[src[i+1]];
        const uint8_t c = base64_dec_table[src[i+2]];
        const uint8_t d = base64_dec_table[src[i+3]];
        if (a == 64 || b == 64) {
            result.error = ErrorCode::InvalidChar;
            result.error_offset = (a == 64) ? i : i + 1;
            return result;
        }
        const uint32_t n = (static_cast<uint32_t>(a) << 18) |
                           (static_cast<uint32_t>(b) << 12) |
                           (static_cast<uint32_t>(c) << 6) |
                           static_cast<uint32_t>(d);
        dst[j++] = static_cast<uint8_t>((n >> 16) & 0xFF);
        if (src[i+2] != '=') dst[j++] = static_cast<uint8_t>((n >> 8) & 0xFF);
        if (src[i+3] != '=') dst[j++] = static_cast<uint8_t>(n & 0xFF);
    }

    result.bytes_written = j;
    return result;
}

} // namespace simdtext::detail

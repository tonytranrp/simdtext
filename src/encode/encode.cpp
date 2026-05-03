#include "simdtext/simdtext.hpp"
#include <array>
#include <cctype>
#include <cstring>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#endif

#if defined(__GNUC__) && (defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86))
#pragma GCC target ("avx2")
#endif

// Hex decode lookup table — replaces branching hex_val for decode hot paths
static constexpr std::array<int8_t, 256> hex_decode_table = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
     0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1,
    -1,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,
    25,26,27,28,29,30,31,32,33,34,35,-1,-1,-1,-1,-1,
    -1,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,
    25,26,27,28,29,30,31,32,33,34,35,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
};

namespace simdtext {

// ── Hex Encode/Decode ──────────────────────────────────────

static constexpr std::array<char, 16> hex_chars = {
    '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'
};

// Forward declarations — implementations in src/highway/simd_hwy.cpp
#ifdef SIMDTEXT_HAVE_HWY
size_t hex_encode_simd(const uint8_t* src, size_t src_size, char* dst);
size_t base64_encode_to_hwym(const uint8_t* src, size_t src_size, char* dst);
#endif

size_t hex_encode_to(std::span<const std::byte> input, std::span<char> output) {
    const size_t required = input.size() * 2;
    if (output.size() < required) return 0;

    const auto* SIMDTEXT_RESTRICT src = reinterpret_cast<const uint8_t*>(input.data());
    auto* SIMDTEXT_RESTRICT dst = output.data();

#ifdef SIMDTEXT_HAVE_HWY
    if (input.size() >= 16) {
        return hex_encode_simd(src, input.size(), dst);
    }
#endif

    size_t i = 0;

    // Process 4 bytes at a time for better ILP
    for (; i + 3 < input.size(); i += 4) {
        const uint8_t b0 = src[i+0], b1 = src[i+1], b2 = src[i+2], b3 = src[i+3];
        const size_t j = i * 2;
        dst[j+0] = hex_chars[b0 >> 4];     dst[j+1] = hex_chars[b0 & 0xF];
        dst[j+2] = hex_chars[b1 >> 4];     dst[j+3] = hex_chars[b1 & 0xF];
        dst[j+4] = hex_chars[b2 >> 4];     dst[j+5] = hex_chars[b2 & 0xF];
        dst[j+6] = hex_chars[b3 >> 4];     dst[j+7] = hex_chars[b3 & 0xF];
    }
    for (; i < input.size(); ++i) {
        dst[i * 2]     = hex_chars[src[i] >> 4];
        dst[i * 2 + 1] = hex_chars[src[i] & 0x0F];
    }
    return required;
}

std::string hex_encode(std::span<const std::byte> input) {
    std::string result(input.size() * 2, '\0');
    (void)hex_encode_to(input, std::span<char>(result));
    return result;
}

// hex_val is declared in simdtext.hpp and defined in url.cpp

// ── SSSE3 hex_decode ──────────────────────────────────────
// Process 32 hex chars → 16 output bytes per iteration.
// 1. Load 16 chars (hi nibbles), load 16 chars (lo nibbles)
// 2. Normalize to lowercase (OR 0x20), subtract '0', adjust for alpha
// 3. Combine: (hi << 4) | lo
// 4. Validate: any nibble > 15 is invalid

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

// AVX2 hex_decode using maddubs + packus_epi16 (fast-hex / zbjornson)
static DecodeResult hex_decode_avx2(const uint8_t* src, size_t src_size, uint8_t* dst) noexcept {
    DecodeResult result{0, 0, ErrorCode::Ok};
    const size_t byte_count = src_size / 2;
    const __m256i A_MASK = _mm256_setr_epi8(0,-1,2,-1,4,-1,6,-1,8,-1,10,-1,12,-1,14,-1,0,-1,2,-1,4,-1,6,-1,8,-1,10,-1,12,-1,14,-1);
    const __m256i B_MASK = _mm256_setr_epi8(1,-1,3,-1,5,-1,7,-1,9,-1,11,-1,13,-1,15,-1,1,-1,3,-1,5,-1,7,-1,9,-1,11,-1,13,-1,15,-1);
    const __m256i v9 = _mm256_set1_epi16(9);
    const __m256i v0F16 = _mm256_set1_epi16(0x0F);
    const __m256i vF0 = _mm256_set1_epi8(static_cast<char>(0xF0));
    const __m256i v0F8 = _mm256_set1_epi8(0x0F);
    const __m256i vInvMask = _mm256_set1_epi16(static_cast<short>(0xFFF0));

    size_t i = 0;
    for (; i + 32 <= byte_count; i += 32) {
        __m256i av1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i * 2));
        __m256i av2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i * 2 + 32));
        __m256i hi1 = _mm256_shuffle_epi8(av1, A_MASK);
        __m256i lo1 = _mm256_shuffle_epi8(av1, B_MASK);
        __m256i hi2 = _mm256_shuffle_epi8(av2, A_MASK);
        __m256i lo2 = _mm256_shuffle_epi8(av2, B_MASK);
        __m256i hi1v = _mm256_add_epi16(_mm256_maddubs_epi16(_mm256_srai_epi16(hi1, 6), v9), _mm256_and_si256(hi1, v0F16));
        __m256i lo1v = _mm256_add_epi16(_mm256_maddubs_epi16(_mm256_srai_epi16(lo1, 6), v9), _mm256_and_si256(lo1, v0F16));
        __m256i hi2v = _mm256_add_epi16(_mm256_maddubs_epi16(_mm256_srai_epi16(hi2, 6), v9), _mm256_and_si256(hi2, v0F16));
        __m256i lo2v = _mm256_add_epi16(_mm256_maddubs_epi16(_mm256_srai_epi16(lo2, 6), v9), _mm256_and_si256(lo2, v0F16));
        __m256i all = _mm256_or_si256(_mm256_or_si256(hi1v, lo1v), _mm256_or_si256(hi2v, lo2v));
        if (!_mm256_testz_si256(_mm256_and_si256(all, vInvMask), _mm256_and_si256(all, vInvMask))) break;
        __m256i c1 = _mm256_or_si256(_mm256_and_si256(_mm256_slli_epi16(hi1v, 4), vF0), _mm256_and_si256(lo1v, v0F8));
        __m256i c2 = _mm256_or_si256(_mm256_and_si256(_mm256_slli_epi16(hi2v, 4), vF0), _mm256_and_si256(lo2v, v0F8));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), _mm256_permute4x64_epi64(_mm256_packus_epi16(c1, c2), 0xD8));
    }
    for (; i + 16 <= byte_count; i += 16) {
        __m256i av = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i * 2));
        __m256i hi = _mm256_shuffle_epi8(av, A_MASK);
        __m256i lo = _mm256_shuffle_epi8(av, B_MASK);
        __m256i hiv = _mm256_add_epi16(_mm256_maddubs_epi16(_mm256_srai_epi16(hi, 6), v9), _mm256_and_si256(hi, v0F16));
        __m256i lov = _mm256_add_epi16(_mm256_maddubs_epi16(_mm256_srai_epi16(lo, 6), v9), _mm256_and_si256(lo, v0F16));
        __m256i all = _mm256_or_si256(hiv, lov);
        if (!_mm256_testz_si256(_mm256_and_si256(all, vInvMask), _mm256_and_si256(all, vInvMask))) break;
        __m256i combined = _mm256_or_si256(_mm256_and_si256(_mm256_slli_epi16(hiv, 4), vF0), _mm256_and_si256(lov, v0F8));
        __m128i lo_lane = _mm256_castsi256_si128(combined);
        __m128i hi_lane = _mm256_extracti128_si256(combined, 1);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i), _mm_packus_epi16(lo_lane, hi_lane));
    }
    for (; i < byte_count; ++i) {
        const int8_t h = hex_decode_table[src[i * 2]];
        const int8_t l = hex_decode_table[src[i * 2 + 1]];
        if (h < 0 || l < 0 || h > 15 || l > 15) {
            result.error = ErrorCode::InvalidChar;
            result.error_offset = i * 2 + (h < 0 || h > 15 ? 0u : 1u);
            return result;
        }
        dst[i] = static_cast<uint8_t>((static_cast<uint8_t>(h) << 4) | static_cast<uint8_t>(l));
    }
    result.bytes_written = byte_count;
    return result;
}

#endif // x86

DecodeResult hex_decode_to(std::string_view input, std::span<std::byte> output) noexcept {
    DecodeResult result{0, 0, ErrorCode::Ok};

    if (input.size() % 2 != 0) {
        result.error = ErrorCode::InvalidLength;
        return result;
    }

    const size_t byte_count = input.size() / 2;
    if (output.size() < byte_count) {
        result.error = ErrorCode::OutputTooSmall;
        return result;
    }

    auto* SIMDTEXT_RESTRICT dst = reinterpret_cast<uint8_t*>(output.data());
    const auto* SIMDTEXT_RESTRICT src = reinterpret_cast<const uint8_t*>(input.data());

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    // AVX2 fast path using maddubs + packus_epi16
    if (__builtin_cpu_supports("avx2") && byte_count >= 16) {
        return hex_decode_avx2(src, input.size(), dst);
    }
#endif

    for (size_t i = 0; i < byte_count; ++i) {
        const int8_t hi = hex_decode_table[src[i * 2]];
        const int8_t lo = hex_decode_table[src[i * 2 + 1]];
        if (hi < 0 || lo < 0 || hi > 15 || lo > 15) {
            result.error = ErrorCode::InvalidChar;
            result.error_offset = i * 2 + (hi < 0 || hi > 15 ? 0u : 1u);
            return result;
        }
        dst[i] = static_cast<uint8_t>((static_cast<uint8_t>(hi) << 4) | static_cast<uint8_t>(lo));
    }

    result.bytes_written = byte_count;
    return result;
}

DecodeResult hex_decode_to(std::string_view input, std::span<char> output) noexcept {
    return hex_decode_to(input, std::span<std::byte>(
        reinterpret_cast<std::byte*>(output.data()), output.size()));
}

std::vector<std::byte> hex_decode(std::string_view input) {
    std::vector<std::byte> result(input.size() / 2);
    (void)hex_decode_to(input, std::span<std::byte>(result));
    return result;
}

// ── Base64 Encode/Decode ───────────────────────────────────

static constexpr std::array<char, 64> base64_chars = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z',
    '0','1','2','3','4','5','6','7','8','9','+','/'
};

static constexpr std::array<uint8_t, 256> base64_table = {
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

#ifdef SIMDTEXT_HAVE_HWY
// Forward declaration — implementation in src/highway/simd_hwy.cpp
size_t base64_encode_to_hwym(const uint8_t* src, size_t src_size, char* dst);
#endif

#ifdef __AVX2__
// Forward declaration — implementation in src/detail/simd_avx2.cpp
DecodeResult base64_decode_avx2(const uint8_t* src, size_t src_size, uint8_t* dst) noexcept;
#endif

size_t base64_encode_to(std::span<const std::byte> input, std::span<char> output) {
    const size_t required = 4 * ((input.size() + 2) / 3);
    if (output.size() < required) return 0;

    const auto* src = reinterpret_cast<const uint8_t*>(input.data());
    auto* dst = output.data();

#ifdef SIMDTEXT_HAVE_HWY
    if (input.size() >= 12) {
        return base64_encode_to_hwym(src, input.size(), dst);
    }
#endif

    size_t i = 0, j = 0;
    for (; i + 2 < input.size(); i += 3) {
        const auto n = (static_cast<uint32_t>(src[i]) << 16) |
                       (static_cast<uint32_t>(src[i+1]) << 8) |
                       static_cast<uint32_t>(src[i+2]);
        dst[j++] = base64_chars[(n >> 18) & 0x3F];
        dst[j++] = base64_chars[(n >> 12) & 0x3F];
        dst[j++] = base64_chars[(n >> 6) & 0x3F];
        dst[j++] = base64_chars[n & 0x3F];
    }

    if (i < input.size()) {
        auto n = static_cast<uint32_t>(src[i]) << 16;
        if (i + 1 < input.size()) n |= static_cast<uint32_t>(src[i+1]) << 8;
        dst[j++] = base64_chars[(n >> 18) & 0x3F];
        dst[j++] = base64_chars[(n >> 12) & 0x3F];
        dst[j++] = (i + 1 < input.size()) ? base64_chars[(n >> 6) & 0x3F] : '=';
        dst[j++] = '=';
    }

    return j;
}

std::string base64_encode(std::span<const std::byte> input) {
    const size_t required = 4 * ((input.size() + 2) / 3);
    std::string result(required, '\0');
    const size_t written = base64_encode_to(input, std::span<char>(result));
    result.resize(written);
    return result;
}

DecodeResult base64_decode_to(std::string_view input, std::span<std::byte> output) noexcept {
    DecodeResult result{0, 0, ErrorCode::Ok};

    if (input.size() % 4 != 0) {
        result.error = ErrorCode::InvalidLength;
        return result;
    }

    const size_t max_bytes = (input.size() / 4) * 3;
    size_t padding = 0;
    if (input.size() >= 1 && input.back() == '=') ++padding;
    if (input.size() >= 2 && input[input.size()-2] == '=') ++padding;
    const size_t expected = max_bytes - padding;

    if (output.size() < expected) {
        result.error = ErrorCode::OutputTooSmall;
        return result;
    }

    auto* SIMDTEXT_RESTRICT dst = reinterpret_cast<uint8_t*>(output.data());
    const auto* SIMDTEXT_RESTRICT src = reinterpret_cast<const uint8_t*>(input.data());

#ifdef __AVX2__
    // Use AVX2 for bulk decode, fall back to scalar for tail/padding
    if (input.size() >= 32) {
        // Process all full 32-byte chunks via AVX2, but leave the last
        // chunk (which may have padding) for scalar if there's padding.
        size_t avx2_end = input.size();
        if (padding > 0) avx2_end -= 4;  // Leave last chunk for scalar
        avx2_end = avx2_end & ~size_t(31);  // Round down to 32-byte boundary

        if (avx2_end >= 32) {
            DecodeResult avx2_result = base64_decode_avx2(src, avx2_end, dst);
            if (!avx2_result.ok()) return avx2_result;
            size_t j = avx2_result.bytes_written;

            // Handle remaining bytes (after AVX2 chunk, before potential padding chunk)
            size_t remaining_start = avx2_end;
            for (size_t ii = remaining_start; ii + 4 <= input.size(); ii += 4) {
                const uint8_t a = base64_table[src[ii]];
                const uint8_t b = base64_table[src[ii+1]];
                const uint8_t c = base64_table[src[ii+2]];
                const uint8_t d = base64_table[src[ii+3]];
                if (a == 64 || b == 64) {
                    result.error = ErrorCode::InvalidChar;
                    result.error_offset = (a == 64) ? ii : ii + 1;
                    return result;
                }
                const uint32_t n = (static_cast<uint32_t>(a) << 18) |
                                   (static_cast<uint32_t>(b) << 12) |
                                   (static_cast<uint32_t>(c) << 6) |
                                   static_cast<uint32_t>(d);
                dst[j++] = static_cast<uint8_t>((n >> 16) & 0xFF);
                if (src[ii+2] != '=') dst[j++] = static_cast<uint8_t>((n >> 8) & 0xFF);
                if (src[ii+3] != '=') dst[j++] = static_cast<uint8_t>(n & 0xFF);
            }
            result.bytes_written = j;
            return result;
        }
    }
#endif

    size_t j = 0;
    const size_t chunks = input.size() / 4;

    for (size_t chunk = 0; chunk < chunks; ++chunk) {
        const size_t i = chunk * 4;
        const uint8_t a = base64_table[src[i]];
        const uint8_t b = base64_table[src[i+1]];
        const uint8_t c = base64_table[src[i+2]];
        const uint8_t d = base64_table[src[i+3]];

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
        if (src[i+2] != static_cast<uint8_t>('=')) dst[j++] = static_cast<uint8_t>((n >> 8) & 0xFF);
        if (src[i+3] != static_cast<uint8_t>('=')) dst[j++] = static_cast<uint8_t>(n & 0xFF);
    }

    result.bytes_written = j;
    return result;
}

DecodeResult base64_decode_to(std::string_view input, std::span<char> output) noexcept {
    return base64_decode_to(input, std::span<std::byte>(
        reinterpret_cast<std::byte*>(output.data()), output.size()));
}

std::vector<std::byte> base64_decode(std::string_view input) {
    const size_t max_bytes = (input.size() / 4) * 3;
    size_t padding = 0;
    if (input.size() >= 1 && input.back() == '=') ++padding;
    if (input.size() >= 2 && input[input.size()-2] == '=') ++padding;
    std::vector<std::byte> result(max_bytes - padding);
    (void)base64_decode_to(input, std::span<std::byte>(result));
    return result;
}

} // namespace simdtext

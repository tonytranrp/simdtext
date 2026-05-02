#include "simdtext/simdtext.hpp"
#include <array>
#include <cctype>
#include <cstring>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <tmmintrin.h>  // SSSE3
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

__attribute__((target("ssse3")))
static DecodeResult hex_decode_ssse3(const uint8_t* src, size_t src_size, uint8_t* dst) noexcept {
    DecodeResult result{0, 0, ErrorCode::Ok};
    const size_t byte_count = src_size / 2;

    const __m128i v_or_mask = _mm_set1_epi8(0x20);
    const __m128i v_sub     = _mm_set1_epi8('0');     // 0x30
    const __m128i v_adj     = _mm_set1_epi8(0x27);    // subtract this for alpha chars
    const __m128i v_9       = _mm_set1_epi8(9);

    const __m128i v_lo_mask = _mm_set1_epi8(0x0F);

    size_t i = 0;  // byte index (output)
    // Process 16 output bytes per iteration (32 input chars)
    for (; i + 16 <= byte_count; i += 16) {
        // Load 16 hi-nibble chars and 16 lo-nibble chars
        __m128i hi_raw = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i * 2));
        __m128i lo_raw = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i * 2 + 16));

        // Normalize to lowercase: 'A'-'F' → 'a'-'f', digits unaffected
        __m128i hi_norm = _mm_or_si128(hi_raw, v_or_mask);
        __m128i lo_norm = _mm_or_si128(lo_raw, v_or_mask);

        // Subtract '0'
        __m128i hi_val = _mm_sub_epi8(hi_norm, v_sub);
        __m128i lo_val = _mm_sub_epi8(lo_norm, v_sub);

        // For alpha chars (val > 9 after subtract), subtract 0x27 more
        // val > 9 means the char was 'a'-'f' (after normalization)
        __m128i hi_alpha = _mm_cmpgt_epi8(hi_val, v_9);
        __m128i lo_alpha = _mm_cmpgt_epi8(lo_val, v_9);
        hi_val = _mm_sub_epi8(hi_val, _mm_and_si128(hi_alpha, v_adj));
        lo_val = _mm_sub_epi8(lo_val, _mm_and_si128(lo_alpha, v_adj));

        // Validate: any nibble > 15 is invalid (invalid hex char)
        // After adjustment, valid nibbles are 0-15. Invalid chars can produce
        // values outside 0-15. Check hi_val > 15 || lo_val > 15 using signed cmpgt vs 15.
        // But signed compare treats values > 127 as negative. Use unsigned: cmpgt with 0x0F.
        // Actually, _mm_cmpgt_epi8 is signed. For values 0-15 this is fine.
        // Invalid chars may produce negative values (high bit set), which would pass
        // signed > 15 check (negative < 15). So also check for negative values.
        // Use: invalid = val > 15 || val < 0 (i.e., high bit set OR val > 15)
        // Simplified: val with high bit set OR val > 15 = val > 15 in unsigned.
        // We can check: (val & 0xF0) != 0, i.e., any of the upper 4 bits are set.
        // Equivalent: val > 15 unsigned = val has bits outside low 4.
        // Use: or all hi/lo vals together, AND with 0xF0 each, check if non-zero.
        __m128i hi_err = _mm_andnot_si128(v_lo_mask, hi_val);  // upper 4 bits
        __m128i lo_err = _mm_andnot_si128(v_lo_mask, lo_val);
        __m128i any_err = _mm_or_si128(hi_err, lo_err);
        if (_mm_movemask_epi8(any_err) != 0) {
            // Fall back to scalar for this chunk to get exact error offset
            for (size_t k = 0; k < 16 && (i + k) < byte_count; ++k) {
                const int8_t hv = hex_decode_table[src[(i + k) * 2]];
                const int8_t lv = hex_decode_table[src[(i + k) * 2 + 1]];
                if (hv < 0 || hv > 15) {
                    result.error = ErrorCode::InvalidChar;
                    result.error_offset = (i + k) * 2;
                    return result;
                }
                if (lv < 0 || lv > 15) {
                    result.error = ErrorCode::InvalidChar;
                    result.error_offset = (i + k) * 2 + 1;
                    return result;
                }
            }
        }

        // Combine: (hi << 4) | lo
        // No _mm_slli_epi8 in SSE2/SSSE3; use 16-bit shift + mask to clear leaked bits
        __m128i hi_shifted = _mm_and_si128(_mm_slli_epi16(hi_val, 4), v_lo_mask);
        __m128i combined = _mm_or_si128(hi_shifted, _mm_and_si128(lo_val, v_lo_mask));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i), combined);
    }

    // Scalar tail
    for (; i < byte_count; ++i) {
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
    // SSSE3 fast path: process 16 output bytes per iteration
    if (__builtin_cpu_supports("ssse3") && byte_count >= 16) {
        return hex_decode_ssse3(src, input.size(), dst);
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

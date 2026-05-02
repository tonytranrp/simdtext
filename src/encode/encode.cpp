#include "simdtext/simdtext.hpp"
#include <array>
#include <cctype>
#include <cstring>

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

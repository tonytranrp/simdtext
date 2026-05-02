#include "simdtext/simdtext.hpp"
#include <array>
#include <cstring>

namespace simdtext {

#ifdef SIMDTEXT_HAVE_HWY
// Forward declarations — implementations in src/highway/simd_hwy.cpp
size_t url_encode_to_hwy(const uint8_t* src, size_t src_size, char* dst, size_t dst_size);
std::string url_encode_hwy(const uint8_t* src, size_t src_size);
size_t url_decode_to_hwy(const uint8_t* src, size_t src_size, char* dst, size_t dst_size);
std::string url_decode_hwy(const uint8_t* src, size_t src_size);
#endif

// ── URL-safe lookup table (branchless classify) ────────────

// Bitmask: bit set = URL-safe character (unreserved per RFC 3986)
static constexpr std::array<uint8_t, 256> url_safe_table = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,  // '-'=1, '.'=1
    1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,   // '0'-'9'=1
    0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,   // 'A'-'O'=1
    1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,   // 'P'-'Z'=1, '_'=1
    0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,   // 'a'-'o'=1
    1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,0,   // 'p'-'z'=1, '~'=1
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

// Hex decode lookup table (same as encode.cpp, duplicated for TU locality)
static constexpr std::array<int8_t, 256> hex_decode_table = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
     0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1,
    -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
};

static constexpr std::array<char, 16> url_hex = {
    '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'
};

size_t url_encode_to(std::string_view input, std::span<char> output) noexcept {
#ifdef SIMDTEXT_HAVE_HWY
    if (input.size() >= 32) {
        return url_encode_to_hwy(reinterpret_cast<const uint8_t*>(input.data()),
                                  input.size(), output.data(), output.size());
    }
#endif
    const auto* SIMDTEXT_RESTRICT src = reinterpret_cast<const uint8_t*>(input.data());
    auto* SIMDTEXT_RESTRICT dst = output.data();
    size_t j = 0;
    const size_t out_size = output.size();

    for (size_t i = 0; i < input.size(); ++i) {
        const uint8_t uc = src[i];
        if (url_safe_table[uc]) {
            if (j >= out_size) return 0;
            dst[j++] = static_cast<char>(uc);
        } else {
            if (j + 2 >= out_size) return 0;
            dst[j++] = '%';
            dst[j++] = url_hex[uc >> 4];
            dst[j++] = url_hex[uc & 0x0F];
        }
    }
    return j;
}

std::string url_encode(std::string_view input) {
#ifdef SIMDTEXT_HAVE_HWY
    if (input.size() >= 32) {
        return url_encode_hwy(reinterpret_cast<const uint8_t*>(input.data()), input.size());
    }
#endif
    std::string result;
    result.reserve(input.size() * 3);
    const auto* src = reinterpret_cast<const uint8_t*>(input.data());
    for (size_t i = 0; i < input.size(); ++i) {
        const uint8_t uc = src[i];
        if (url_safe_table[uc]) {
            result += static_cast<char>(uc);
        } else {
            result += '%';
            result += url_hex[uc >> 4];
            result += url_hex[uc & 0x0F];
        }
    }
    return result;
}

size_t url_decode_to(std::string_view input, std::span<char> output) noexcept {
#ifdef SIMDTEXT_HAVE_HWY
    if (input.size() >= 64) {
        return url_decode_to_hwy(reinterpret_cast<const uint8_t*>(input.data()),
                                  input.size(), output.data(), output.size());
    }
#endif
    const auto* SIMDTEXT_RESTRICT src = reinterpret_cast<const uint8_t*>(input.data());
    auto* SIMDTEXT_RESTRICT dst = output.data();
    size_t j = 0;
    const size_t out_size = output.size();

    for (size_t i = 0; i < input.size(); ++i) {
        if (src[i] == '%' && i + 2 < input.size()) {
            const int8_t hi = hex_decode_table[src[i + 1]];
            const int8_t lo = hex_decode_table[src[i + 2]];
            if (hi >= 0 && lo >= 0 && hi <= 15 && lo <= 15) {
                if (j >= out_size) return 0;
                dst[j++] = static_cast<char>((static_cast<uint8_t>(hi) << 4) | static_cast<uint8_t>(lo));
                i += 2;
                continue;
            }
        } else if (src[i] == '+') {
            if (j >= out_size) return 0;
            dst[j++] = ' ';
            continue;
        }
        if (j >= out_size) return 0;
        dst[j++] = static_cast<char>(src[i]);
    }
    return j;
}

std::string url_decode(std::string_view input) {
#ifdef SIMDTEXT_HAVE_HWY
    if (input.size() >= 64) {
        return url_decode_hwy(reinterpret_cast<const uint8_t*>(input.data()), input.size());
    }
#endif
    std::string result;
    result.reserve(input.size());
    const auto* src = reinterpret_cast<const uint8_t*>(input.data());
    for (size_t i = 0; i < input.size(); ++i) {
        if (src[i] == '%' && i + 2 < input.size()) {
            const int8_t hi = hex_decode_table[src[i + 1]];
            const int8_t lo = hex_decode_table[src[i + 2]];
            if (hi >= 0 && lo >= 0 && hi <= 15 && lo <= 15) {
                result += static_cast<char>((static_cast<uint8_t>(hi) << 4) | static_cast<uint8_t>(lo));
                i += 2;
                continue;
            }
        } else if (src[i] == '+') {
            result += ' ';
            continue;
        }
        result += static_cast<char>(src[i]);
    }
    return result;
}

// ── Query String Parsing ───────────────────────────────────

std::unordered_map<std::string, std::string> parse_query(std::string_view query) {
    std::unordered_map<std::string, std::string> params;

    if (!query.empty() && query.front() == '?') {
        query.remove_prefix(1);
    }

    if (query.empty()) return params;

    for (const auto pair : split(query, '&')) {
        const auto eq = pair.find('=');
        if (eq == std::string_view::npos) {
            std::string key(pair);
            params[std::move(key)] = "";
        } else {
            std::string key(pair.substr(0, eq));
            std::string val = url_decode(pair.substr(eq + 1));
            params[std::move(key)] = std::move(val);
        }
    }
    return params;
}

// Internal helper used by encode.cpp and url.cpp
int hex_val(char c) noexcept {
    const int8_t v = hex_decode_table[static_cast<uint8_t>(c)];
    return (v >= 0 && v <= 15) ? static_cast<int>(v) : -1;
}

} // namespace simdtext

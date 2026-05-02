#include "simdtext/simdtext.hpp"
#include <array>
#include <cstring>

namespace simdtext {

// ── URL Encode/Decode ──────────────────────────────────────

constexpr static bool is_url_safe(char c) noexcept {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~';
}

static constexpr std::array<char, 16> url_hex = {
    '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'
};

size_t url_encode_to(std::string_view input, std::span<char> output) {
    size_t j = 0;
    for (size_t i = 0; i < input.size(); ++i) {
        if (is_url_safe(input[i])) {
            if (j >= output.size()) return 0;
            output[j++] = input[i];
        } else {
            if (j + 2 >= output.size()) return 0;
            output[j++] = '%';
            const auto uc = static_cast<unsigned char>(input[i]);
            output[j++] = url_hex[uc >> 4];
            output[j++] = url_hex[uc & 0x0F];
        }
    }
    return j;
}

std::string url_encode(std::string_view input) {
    std::string result;
    result.reserve(input.size() * 3);
    for (const char c : input) {
        if (is_url_safe(c)) {
            result += c;
        } else {
            result += '%';
            const auto uc = static_cast<unsigned char>(c);
            result += url_hex[uc >> 4];
            result += url_hex[uc & 0x0F];
        }
    }
    return result;
}

size_t url_decode_to(std::string_view input, std::span<char> output) {
    size_t j = 0;
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size()) {
            const int hi = hex_val(input[i + 1]);
            const int lo = hex_val(input[i + 2]);
            if (hi >= 0 && lo >= 0) {
                if (j >= output.size()) return 0;
                output[j++] = static_cast<char>((hi << 4) | lo);
                i += 2;
                continue;
            }
        } else if (input[i] == '+') {
            if (j >= output.size()) return 0;
            output[j++] = ' ';
            continue;
        }
        if (j >= output.size()) return 0;
        output[j++] = input[i];
    }
    return j;
}

std::string url_decode(std::string_view input) {
    std::string result;
    result.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size()) {
            const int hi = hex_val(input[i + 1]);
            const int lo = hex_val(input[i + 2]);
            if (hi >= 0 && lo >= 0) {
                result += static_cast<char>((hi << 4) | lo);
                i += 2;
                continue;
            }
        } else if (input[i] == '+') {
            result += ' ';
            continue;
        }
        result += input[i];
    }
    return result;
}

// ── Query String Parsing ───────────────────────────────────

std::unordered_map<std::string, std::string> parse_query(std::string_view query) {
    std::unordered_map<std::string, std::string> params;

    if (!query.empty() && query.front() == '?') {
        query.remove_prefix(1);
    }

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
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

} // namespace simdtext

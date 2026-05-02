#include "simdtext/simdtext.hpp"
#include <cstring>

namespace simdtext {

// ── URL Encode/Decode ──────────────────────────────────────

static bool is_url_safe(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~';
}

static const char url_hex[] = "0123456789ABCDEF";

size_t url_encode_to(std::string_view input, std::span<char> output) {
    size_t j = 0;
    for (size_t i = 0; i < input.size(); ++i) {
        if (is_url_safe(input[i])) {
            if (j >= output.size()) return 0;
            output[j++] = input[i];
        } else {
            if (j + 2 >= output.size()) return 0;
            output[j++] = '%';
            output[j++] = url_hex[static_cast<unsigned char>(input[i]) >> 4];
            output[j++] = url_hex[static_cast<unsigned char>(input[i]) & 0x0F];
        }
    }
    return j;
}

std::string url_encode(std::string_view input) {
    std::string result;
    result.reserve(input.size() * 3);
    for (size_t i = 0; i < input.size(); ++i) {
        if (is_url_safe(input[i])) {
            result += input[i];
        } else {
            result += '%';
            result += url_hex[static_cast<unsigned char>(input[i]) >> 4];
            result += url_hex[static_cast<unsigned char>(input[i]) & 0x0F];
        }
    }
    return result;
}

size_t url_decode_to(std::string_view input, std::span<char> output) {
    size_t j = 0;
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size()) {
            int hi = hex_val(input[i + 1]);
            int lo = hex_val(input[i + 2]);
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
            int hi = hex_val(input[i + 1]);
            int lo = hex_val(input[i + 2]);
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

    // Strip leading '?'
    if (!query.empty() && query.front() == '?') {
        query.remove_prefix(1);
    }

    for (auto pair : split(query, '&')) {
        auto eq = pair.find('=');
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
int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

} // namespace simdtext

/// @file url_handler.cpp
/// @brief Decode URL query strings, parse parameters.
///
/// Usage: ./url_handler <query_string>
///   e.g. ./url_handler "name=Tony&lang=c%2B%2B&city=Honolulu"
///
/// Demonstrates: url_decode(), url_encode(), parse_query()

#include <simdtext/simdtext.hpp>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <query_string>\n", argv[0]);
        std::fprintf(stderr, "  e.g. %s \"name=Tony&lang=c%%2B%%2B\"\n", argv[0]);
        return 1;
    }

    std::string_view query = argv[1];

    // Decode the full query string
    std::string decoded = simdtext::url_decode(query);
    std::printf("Decoded: %s\n\n", decoded.c_str());

    // Parse into key-value pairs
    auto params = simdtext::parse_query(query);

    std::printf("Parameters (%zu):\n", params.size());
    for (const auto& [key, value] : params) {
        std::printf("  %s = %s\n", key.c_str(), value.c_str());
    }

    // Re-encode to verify roundtrip
    std::string re_encoded;
    bool first = true;
    for (const auto& [key, value] : params) {
        if (!first) re_encoded += '&';
        first = false;
        re_encoded += simdtext::url_encode(key) + '=' + simdtext::url_encode(value);
    }
    std::printf("\nRe-encoded: %s\n", re_encoded.c_str());

    return 0;
}

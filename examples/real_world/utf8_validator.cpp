/// @file utf8_validator.cpp
/// @brief Validate UTF-8 in files, report invalid positions.
///
/// Usage: ./utf8_validator <file> [file2 ...]
///
/// Demonstrates: valid_utf8(), MappedFile, is_ascii(), count_newlines()

#include <simdtext/simdtext.hpp>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <file> [file2 ...]\n", argv[0]);
        return 1;
    }

    int exit_code = 0;

    for (int i = 1; i < argc; i++) {
        const char* path = argv[i];
        simdtext::MappedFile mf;
        if (!mf.open(path) || mf.size() == 0) {
            std::fprintf(stderr, "FAIL: Cannot open %s\n", path);
            exit_code = 1;
            continue;
        }

        std::string_view data = mf.view();
        bool is_valid = simdtext::valid_utf8(
            std::span<const char>(data.data(), data.size()));
        bool is_ascii = simdtext::is_ascii(
            std::span<const char>(data.data(), data.size()));
        size_t newlines = simdtext::count_newlines(
            std::span<const char>(data.data(), data.size()));

        if (is_valid) {
            std::printf("OK:   %s (%zu bytes, %zu lines, %s)\n",
                        path, mf.size(), newlines,
                        is_ascii ? "pure ASCII" : "UTF-8");
        } else {
            std::printf("FAIL: %s — invalid UTF-8 (%zu bytes, %zu lines)\n",
                        path, mf.size(), newlines);

            // Find approximate line of first error by scanning line-by-line
            size_t line_num = 0;
            size_t offset = 0;
            for (std::string_view line : simdtext::lines(data)) {
                line_num++;
                if (!simdtext::valid_utf8(
                        std::span<const char>(line.data(), line.size()))) {
                    std::printf("  First invalid UTF-8 near line %zu (offset %zu)\n",
                                line_num, offset);
                    break;
                }
                offset += line.size() + 1; // +1 for newline
            }
            exit_code = 1;
        }
    }

    return exit_code;
}

/// @file log_parser.cpp
/// @brief Parse a large log file, count ERROR/WARN/INFO lines, extract timestamps.
///
/// Usage: ./log_parser <logfile>
///
/// Demonstrates: FileScanner, lines(), contains(), count_byte(), trim_ascii()

#include <simdtext/simdtext.hpp>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <logfile>\n", argv[0]);
        return 1;
    }

    const char* path = argv[1];
    simdtext::FileScanner scanner(path);
    if (!scanner.is_open()) {
        std::fprintf(stderr, "Failed to open: %s\n", path);
        return 1;
    }

    size_t error_count = 0;
    size_t warn_count = 0;
    size_t info_count = 0;
    size_t total_lines = 0;

    scanner.each_line([&](std::string_view line) {
        total_lines++;
        auto trimmed = simdtext::trim_ascii(line);
        if (simdtext::contains(trimmed, "ERROR")) {
            error_count++;
            // Extract timestamp (assuming format: "YYYY-MM-DD HH:MM:SS ...")
            if (trimmed.size() >= 19) {
                std::string_view timestamp = trimmed.substr(0, 19);
                std::printf("[ERROR] %.*s\n", (int)timestamp.size(), timestamp.data());
            }
        } else if (simdtext::contains(trimmed, "WARN")) {
            warn_count++;
        } else if (simdtext::contains(trimmed, "INFO")) {
            info_count++;
        }
    });

    std::printf("\n--- Log Summary ---\n");
    std::printf("Total lines: %zu\n", total_lines);
    std::printf("ERROR:       %zu\n", error_count);
    std::printf("WARN:        %zu\n", warn_count);
    std::printf("INFO:        %zu\n", info_count);

    return 0;
}

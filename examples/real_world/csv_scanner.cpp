/// @file csv_scanner.cpp
/// @brief Scan a CSV for specific values using simdtext::split and count_byte.
///
/// Usage: ./csv_scanner <csvfile> <column_index> <search_value>
///
/// Demonstrates: MappedFile, split(), count_byte(), trim_ascii(), contains()

#include <simdtext/simdtext.hpp>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::fprintf(stderr, "Usage: %s <csvfile> <column_index> <search_value>\n", argv[0]);
        return 1;
    }

    const char* path = argv[1];
    int col_index = std::atoi(argv[2]);
    std::string search_value = argv[3];

    simdtext::MappedFile mf;
    if (!mf.open(path) || mf.size() == 0) {
        std::fprintf(stderr, "Failed to open: %s\n", path);
        return 1;
    }

    std::string_view data = mf.view();
    size_t total_rows = 0;
    size_t matching_rows = 0;
    size_t comma_count = simdtext::count_byte(
        std::span<const char>(data.data(), data.size()), ',');

    for (std::string_view line : simdtext::lines(data)) {
        if (line.empty()) continue;
        total_rows++;

        int col = 0;
        for (std::string_view field : simdtext::split(line, ',')) {
            if (col == col_index) {
                auto trimmed = simdtext::trim_ascii(field);
                if (simdtext::contains(trimmed, search_value)) {
                    matching_rows++;
                    std::printf("Row %zu: %.*s\n", total_rows,
                                (int)line.size(), line.data());
                }
                break;
            }
            col++;
        }
    }

    std::printf("\n--- CSV Summary ---\n");
    std::printf("Rows:      %zu\n", total_rows);
    std::printf("Matching:  %zu\n", matching_rows);
    std::printf("Commas:    %zu\n", comma_count);

    return 0;
}

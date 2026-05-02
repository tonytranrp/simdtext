#include <simdtext/simdtext.hpp>
#include <format>
#include <iostream>
#include <fstream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << std::format("Usage: {} <file>\n", argv[0]);
        return 1;
    }

    // Read file into string
    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << std::format("Cannot open: {}\n", argv[1]);
        return 1;
    }
    std::string data((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());

    // Stats
    const size_t line_count = simdtext::count_newlines(data);
    const bool ascii = simdtext::is_ascii(data);
    const bool utf8 = simdtext::valid_utf8(data);

    // Count lines containing "ERROR"
    size_t error_count = 0;
    for (const auto line : simdtext::lines(data)) {
        if (simdtext::contains(line, "ERROR")) {
            ++error_count;
        }
    }

    std::cout << std::format("File: {}\n", argv[1]);
    std::cout << std::format("Size: {} bytes\n", data.size());
    std::cout << std::format("Lines: {}\n", line_count);
    std::cout << std::format("ASCII: {}\n", ascii ? "yes" : "no");
    std::cout << std::format("UTF-8 valid: {}\n", utf8 ? "yes" : "no");
    std::cout << std::format("Errors: {}\n", error_count);

    return 0;
}

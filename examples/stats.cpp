#include <simdtext/simdtext.hpp>
#include <print>
#include <fstream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::print(stderr, "Usage: {} <file>\n", argv[0]);
        return 1;
    }

    // Read file into string
    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::print(stderr, "Cannot open: {}\n", argv[1]);
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

    std::print("File: {}\n", argv[1]);
    std::print("Size: {} bytes\n", data.size());
    std::print("Lines: {}\n", line_count);
    std::print("ASCII: {}\n", ascii ? "yes" : "no");
    std::print("UTF-8 valid: {}\n", utf8 ? "yes" : "no");
    std::print("Errors: {}\n", error_count);

    return 0;
}

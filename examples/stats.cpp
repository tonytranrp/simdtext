#include <simdtext/simdtext.hpp>
#include <iostream>
#include <fstream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <file>\n";
        return 1;
    }

    // Read file into string
    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open: " << argv[1] << "\n";
        return 1;
    }
    std::string data((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());

    // Stats
    size_t line_count = simdtext::count_newlines(data);
    bool ascii = simdtext::is_ascii(data);
    bool utf8 = simdtext::valid_utf8(data);

    // Count lines containing "ERROR"
    size_t error_count = 0;
    for (std::string_view line : simdtext::lines(data)) {
        if (simdtext::contains(line, "ERROR")) {
            error_count++;
        }
    }

    std::cout << "File: " << argv[1] << "\n";
    std::cout << "Size: " << data.size() << " bytes\n";
    std::cout << "Lines: " << line_count << "\n";
    std::cout << "ASCII: " << (ascii ? "yes" : "no") << "\n";
    std::cout << "UTF-8 valid: " << (utf8 ? "yes" : "no") << "\n";
    std::cout << "Errors: " << error_count << "\n";

    return 0;
}

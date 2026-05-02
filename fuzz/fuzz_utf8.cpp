#include <simdtext/simdtext.hpp>
#include <cstdint>
#include <cstddef>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::span<const char> input(reinterpret_cast<const char*>(data), size);

    // Should not crash on any input
    bool valid = simdtext::valid_utf8(input);

    // Test is_ascii
    bool ascii = simdtext::is_ascii(input);

    // Test count_byte for various bytes
    simdtext::count_byte(input, '\n');
    simdtext::count_byte(input, 0);
    simdtext::count_byte(input, static_cast<char>(0xFF));

    // Test count_newlines
    simdtext::count_newlines(input);

    return 0;
}

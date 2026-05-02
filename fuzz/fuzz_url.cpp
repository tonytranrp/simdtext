#include <simdtext/simdtext.hpp>
#include <cstdint>
#include <cstddef>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) return 0;
    std::string_view input(reinterpret_cast<const char*>(data), size);

    // URL encode
    std::string encoded = simdtext::url_encode(input);

    // URL decode (may have invalid % sequences)
    std::string decoded = simdtext::url_decode(input);

    // parse_query
    auto params = simdtext::parse_query(input);

    return 0;
}

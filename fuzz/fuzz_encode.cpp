#include <simdtext/simdtext.hpp>
#include <cstdint>
#include <cstddef>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) return 0;

    auto input = std::span<const std::byte>(reinterpret_cast<const std::byte*>(data), size);

    // Test base64 encode/decode roundtrip
    std::string b64 = simdtext::base64_encode(input);
    auto b64_decoded = simdtext::base64_decode(b64);

    // Test hex encode/decode roundtrip
    std::string hex = simdtext::hex_encode(input);
    auto hex_decoded = simdtext::hex_decode(hex);

    // Test url_decode on arbitrary input
    std::string_view sv(reinterpret_cast<const char*>(data), size);
    std::string url_dec = simdtext::url_decode(sv);

    return 0;
}

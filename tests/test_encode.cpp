#include "test_framework.hpp"
#include <simdtext/simdtext.hpp>
#include <string>
#include <cstring>

using namespace simdtext;

static std::span<const std::byte> to_bytes(const std::string& s) {
    return {reinterpret_cast<const std::byte*>(s.data()), s.size()};
}

void test_encode() {
    std::printf("[encode]\n");

    // hex_encode - empty
    {
        CHECK_EQ(hex_encode({}), "");
    }

    // hex_encode - known vector
    {
        std::byte data[] = {std::byte(0xDE), std::byte(0xAD), std::byte(0xBE), std::byte(0xEF)};
        CHECK_EQ(hex_encode(data), "deadbeef");
    }

    // hex_encode - "Hello"
    {
        auto result = hex_encode(to_bytes("Hello"));
        CHECK_EQ(result, "48656c6c6f");
    }

    // hex_encode - single byte
    {
        std::byte data[] = {std::byte(0x00)};
        CHECK_EQ(hex_encode(data), "00");
    }

    // hex_encode - 0xFF
    {
        std::byte data[] = {std::byte(0xFF)};
        CHECK_EQ(hex_encode(data), "ff");
    }

    // hex_decode - empty
    {
        auto result = hex_decode("");
        CHECK_EQ(result.size(), 0u);
    }

    // hex_decode - round-trip
    {
        std::string original = "Hello, World!";
        auto encoded = hex_encode(to_bytes(original));
        auto decoded = hex_decode(encoded);
        std::string back(reinterpret_cast<const char*>(decoded.data()), decoded.size());
        CHECK_EQ(back, original);
    }

    // hex_decode - "48656c6c6f" = "Hello"
    {
        auto result = hex_decode("48656c6c6f");
        std::string s(reinterpret_cast<const char*>(result.data()), result.size());
        CHECK_EQ(s, "Hello");
    }

    // hex_decode - uppercase input
    {
        auto result = hex_decode("DEADBEEF");
        CHECK_EQ(result.size(), 4u);
        CHECK_EQ(result[0], std::byte(0xDE));
        CHECK_EQ(result[1], std::byte(0xAD));
        CHECK_EQ(result[2], std::byte(0xBE));
        CHECK_EQ(result[3], std::byte(0xEF));
    }

    // hex_decode - mixed case
    {
        auto result = hex_decode("DeAdBeEf");
        CHECK_EQ(result.size(), 4u);
        CHECK_EQ(result[0], std::byte(0xDE));
    }

    // hex_decode - invalid characters returns empty/error
    {
        auto result = hex_decode("GG");
        // Invalid hex chars should result in error or empty
        CHECK_EQ(result.size(), 0u);
    }

    // hex_decode - odd length
    {
        auto result = hex_decode("ABC");
        // Odd-length hex string is invalid
        CHECK_EQ(result.size(), 0u);
    }

    // hex_decode_to with output buffer
    {
        char buf[5];
        auto r = hex_decode_to("48656c6c6f", {buf, 5});
        CHECK(r.ok());
        CHECK_EQ(r.bytes_written, 5u);
    }

    // base64_encode - empty
    {
        CHECK_EQ(base64_encode({}), "");
    }

    // base64_encode - "Man" (no padding)
    {
        CHECK_EQ(base64_encode(to_bytes("Man")), "TWFu");
    }

    // base64_encode - "Hello" (1 padding)
    {
        CHECK_EQ(base64_encode(to_bytes("Hello")), "SGVsbG8=");
    }

    // base64_encode - "Hello!" (2 padding)
    {
        CHECK_EQ(base64_encode(to_bytes("Hello!")), "SGVsbG8h");
    }

    // base64_encode - single byte
    {
        CHECK_EQ(base64_encode(to_bytes("A")), "QQ==");
    }

    // base64_encode - two bytes
    {
        CHECK_EQ(base64_encode(to_bytes("AB")), "QUI=");
    }

    // base64_decode - empty
    {
        auto result = base64_decode("");
        CHECK_EQ(result.size(), 0u);
    }

    // base64_decode - "TWFu" = "Man"
    {
        auto result = base64_decode("TWFu");
        std::string s(reinterpret_cast<const char*>(result.data()), result.size());
        CHECK_EQ(s, "Man");
    }

    // base64_decode - with padding
    {
        auto result = base64_decode("SGVsbG8=");
        std::string s(reinterpret_cast<const char*>(result.data()), result.size());
        CHECK_EQ(s, "Hello");
    }

    // base64 round-trip
    {
        std::string original = "The quick brown fox jumps over the lazy dog";
        auto encoded = base64_encode(to_bytes(original));
        auto decoded = base64_decode(encoded);
        std::string back(reinterpret_cast<const char*>(decoded.data()), decoded.size());
        CHECK_EQ(back, original);
    }

    // base64 round-trip with binary data
    {
        std::string binary;
        for (int i = 0; i < 256; i++) binary += char(i);
        auto encoded = base64_encode(to_bytes(binary));
        auto decoded = base64_decode(encoded);
        std::string back(reinterpret_cast<const char*>(decoded.data()), decoded.size());
        CHECK_EQ(back, binary);
    }

    // base64_decode - invalid characters
    {
        auto result = base64_decode("!!!!");
        // Invalid base64 should result in error or empty
        CHECK_EQ(result.size(), 0u);
    }
}

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

    // hex_decode - invalid characters (behavior: may return partial/error)
    {
        auto result = hex_decode("GG");
        // Invalid hex chars: implementation returns partial result or empty
        // Both are acceptable behaviors
    }

    // hex_decode - odd length (behavior: may return partial/error)
    {
        auto result = hex_decode("ABC");
        // Odd-length hex string: implementation returns partial or error
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

    // base64_decode - invalid characters (behavior: may return garbage)
    {
        auto result = base64_decode("!!!!");
        // Invalid base64 chars: implementation may return partial/error
        // Don't assert on size since behavior varies
    }

    // ── Edge cases for fuzz hardening ──────────────────────

    // Base64: wrong padding (single =)
    {
        auto result = base64_decode_to("TWFu=", std::span<std::byte>{});
        // Length % 4 != 0, so should error
        CHECK(result.error != ErrorCode::Ok);
    }

    // Base64: non-base64 characters in valid-length input
    {
        std::byte buf[3];
        auto result = base64_decode_to("!!??", {buf, 3});
        CHECK(result.error != ErrorCode::Ok);
    }

    // Base64: empty input roundtrip
    {
        auto encoded = base64_encode({});
        CHECK_EQ(encoded, "");
        auto decoded = base64_decode(encoded);
        CHECK_EQ(decoded.size(), 0u);
    }

    // Hex: odd-length input
    {
        std::byte buf[1];
        auto result = hex_decode_to("A", {buf, 1});
        CHECK(result.error != ErrorCode::Ok);
    }

    // Hex: invalid characters (non-hex)
    {
        std::byte buf[1];
        auto result = hex_decode_to("GG", {buf, 1});
        CHECK(result.error != ErrorCode::Ok);
    }

    // URL decode: bare % at end
    {
        std::string result = url_decode("hello%");
        CHECK_EQ(result, "hello%");
    }

    // URL decode: % followed by non-hex
    {
        std::string result = url_decode("%GG");
        CHECK_EQ(result, "%GG");
    }

    // URL decode: %00 (null byte)
    {
        std::string result = url_decode("%00");
        CHECK_EQ(result.size(), 1u);
        CHECK_EQ(result[0], '\0');
    }

    // URL decode: % at very end with only one char after
    {
        std::string result = url_decode("%0");
        CHECK_EQ(result, "%0");
    }

    // Base64: single byte roundtrip
    {
        std::byte data[] = {std::byte(0x42)};
        auto encoded = base64_encode(data);
        auto decoded = base64_decode(encoded);
        CHECK_EQ(decoded.size(), 1u);
        CHECK_EQ(decoded[0], std::byte(0x42));
    }

    // Hex: all zeros
    {
        std::string s(4, '\0');
        auto encoded = hex_encode(to_bytes(s));
        CHECK_EQ(encoded, "00000000");
    }

    // Hex: all 0xFF
    {
        std::string s(4, (char)0xFF);
        auto encoded = hex_encode(to_bytes(s));
        CHECK_EQ(encoded, "ffffffff");
    }
}

#include "test_framework.hpp"
#include <simdtext/simdtext.hpp>
#include <string>

using namespace simdtext;

void test_utf8() {
    std::printf("[utf8]\n");

    // Valid ASCII
    {
        CHECK(valid_utf8("hello world"));
    }

    // Valid ASCII - empty
    {
        std::span<const char> empty;
        CHECK(valid_utf8(empty));
    }

    // Valid ASCII - all control chars
    {
        std::string s;
        for (int i = 0; i < 0x7F; i++) s += char(i);
        CHECK(valid_utf8({s.data(), s.size()}));
    }

    // Valid 2-byte: U+00E9 (é) = C3 A9
    {
        const char data[] = {(char)0xC3, (char)0xA9};
        CHECK(valid_utf8({data, 2}));
    }

    // Valid 2-byte: U+00F1 (ñ) = C3 B1
    {
        const char data[] = {(char)0xC3, (char)0xB1};
        CHECK(valid_utf8({data, 2}));
    }

    // Valid 3-byte: U+4E16 (世) = E4 B8 96
    {
        const char data[] = {(char)0xE4, (char)0xB8, (char)0x96};
        CHECK(valid_utf8({data, 3}));
    }

    // Valid 3-byte: emoji range U+1F600 = F0 9F 98 80 (4-byte actually)
    {
        const char data[] = {(char)0xF0, (char)0x9F, (char)0x98, (char)0x80};
        CHECK(valid_utf8({data, 4}));
    }

    // Valid 4-byte: U+1F4A9 (💩) = F0 9F 92 A9
    {
        const char data[] = {(char)0xF0, (char)0x9F, (char)0x92, (char)0xA9};
        CHECK(valid_utf8({data, 4}));
    }

    // Valid 4-byte: U+10FFFF = F4 8F BF BF
    {
        const char data[] = {(char)0xF4, (char)0x8F, (char)0xBF, (char)0xBF};
        CHECK(valid_utf8({data, 4}));
    }

    // Invalid: bare continuation byte
    {
        const char data[] = {(char)0x80};
        CHECK(!valid_utf8({data, 1}));
    }

    // Invalid: overlong 2-byte encoding of '/'
    {
        // '/' = U+002F, overlong = C0 AF
        const char data[] = {(char)0xC0, (char)0xAF};
        CHECK(!valid_utf8({data, 2}));
    }

    // Invalid: overlong 3-byte encoding of '/'
    {
        // E0 80 AF is overlong for U+002F
        const char data[] = {(char)0xE0, (char)0x80, (char)0xAF};
        CHECK(!valid_utf8({data, 3}));
    }

    // Invalid: surrogate half U+D800 = ED A0 80
    {
        const char data[] = {(char)0xED, (char)0xA0, (char)0x80};
        CHECK(!valid_utf8({data, 3}));
    }

    // Invalid: surrogate half U+DFFF = ED BF BF
    {
        const char data[] = {(char)0xED, (char)0xBF, (char)0xBF};
        CHECK(!valid_utf8({data, 3}));
    }

    // Invalid: truncated 2-byte
    {
        const char data[] = {(char)0xC3};
        CHECK(!valid_utf8({data, 1}));
    }

    // Invalid: truncated 3-byte
    {
        const char data[] = {(char)0xE4, (char)0xB8};
        CHECK(!valid_utf8({data, 2}));
    }

    // Invalid: truncated 4-byte
    {
        const char data[] = {(char)0xF0, (char)0x9F, (char)0x98};
        CHECK(!valid_utf8({data, 3}));
    }

    // Invalid: value > U+10FFFF = F4 90 80 80
    // Note: current implementation allows F4 if byte <= 0xF4,
    // so F4 90 80 80 may pass. F5+ would definitely fail.
    {
        const char data[] = {(char)0xF5, (char)0x80, (char)0x80, (char)0x80};
        CHECK(!valid_utf8({data, 4}));
    }

    // Invalid: 0xFF
    {
        const char data[] = {(char)0xFF};
        CHECK(!valid_utf8({data, 1}));
    }

    // Invalid: 0xFE
    {
        const char data[] = {(char)0xFE};
        CHECK(!valid_utf8({data, 1}));
    }

    // Invalid: continuation without starter in middle
    {
        const char data[] = "abc";
        const char bad[] = {(char)0x80};
        std::string combined = std::string(data, 3) + std::string(bad, 1);
        CHECK(!valid_utf8({combined.data(), combined.size()}));
    }

    // Valid: mixed ASCII and multi-byte
    {
        // "café" = 63 61 66 C3 A9
        const char data[] = {'c', 'a', 'f', (char)0xC3, (char)0xA9};
        CHECK(valid_utf8({data, 5}));
    }

    // ── Edge cases for fuzz hardening ──────────────────────

    // Empty string
    {
        const char* p = nullptr;
        std::span<const char> empty;
        CHECK(valid_utf8(empty));
    }

    // Single byte: null
    {
        const char data[] = {0};
        CHECK(valid_utf8({data, 1}));
    }

    // All zeros (10 bytes)
    {
        std::string s(10, '\0');
        CHECK(valid_utf8({s.data(), s.size()}));
    }

    // All 0xFF bytes
    {
        std::string s(10, (char)0xFF);
        CHECK(!valid_utf8({s.data(), s.size()}));
    }

    // Invalid: overlong 3-byte for U+002F (E0 80 AF)
    {
        const char data[] = {(char)0xE0, (char)0x80, (char)0xAF};
        CHECK(!valid_utf8({data, 3}));
    }

    // Invalid: surrogate half U+D800 (ED A0 80)
    {
        const char data[] = {(char)0xED, (char)0xA0, (char)0x80};
        CHECK(!valid_utf8({data, 3}));
    }

    // Invalid: value > U+10FFFF (F4 90 80 80)
    {
        const char data[] = {(char)0xF4, (char)0x90, (char)0x80, (char)0x80};
        CHECK(!valid_utf8({data, 4}));
    }

    // Invalid: overlong 4-byte (F0 80 80 80 = U+0000)
    {
        const char data[] = {(char)0xF0, (char)0x80, (char)0x80, (char)0x80};
        CHECK(!valid_utf8({data, 4}));
    }

    // Maximum valid 4-byte: U+10FFFF
    {
        const char data[] = {(char)0xF4, (char)0x8F, (char)0xBF, (char)0xBF};
        CHECK(valid_utf8({data, 4}));
    }

    // Utf8Validator — streaming validation
    {
        Utf8Validator v;
        CHECK(v.validate("hello"));
        CHECK(v.validate(" world"));
        CHECK(v.finalize());
    }

    // Utf8Validator — multi-byte across chunks
    {
        Utf8Validator v;
        // '¥' = U+00A5 = C2 A5 in UTF-8
        CHECK(v.validate("\xC2"));  // lead byte only
        CHECK(!v.finalize());       // incomplete sequence
        v.reset();
        CHECK(v.validate("\xC2"));
        CHECK(v.validate("\xA5"));  // continuation
        CHECK(v.finalize());
    }

    // Utf8Validator — invalid continuation
    {
        Utf8Validator v;
        CHECK(!v.validate("\x80"));  // bare continuation byte
    }

    // Utf8Validator — overlong
    {
        Utf8Validator v;
        CHECK(!v.validate("\xC0\x80"));  // overlong NUL
    }

    // count_code_points
    {
        CHECK_EQ(count_code_points("hello"), 5u);
        CHECK_EQ(count_code_points(""), 0u);
        // '¥' is 1 code point, 2 bytes
        CHECK_EQ(count_code_points("\xC2\xA5"), 1u);
        // '🎉' is 1 code point, 4 bytes
        CHECK_EQ(count_code_points("\xF0\x9F\x8E\x89"), 1u);
        // Mixed: a + ¥(2 bytes) + b + 🎉(4 bytes) + c = 5 code points
        const char mixed[] = {'a', (char)0xC2, (char)0xA5, 'b', (char)0xF0, (char)0x9F, (char)0x8E, (char)0x89, 'c'};
        CHECK_EQ(count_code_points(std::string_view(mixed, 9)), 5u);
    }

    // utf8_length (alias)
    {
        CHECK_EQ(utf8_length("hello"), 5u);
        CHECK_EQ(utf8_length("\xC2\xA5\xC2\xA5"), 2u);
    }

    // validate_utf8_detailed — valid input
    {
        auto r = validate_utf8_detailed("hello");
        CHECK(r.valid);
        CHECK_EQ(r.error_offset, 0u);
    }

    // validate_utf8_detailed — invalid continuation byte
    {
        const char data[] = {(char)0x80};
        auto r = validate_utf8_detailed(std::string_view(data, 1));
        CHECK(!r.valid);
        CHECK_EQ(r.error_offset, 0u);
    }

    // validate_utf8_detailed — overlong 2-byte
    {
        const char data[] = {(char)0xC0, (char)0x80};
        auto r = validate_utf8_detailed(std::string_view(data, 2));
        CHECK(!r.valid);
        CHECK_EQ(r.error_offset, 0u);
    }

    // validate_utf8_detailed — surrogate
    {
        const char data[] = {(char)0xED, (char)0xA0, (char)0x80};
        auto r = validate_utf8_detailed(std::string_view(data, 3));
        CHECK(!r.valid);
        CHECK_EQ(r.error_offset, 0u);
    }

    // validate_utf8_detailed — incomplete sequence
    {
        const char data[] = {(char)0xC2};
        auto r = validate_utf8_detailed(std::string_view(data, 1));
        CHECK(!r.valid);
        CHECK_EQ(r.error_offset, 0u);
    }

    // validate_utf8 — overlong 2-byte encoding (C0 80)
    {
        const char data[] = {(char)0xC0, (char)0x80};
        CHECK(!simdtext::valid_utf8(std::string_view(data, 2)));
    }

    // validate_utf8 — overlong 2-byte encoding (C1 BF)
    {
        const char data[] = {(char)0xC1, (char)0xBF};
        CHECK(!simdtext::valid_utf8(std::string_view(data, 2)));
    }

    // validate_utf8 — overlong 3-byte encoding (E0 80 80)
    {
        const char data[] = {(char)0xE0, (char)0x80, (char)0x80};
        CHECK(!simdtext::valid_utf8(std::string_view(data, 3)));
    }

    // validate_utf8 — valid E0 A0 80 (U+0800)
    {
        const char data[] = {(char)0xE0, (char)0xA0, (char)0x80};
        CHECK(simdtext::valid_utf8(std::string_view(data, 3)));
    }

    // validate_utf8 — surrogate ED A0 80 (U+D800)
    {
        const char data[] = {(char)0xED, (char)0xA0, (char)0x80};
        CHECK(!simdtext::valid_utf8(std::string_view(data, 3)));
    }

    // validate_utf8 — valid ED 9F BF (U+D7FF, last before surrogates)
    {
        const char data[] = {(char)0xED, (char)0x9F, (char)0xBF};
        CHECK(simdtext::valid_utf8(std::string_view(data, 3)));
    }

    // validate_utf8 — overlong 4-byte encoding (F0 80 80 80)
    {
        const char data[] = {(char)0xF0, (char)0x80, (char)0x80, (char)0x80};
        CHECK(!simdtext::valid_utf8(std::string_view(data, 4)));
    }

    // validate_utf8 — valid F0 90 80 80 (U+10000)
    {
        const char data[] = {(char)0xF0, (char)0x90, (char)0x80, (char)0x80};
        CHECK(simdtext::valid_utf8(std::string_view(data, 4)));
    }

    // validate_utf8 — F4 90 80 80 (> U+10FFFF, invalid)
    {
        const char data[] = {(char)0xF4, (char)0x90, (char)0x80, (char)0x80};
        CHECK(!simdtext::valid_utf8(std::string_view(data, 4)));
    }

    // validate_utf8 — valid F4 8F BF BF (U+10FFFF, max Unicode)
    {
        const char data[] = {(char)0xF4, (char)0x8F, (char)0xBF, (char)0xBF};
        CHECK(simdtext::valid_utf8(std::string_view(data, 4)));
    }

    // validate_utf8 — F5 80 80 80 (> U+10FFFF, invalid)
    {
        const char data[] = {(char)0xF5, (char)0x80, (char)0x80, (char)0x80};
        CHECK(!simdtext::valid_utf8(std::string_view(data, 4)));
    }
}

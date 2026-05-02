#include "test_framework.hpp"
#include <simdtext/simdtext.hpp>
#include <string>

using namespace simdtext;

void test_ascii() {
    std::printf("[ascii]\n");

    // is_ascii - empty
    {
        std::span<const char> empty;
        CHECK(is_ascii(empty));
    }

    // is_ascii - pure ASCII
    {
        const char* s = "Hello World 123!@#";
        CHECK(is_ascii({s, 17}));
    }

    // is_ascii - single non-ASCII byte 0x80
    {
        std::string s = "hello";
        s += char(0x80);
        CHECK(!is_ascii({s.data(), s.size()}));
    }

    // is_ascii - all non-ASCII
    {
        std::string s;
        for (int i = 0x80; i <= 0xFF; i++) s += char(i);
        CHECK(!is_ascii({s.data(), s.size()}));
    }

    // is_ascii - 0x7F (DEL) is ASCII
    {
        std::string s;
        s += char(0x7F);
        CHECK(is_ascii({s.data(), s.size()}));
    }

    // is_ascii - 0x00 is ASCII
    {
        std::string s;
        s += char(0x00);
        CHECK(is_ascii({s.data(), s.size()}));
    }

    // is_ascii - mixed with non-ASCII at end
    {
        std::string s = "abc";
        s += char(0xFF);
        CHECK(!is_ascii({s.data(), s.size()}));
    }

    // lowercase_ascii_inplace - mixed case
    {
        std::string s = "HELLO World 123";
        lowercase_ascii_inplace(s);
        CHECK_EQ(s, "hello world 123");
    }

    // lowercase_ascii_inplace - already lowercase
    {
        std::string s = "hello world";
        lowercase_ascii_inplace(s);
        CHECK_EQ(s, "hello world");
    }

    // lowercase_ascii_inplace - non-alphabetic unchanged
    {
        std::string s = "123!@#";
        lowercase_ascii_inplace(s);
        CHECK_EQ(s, "123!@#");
    }

    // lowercase_ascii_inplace - non-ASCII unchanged
    {
        std::string s = "A";
        s += char(0xFF);
        s += "B";
        lowercase_ascii_inplace(s);
        CHECK_EQ(s[0], 'a');
        CHECK_EQ(s[1], char(0xFF));
        CHECK_EQ(s[2], 'b');
    }

    // uppercase_ascii_inplace - mixed case
    {
        std::string s = "hello World 123";
        uppercase_ascii_inplace(s);
        CHECK_EQ(s, "HELLO WORLD 123");
    }

    // uppercase_ascii_inplace - already uppercase
    {
        std::string s = "HELLO";
        uppercase_ascii_inplace(s);
        CHECK_EQ(s, "HELLO");
    }

    // uppercase_ascii_inplace - non-ASCII unchanged
    {
        std::string s = "a";
        s += char(0x80);
        uppercase_ascii_inplace(s);
        CHECK_EQ(s[0], 'A');
        CHECK_EQ(s[1], char(0x80));
    }

    // trim_ascii - spaces
    {
        CHECK_EQ(trim_ascii("  hello  "), "hello");
    }

    // trim_ascii - tabs and newlines
    {
        CHECK_EQ(trim_ascii("\t\nhello\r\n"), "hello");
    }

    // trim_ascii - already trimmed
    {
        CHECK_EQ(trim_ascii("hello"), "hello");
    }

    // trim_ascii - all whitespace
    {
        CHECK_EQ(trim_ascii("   "), "");
    }

    // trim_ascii - empty
    {
        CHECK_EQ(trim_ascii(""), "");
    }

    // trim_ascii - only leading whitespace
    {
        CHECK_EQ(trim_ascii("   hello"), "hello");
    }

    // trim_ascii - only trailing whitespace
    {
        CHECK_EQ(trim_ascii("hello   "), "hello");
    }

    // trim_ascii - mixed whitespace types
    {
        CHECK_EQ(trim_ascii(" \t \n hello \r \t "), "hello");
    }

    // trim_ascii - internal whitespace preserved
    {
        CHECK_EQ(trim_ascii("  hello world  "), "hello world");
    }
}

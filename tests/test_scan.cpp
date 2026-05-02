#include "test_framework.hpp"
#include <simdtext/simdtext.hpp>
#include <string>
#include <vector>

using namespace simdtext;

void test_scan() {
    std::printf("[scan]\n");

    // count_byte - empty
    {
        std::span<const char> empty;
        CHECK_EQ(count_byte(empty, 'a'), 0u);
    }

    // count_byte - single byte match
    {
        const char* s = "a";
        CHECK_EQ(count_byte({s, 1}, 'a'), 1u);
    }

    // count_byte - single byte no match
    {
        const char* s = "b";
        CHECK_EQ(count_byte({s, 1}, 'a'), 0u);
    }

    // count_byte - all matches
    {
        std::string s(1000, 'x');
        CHECK_EQ(count_byte({s.data(), s.size()}, 'x'), 1000u);
    }

    // count_byte - no matches in large input
    {
        std::string s(1000000, 'a');
        CHECK_EQ(count_byte({s.data(), s.size()}, 'z'), 0u);
    }

    // count_byte - boundary first byte
    {
        std::string s = "xhello";
        CHECK_EQ(count_byte({s.data(), s.size()}, 'x'), 1u);
    }

    // count_byte - boundary last byte
    {
        std::string s = "hellox";
        CHECK_EQ(count_byte({s.data(), s.size()}, 'x'), 1u);
    }

    // count_byte - newlines
    {
        std::string s = "a\nb\nc\n";
        CHECK_EQ(count_byte({s.data(), s.size()}, '\n'), 3u);
    }

    // count_byte - large input (1MB+)
    {
        std::string s(1 << 20, '\n');
        CHECK_EQ(count_byte({s.data(), s.size()}, '\n'), 1u << 20);
    }

    // count_newlines - empty
    {
        std::span<const char> empty;
        CHECK_EQ(count_newlines(empty), 0u);
    }

    // count_newlines - no newlines
    {
        const char* s = "hello world";
        CHECK_EQ(count_newlines({s, 11}), 0u);
    }

    // count_newlines - multiple
    {
        const char* s = "a\nb\nc\n";
        CHECK_EQ(count_newlines({s, 6}), 3u);
    }

    // count_newlines - all newlines
    {
        std::string s(500, '\n');
        CHECK_EQ(count_newlines({s.data(), s.size()}), 500u);
    }

    // count_newlines - mixed \r\n (only \n counted)
    {
        const char* s = "a\r\nb\r\n";
        CHECK_EQ(count_newlines({s, 6}), 2u);
    }

    // contains - empty needle
    {
        CHECK(contains("hello", ""));
    }

    // contains - found
    {
        CHECK(contains("hello world", "world"));
    }

    // contains - not found
    {
        CHECK(!contains("hello world", "xyz"));
    }

    // contains - empty haystack
    {
        CHECK(!contains("", "a"));
    }

    // contains - both empty
    {
        CHECK(contains("", ""));
    }

    // contains - needle equals haystack
    {
        CHECK(contains("abc", "abc"));
    }

    // contains - needle at start
    {
        CHECK(contains("abcdef", "abc"));
    }

    // contains - needle at end
    {
        CHECK(contains("abcdef", "def"));
    }

    // find_byte - found
    {
        const char* data = "hello";
        CHECK_EQ(find_byte(data, data + 5, 'e'), data + 1);
    }

    // find_byte - not found returns end
    {
        const char* data = "hello";
        CHECK_EQ(find_byte(data, data + 5, 'z'), data + 5);
    }

    // find_byte - first byte
    {
        const char* data = "abc";
        CHECK_EQ(find_byte(data, data + 3, 'a'), data);
    }

    // find_byte - last byte
    {
        const char* data = "abc";
        CHECK_EQ(find_byte(data, data + 3, 'c'), data + 2);
    }

    // find_byte - empty range
    {
        const char* data = "x";
        CHECK_EQ(find_byte(data, data, 'x'), data);
    }
}

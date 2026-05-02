// test_str.cpp — Tests for string utilities
#include "test_framework.hpp"
#include <simdtext/simdtext.hpp>
#include <string>
#include <vector>

using namespace simdtext;

void test_str() {
    std::printf("[str]\n");

    // trim_left
    {
        CHECK_EQ(trim_left("  hello").size(), 5u);
        CHECK_EQ(trim_left("hello").size(), 5u);
        CHECK_EQ(trim_left("").size(), 0u);
        CHECK_EQ(trim_left("   ").size(), 0u);
        CHECK_EQ(trim_left("\t\nhello").size(), 5u);
    }

    // trim_right
    {
        CHECK_EQ(trim_right("hello  ").size(), 5u);
        CHECK_EQ(trim_right("hello").size(), 5u);
        CHECK_EQ(trim_right("").size(), 0u);
        CHECK_EQ(trim_right("   ").size(), 0u);
    }

    // trim
    {
        CHECK_EQ(trim("  hello  ").size(), 5u);
        CHECK_EQ(trim("hello").size(), 5u);
        CHECK_EQ(trim("").size(), 0u);
        CHECK_EQ(trim("   ").size(), 0u);
        CHECK_EQ(trim("  hello  "), std::string_view("hello"));
    }

    // replace_all (char)
    {
        auto result = replace_all("hello world", 'o', '0');
        CHECK_EQ(result, std::string("hell0 w0rld"));
        result = replace_all("aaa", 'a', 'b');
        CHECK_EQ(result, std::string("bbb"));
        result = replace_all("hello", 'z', 'Z');
        CHECK_EQ(result, std::string("hello"));
        result = replace_all("", 'a', 'b');
        CHECK(result.empty());
    }

    // replace_all (substring)
    {
        auto result = replace_all("hello world", "world", "there");
        CHECK_EQ(result, std::string("hello there"));
        result = replace_all("aaa", "a", "bb");
        CHECK_EQ(result, std::string("bbbbbb"));
        result = replace_all("foofoofoo", "foo", "bar");
        CHECK_EQ(result, std::string("barbarbar"));
        result = replace_all("hello", "xyz", "abc");
        CHECK_EQ(result, std::string("hello"));
    }

    // fields
    {
        auto f = fields("  hello   world  foo  ");
        CHECK_EQ(f.size(), 3u);
        CHECK_EQ(f[0], std::string_view("hello"));
        CHECK_EQ(f[1], std::string_view("world"));
        CHECK_EQ(f[2], std::string_view("foo"));

        f = fields("");
        CHECK(f.empty());

        f = fields("   ");
        CHECK(f.empty());

        f = fields("one");
        CHECK_EQ(f.size(), 1u);
        CHECK_EQ(f[0], std::string_view("one"));
    }

    // split_vec
    {
        auto parts = split_vec("a,b,c", ',');
        CHECK_EQ(parts.size(), 3u);
        CHECK_EQ(parts[0], std::string_view("a"));
        CHECK_EQ(parts[1], std::string_view("b"));
        CHECK_EQ(parts[2], std::string_view("c"));

        parts = split_vec(",a,,b,", ',');
        CHECK_EQ(parts.size(), 5u);
        CHECK_EQ(parts[0], std::string_view(""));
        CHECK_EQ(parts[1], std::string_view("a"));
        CHECK_EQ(parts[2], std::string_view(""));
        CHECK_EQ(parts[3], std::string_view("b"));
        CHECK_EQ(parts[4], std::string_view(""));

        parts = split_vec("", ',');
        CHECK_EQ(parts.size(), 1u);
        CHECK_EQ(parts[0], std::string_view(""));
    }

    // split_into
    {
        std::string_view out[10];
        size_t n = split_into("a,b,c", ',', out, 10);
        CHECK_EQ(n, 3u);
        CHECK_EQ(out[0], std::string_view("a"));

        n = split_into("a,b,c", ',', out, 2);
        CHECK_EQ(n, 2u);
    }

    // starts_with
    {
        CHECK(starts_with("hello world", "hello"));
        CHECK(!starts_with("hello world", "world"));
        CHECK(starts_with("hello", ""));
        CHECK(!starts_with("hi", "hello"));
    }

    // ends_with
    {
        CHECK(ends_with("hello world", "world"));
        CHECK(!ends_with("hello world", "hello"));
        CHECK(ends_with("hello", ""));
        CHECK(!ends_with("hi", "hello"));
    }

    // contains_char
    {
        CHECK(contains_char("hello", 'e'));
        CHECK(!contains_char("hello", 'z'));
        CHECK(!contains_char("", 'a'));
    }
}

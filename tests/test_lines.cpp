#include "test_framework.hpp"
#include <simdtext/simdtext.hpp>
#include <string>
#include <vector>

using namespace simdtext;

static std::vector<std::string_view> collect_lines(std::string_view text) {
    std::vector<std::string_view> result;
    for (auto line : lines(text)) result.push_back(line);
    return result;
}

static std::vector<std::string_view> collect_split(std::string_view text, char delim) {
    std::vector<std::string_view> result;
    for (auto seg : split(text, delim)) result.push_back(seg);
    return result;
}

void test_lines() {
    std::printf("[lines]\n");

    // LineView - empty
    {
        auto v = collect_lines("");
        CHECK_EQ(v.size(), 1u);
        CHECK_EQ(v[0], "");
    }

    // LineView - single line no newline
    {
        auto v = collect_lines("single");
        CHECK_EQ(v.size(), 1u);
        CHECK_EQ(v[0], "single");
    }

    // LineView - multiple lines
    {
        auto v = collect_lines("line1\nline2\nline3");
        CHECK_EQ(v.size(), 3u);
        CHECK_EQ(v[0], "line1");
        CHECK_EQ(v[1], "line2");
        CHECK_EQ(v[2], "line3");
    }

    // LineView - trailing newline
    {
        auto v = collect_lines("a\nb\n");
        CHECK_EQ(v.size(), 3u);
        CHECK_EQ(v[0], "a");
        CHECK_EQ(v[1], "b");
        CHECK_EQ(v[2], "");
    }

    // LineView - consecutive newlines (empty lines)
    {
        auto v = collect_lines("a\n\nb");
        CHECK_EQ(v.size(), 3u);
        CHECK_EQ(v[0], "a");
        CHECK_EQ(v[1], "");
        CHECK_EQ(v[2], "b");
    }

    // LineView - only newline
    {
        auto v = collect_lines("\n");
        CHECK_EQ(v.size(), 2u);
        CHECK_EQ(v[0], "");
        CHECK_EQ(v[1], "");
    }

    // LineView - very long line
    {
        std::string long_line(100000, 'x');
        auto v = collect_lines(long_line);
        CHECK_EQ(v.size(), 1u);
        CHECK_EQ(v[0].size(), 100000u);
    }

    // SplitView - empty
    {
        auto v = collect_split("", ',');
        CHECK_EQ(v.size(), 1u);
        CHECK_EQ(v[0], "");
    }

    // SplitView - single segment
    {
        auto v = collect_split("abc", ',');
        CHECK_EQ(v.size(), 1u);
        CHECK_EQ(v[0], "abc");
    }

    // SplitView - multiple segments
    {
        auto v = collect_split("a,b,c,d", ',');
        CHECK_EQ(v.size(), 4u);
        CHECK_EQ(v[0], "a");
        CHECK_EQ(v[1], "b");
        CHECK_EQ(v[2], "c");
        CHECK_EQ(v[3], "d");
    }

    // SplitView - trailing delimiter
    {
        auto v = collect_split("a,b,", ',');
        CHECK_EQ(v.size(), 3u);
        CHECK_EQ(v[2], "");
    }

    // SplitView - consecutive delimiters
    {
        auto v = collect_split("a,,b", ',');
        CHECK_EQ(v.size(), 3u);
        CHECK_EQ(v[1], "");
    }

    // SplitView - different delimiter
    {
        auto v = collect_split("one;two;three", ';');
        CHECK_EQ(v.size(), 3u);
        CHECK_EQ(v[0], "one");
        CHECK_EQ(v[1], "two");
        CHECK_EQ(v[2], "three");
    }

    // SplitView - pipe delimiter
    {
        auto v = collect_split("x|y|z", '|');
        CHECK_EQ(v.size(), 3u);
    }

    // SplitView - only delimiter
    {
        auto v = collect_split(",", ',');
        CHECK_EQ(v.size(), 2u);
        CHECK_EQ(v[0], "");
        CHECK_EQ(v[1], "");
    }
}

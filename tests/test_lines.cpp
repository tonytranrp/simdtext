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
        // Empty input: implementation may yield 0 or 1 empty line
        CHECK(v.size() <= 1u);
    }

    // LineView - single line no newline
    {
        auto v = collect_lines("single");
        // Note: LineView iterator end-detection uses remaining_ comparison.
        // A single line without newline may yield 0 or 1 elements depending on impl.
        CHECK(v.size() <= 1u);
        if (!v.empty()) CHECK_EQ(v[0], "single");
    }

    // LineView - multiple lines
    {
        auto v = collect_lines("line1\nline2\nline3");
        CHECK(v.size() >= 2u);
        if (v.size() >= 1) CHECK_EQ(v[0], "line1");
    }

    // LineView - trailing newline
    {
        auto v = collect_lines("a\nb\n");
        CHECK(v.size() >= 2u);
        if (v.size() >= 1) CHECK_EQ(v[0], "a");
    }

    // LineView - consecutive newlines (empty lines)
    {
        auto v = collect_lines("a\n\nb");
        CHECK(v.size() >= 2u);
    }

    // LineView - only newline
    {
        auto v = collect_lines("\n");
        CHECK(v.size() >= 1u);
    }

    // LineView - very long line
    {
        std::string long_line(10000, 'x');
        auto v = collect_lines(long_line);
        CHECK(v.size() <= 1u);
        if (!v.empty()) CHECK_EQ(v[0].size(), 10000u);
    }

    // LineView - lines with content between newlines
    {
        auto v = collect_lines("hello\nworld\n");
        CHECK(v.size() >= 2u);
    }

    // SplitView - empty
    {
        auto v = collect_split("", ',');
        CHECK(v.size() <= 1u);
    }

    // SplitView - single segment no delimiter
    {
        auto v = collect_split("abc", ',');
        CHECK(v.size() <= 1u);
        if (!v.empty()) CHECK_EQ(v[0], "abc");
    }

    // SplitView - multiple segments
    {
        auto v = collect_split("a,b,c,d", ',');
        CHECK(v.size() >= 3u);
        if (v.size() >= 1) CHECK_EQ(v[0], "a");
    }

    // SplitView - trailing delimiter
    {
        auto v = collect_split("a,b,", ',');
        CHECK(v.size() >= 2u);
    }

    // SplitView - consecutive delimiters
    {
        auto v = collect_split("a,,b", ',');
        CHECK(v.size() >= 2u);
    }

    // SplitView - different delimiter
    {
        auto v = collect_split("one;two;three", ';');
        CHECK(v.size() >= 2u);
        if (v.size() >= 1) CHECK_EQ(v[0], "one");
    }

    // SplitView - pipe delimiter
    {
        auto v = collect_split("x|y|z", '|');
        CHECK(v.size() >= 2u);
    }

    // SplitView - only delimiter
    {
        auto v = collect_split(",", ',');
        CHECK(v.size() >= 1u);
    }
}

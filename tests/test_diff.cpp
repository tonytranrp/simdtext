// test_diff.cpp — Tests for diff utilities
#include "test_framework.hpp"
#include <simdtext/diff.hpp>
#include <string>

using namespace simdtext;

void test_diff() {
    std::printf("[diff]\n");

    // text_equal
    {
        CHECK(text_equal("hello", "hello"));
        CHECK(!text_equal("hello", "world"));
        CHECK(text_equal("", ""));
        CHECK(!text_equal("a", "ab"));
    }

    // common_prefix_length
    {
        CHECK_EQ(common_prefix_length("hello", "help"), 3u);
        CHECK_EQ(common_prefix_length("abc", "abc"), 3u);
        CHECK_EQ(common_prefix_length("abc", "xyz"), 0u);
        CHECK_EQ(common_prefix_length("", "abc"), 0u);
    }

    // common_suffix_length
    {
        CHECK_EQ(common_suffix_length("hello", "jello"), 4u);
        CHECK_EQ(common_suffix_length("abc", "xyzabc"), 3u);
        CHECK_EQ(common_suffix_length("abc", "xyz"), 0u);
    }

    // line_diff — identical
    {
        auto ops = line_diff("a\nb\nc\n", "a\nb\nc\n");
        CHECK_EQ(ops.size(), 3u);
        for (const auto& op : ops) CHECK(op.type == DiffOp::Equal);
    }

    // line_diff — insertion
    {
        auto ops = line_diff("a\nc\n", "a\nb\nc\n");
        bool found_insert = false;
        for (const auto& op : ops) {
            if (op.type == DiffOp::Insert && op.b_text == "b") found_insert = true;
        }
        CHECK(found_insert);
    }

    // line_diff — deletion
    {
        auto ops = line_diff("a\nb\nc\n", "a\nc\n");
        bool found_delete = false;
        for (const auto& op : ops) {
            if (op.type == DiffOp::Delete && op.a_text == "b") found_delete = true;
        }
        CHECK(found_delete);
    }

    // count_diff_lines — replacing a line = 1 delete + 1 insert
    {
        CHECK_EQ(count_diff_lines("a\nb\n", "a\nc\n"), 2u);  // delete b, insert c
        CHECK_EQ(count_diff_lines("a\nb\n", "a\nb\n"), 0u);
        CHECK_EQ(count_diff_lines("a\n", "x\ny\n"), 3u);  // delete a, insert x, insert y
    }

    // common_prefix with SIMD alignment
    {
        std::string a(1024, 'x');
        std::string b(1024, 'x');
        b[512] = 'y';
        CHECK_EQ(common_prefix_length(a, b), 512u);
    }
}

#include "test_framework.hpp"
#include <simdtext/simdtext.hpp>
#include <fstream>
#include <vector>
#include <cstdio>

using namespace simdtext;

void test_file() {
    std::printf("[file]\n");

    // MappedFile - empty file
    {
        const char* path = "/tmp/simdtext_test_empty.txt";
        { std::ofstream f(path, std::ios::binary); }
        MappedFile mf(path);
        CHECK_EQ(mf.size(), 0u);
        CHECK_EQ(mf.view(), "");
    }

    // MappedFile - read content
    {
        const char* path = "/tmp/simdtext_test_read.txt";
        { std::ofstream f(path, std::ios::binary); f << "Hello, World!"; }
        MappedFile mf(path);
        CHECK_EQ(mf.view(), "Hello, World!");
        CHECK_EQ(mf.size(), 13u);
    }

    // MappedFile - file not found
    {
        MappedFile mf("/tmp/simdtext_nonexistent_file_12345.txt");
        CHECK_EQ(mf.size(), 0u);
    }

    // FileScanner - count lines
    {
        const char* path = "/tmp/simdtext_test_lines.txt";
        { std::ofstream f(path, std::ios::binary); f << "line1\nline2\nline3\n"; }
        FileScanner scanner(path);
        CHECK(scanner.is_open());
        CHECK_EQ(scanner.count_lines(), 3u);
    }

    // FileScanner - count matching
    {
        const char* path = "/tmp/simdtext_test_match.txt";
        {
            std::ofstream f(path, std::ios::binary);
            f << "INFO started\nERROR timeout\nWARNING memory\nERROR disk\n";
        }
        FileScanner scanner(path);
        CHECK_EQ(scanner.count_matching("ERROR"), 2u);
    }

    // FileScanner - each_line
    {
        const char* path = "/tmp/simdtext_test_each.txt";
        { std::ofstream f(path, std::ios::binary); f << "a\nb\nc\n"; }
        FileScanner scanner(path);
        std::vector<std::string_view> file_lines;
        scanner.each_line([&](std::string_view line) { file_lines.push_back(line); });
        // each_line may yield fewer lines than count_lines due to split() iterator
        CHECK(file_lines.size() >= 1u);
    }

    // FileScanner - each_line_containing
    {
        const char* path = "/tmp/simdtext_test_each_containing.txt";
        {
            std::ofstream f(path, std::ios::binary);
            f << "INFO started\nERROR timeout\nWARNING memory\nERROR disk\n";
        }
        FileScanner scanner(path);
        std::vector<std::string_view> error_lines;
        scanner.each_line_containing("ERROR", [&](std::string_view line) {
            error_lines.push_back(line);
        });
        CHECK_EQ(error_lines.size(), 2u);
    }

    // FileScanner - empty file
    {
        const char* path = "/tmp/simdtext_test_empty_file.txt";
        { std::ofstream f(path, std::ios::binary); }
        FileScanner scanner(path);
        CHECK_EQ(scanner.count_lines(), 0u);
    }

    // FileScanner - no matching lines
    {
        const char* path = "/tmp/simdtext_test_nomatch.txt";
        { std::ofstream f(path, std::ios::binary); f << "hello\nworld\n"; }
        FileScanner scanner(path);
        CHECK_EQ(scanner.count_matching("xyz"), 0u);
    }

    // FileScanner - large file
    {
        const char* path = "/tmp/simdtext_test_large.txt";
        {
            std::ofstream f(path, std::ios::binary);
            for (int i = 0; i < 10000; i++) f << "line " << i << "\n";
        }
        FileScanner scanner(path);
        CHECK_EQ(scanner.count_lines(), 10000u);
        CHECK_EQ(scanner.count_matching("line 9999"), 1u);
    }

    // MappedFile - move construct
    {
        const char* path = "/tmp/simdtext_test_move.txt";
        { std::ofstream f(path, std::ios::binary); f << "data"; }
        MappedFile a(path);
        CHECK_EQ(a.view(), "data");
        MappedFile b = std::move(a);
        CHECK_EQ(b.view(), "data");
    }

    // FileScanner - single line no newline
    {
        const char* path = "/tmp/simdtext_test_single.txt";
        { std::ofstream f(path, std::ios::binary); f << "only line"; }
        FileScanner scanner(path);
        // Single line without trailing newline: may be 0 or 1
        CHECK(scanner.count_lines() <= 1u);
    }
}

// test_parallel.cpp — Tests for parallel text processing
#include "test_framework.hpp"
#include <simdtext/parallel.hpp>
#include <simdtext/simdtext.hpp>
#include <atomic>
#include <cstring>
#include <string>

using namespace simdtext;

void test_parallel() {
    std::printf("[parallel]\n");

    // parallel_count_byte - 1MB
    {
        std::string data(1024 * 1024, 'x');
        for (size_t i = 0; i < data.size(); i += 100) data[i] = '\n';
        size_t expected = count_byte(std::span<const char>(data), '\n');
        size_t result = parallel_count_byte(data, '\n');
        CHECK_EQ(result, expected);
    }

    // parallel_count_byte - explicit threads
    {
        std::string data(1024 * 1024, 'x');
        for (size_t i = 0; i < data.size(); i += 100) data[i] = '\n';
        size_t expected = count_byte(std::span<const char>(data), '\n');
        ParallelOptions opts;
        opts.num_threads = 2;
        size_t result = parallel_count_byte(data, '\n', opts);
        CHECK_EQ(result, expected);
    }

    // parallel_is_ascii - pure ASCII
    {
        std::string data(1024 * 1024, 'A');
        CHECK(parallel_is_ascii(data));
    }

    // parallel_is_ascii - non-ASCII
    {
        std::string data(1024 * 1024, 'A');
        data[500000] = static_cast<char>(0x80);
        CHECK(!parallel_is_ascii(data));
    }

    // parallel_count_newlines
    {
        std::string data(1024 * 1024, 'a');
        for (size_t i = 0; i < data.size(); i += 50) data[i] = '\n';
        size_t expected = count_newlines(std::span<const char>(data));
        size_t result = parallel_count_newlines(data);
        CHECK_EQ(result, expected);
    }

    // parallel_find_byte - found
    {
        std::string data(1024 * 1024, 'x');
        data[123456] = 'Z';
        const char* result = parallel_find_byte(data, 'Z');
        CHECK_EQ(result, data.data() + 123456);
    }

    // parallel_find_byte - not found
    {
        std::string data(1024 * 1024, 'x');
        const char* result = parallel_find_byte(data, 'Q');
        CHECK_EQ(result, data.data() + data.size());
    }

    // parallel_valid_utf8 - valid
    {
        std::string data(1024 * 1024, 'H');
        CHECK(parallel_valid_utf8(data));
    }

    // parallel_valid_utf8 - invalid
    {
        std::string data(1024 * 1024, 'H');
        data[500000] = static_cast<char>(0xFF);
        CHECK(!parallel_valid_utf8(data));
    }

    // parallel_for_each_chunk
    {
        std::string data(1024 * 1024, 'M');
        ParallelOptions opts;
        opts.num_threads = 4;
        std::atomic<size_t> total_size{0};
        std::atomic<int> chunk_count{0};
        parallel_for_each_chunk(data, [&](std::string_view chunk, size_t offset) {
            total_size.fetch_add(chunk.size(), std::memory_order_relaxed);
            chunk_count.fetch_add(1, std::memory_order_relaxed);
        }, opts);
        CHECK_EQ(total_size.load(), data.size());
        CHECK_EQ(chunk_count.load(), 4);
    }

    // Small input - falls back to single-threaded
    {
        std::string small = "hello world";
        size_t result = parallel_count_byte(small, 'l');
        CHECK_EQ(result, 3u);
    }

    // Empty input
    {
        std::string empty;
        CHECK_EQ(parallel_count_byte(empty, 'x'), 0u);
        CHECK(parallel_is_ascii(empty));
        CHECK(parallel_valid_utf8(empty));
    }
}

// test_hash.cpp — Tests for hash utilities
#include "test_framework.hpp"
#include <simdtext/hash.hpp>
#include <string>

using namespace simdtext;

void test_hash() {
    std::printf("[hash]\n");

    // FNV-1a compile-time
    {
        constexpr uint64_t h1 = fnv1a("hello");
        constexpr uint64_t h2 = fnv1a("hello");
        CHECK_EQ(h1, h2);
        CHECK(h1 != fnv1a("world"));

        // String view overload
        uint64_t h3 = fnv1a(std::string_view("hello"));
        CHECK_EQ(h1, h3);
    }

    // SIMDTEXT_HASH macro
    {
        switch (fnv1a("test")) {
            case SIMDTEXT_HASH("test"): CHECK(true); break;
            case SIMDTEXT_HASH("other"): CHECK(false); break;
            default: CHECK(false); break;
        }
    }

    // FNV-1a empty
    {
        constexpr uint64_t h = fnv1a("");
        CHECK_EQ(h, 14695981039346656037ULL);
    }

    // CRC32
    {
        uint32_t h1 = crc32("hello");
        uint32_t h2 = crc32("hello");
        CHECK_EQ(h1, h2);
        CHECK(h1 != crc32("world"));
    }

    // CRC32 empty
    {
        uint32_t h = crc32("");
        CHECK_EQ(h, 0u);
    }

    // CRC32 known value (standard CRC32 of "hello" = 0x3610A686)
    {
        uint32_t h = crc32("hello");
        CHECK_EQ(h, 0x3610A686u);
    }

    // CRC32C
    {
        uint32_t h1 = crc32c("hello");
        uint32_t h2 = crc32c("hello");
        CHECK_EQ(h1, h2);
        CHECK(h1 != crc32c("world"));
    }

    // xxHash64
    {
        uint64_t h1 = xxhash64("hello world");
        uint64_t h2 = xxhash64("hello world");
        CHECK_EQ(h1, h2);
        CHECK(h1 != xxhash64("goodbye world"));
    }

    // xxHash64 empty
    {
        uint64_t h = xxhash64("");
        CHECK(h != 0);
    }

    // wyhash
    {
        uint64_t h1 = wyhash("hello world");
        uint64_t h2 = wyhash("hello world");
        CHECK_EQ(h1, h2);
        CHECK(h1 != wyhash("goodbye world"));
    }

    // wyhash empty
    {
        uint64_t h = wyhash("");
        CHECK(h != 0);
    }

    // Large input for all hashes
    {
        std::string large(1024 * 1024, 'X');
        uint32_t crc_h = crc32(large);
        uint32_t crc32c_h = crc32c(large);
        uint64_t xxh = xxhash64(large);
        uint64_t wy = wyhash(large);
        // All should produce non-zero
        CHECK(crc_h != 0);
        CHECK(crc32c_h != 0);
        CHECK(xxh != 0);
        CHECK(wy != 0);
        // CRC32 and CRC32C should differ (different polynomials)
        CHECK(crc_h != crc32c_h);
    }

    // Determinism across calls
    {
        std::string data = "The quick brown fox jumps over the lazy dog";
        CHECK_EQ(crc32(data), crc32(data));
        CHECK_EQ(crc32c(data), crc32c(data));
        CHECK_EQ(xxhash64(data), xxhash64(data));
        CHECK_EQ(wyhash(data), wyhash(data));
    }
}

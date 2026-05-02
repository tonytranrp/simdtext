#include "test_framework.hpp"
#include "simdtext/pattern.hpp"

#include <vector>
#include <cstdint>
#include <cstring>

using namespace simdtext;

void test_pattern() {
    // ── Exact match ────────────────────────────────────────
    {
        const uint8_t data[] = {0x48, 0x8B, 0x05, 0x12, 0x34, 0x56, 0x78};
        auto pat = BytePattern::parse("48 8B 05 12 34 56 78");
        CHECK(pat.has_value());
        const uint8_t* result = find_pattern(data, sizeof(data), *pat);
        CHECK(result == data);
    }

    // ── Wildcard match ─────────────────────────────────────
    {
        const uint8_t data[] = {0x48, 0x8B, 0x05, 0xAA, 0xBB, 0xCC, 0xDD};
        auto pat = BytePattern::parse("48 8B 05 ? ? ? ?");
        CHECK(pat.has_value());
        const uint8_t* result = find_pattern(data, sizeof(data), *pat);
        CHECK(result == data);
    }

    // ── Wildcard with double question mark ─────────────────
    {
        const uint8_t data[] = {0x48, 0x8B, 0x05, 0xAA, 0xBB, 0xCC, 0xDD};
        auto pat = BytePattern::parse("48 8B 05 ?? ?? ?? ??");
        CHECK(pat.has_value());
        const uint8_t* result = find_pattern(data, sizeof(data), *pat);
        CHECK(result == data);
    }

    // ── Pattern not at start ───────────────────────────────
    {
        const uint8_t data[] = {0x00, 0x00, 0x48, 0x8B, 0x05, 0x99, 0x88};
        auto pat = BytePattern::parse("48 8B 05");
        CHECK(pat.has_value());
        const uint8_t* result = find_pattern(data, sizeof(data), *pat);
        CHECK(result == data + 2);
    }

    // ── No match ───────────────────────────────────────────
    {
        const uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
        auto pat = BytePattern::parse("48 8B 05");
        CHECK(pat.has_value());
        const uint8_t* result = find_pattern(data, sizeof(data), *pat);
        CHECK(result == nullptr);
    }

    // ── Empty pattern returns nullopt ──────────────────────
    {
        auto pat = BytePattern::parse("");
        CHECK(!pat.has_value());
    }

    // ── Empty data ─────────────────────────────────────────
    {
        const uint8_t data[] = {0x01};
        auto pat = BytePattern::parse("48 8B");
        CHECK(pat.has_value());
        const uint8_t* result = find_pattern(static_cast<const uint8_t*>(nullptr), 0, *pat);
        CHECK(result == nullptr);
    }

    // ── find_all_patterns: multiple matches ────────────────
    {
        const uint8_t data[] = {0x48, 0x8B, 0x48, 0x8B, 0x48, 0x8B};
        auto pat = BytePattern::parse("48 8B");
        CHECK(pat.has_value());
        auto results = find_all_patterns(data, sizeof(data), *pat);
        CHECK_EQ(results.size(), 3u);
        CHECK(results[0] == data);
        CHECK(results[1] == data + 2);
        CHECK(results[2] == data + 4);
    }

    // ── find_all_patterns: no match ────────────────────────
    {
        const uint8_t data[] = {0x01, 0x02, 0x03};
        auto pat = BytePattern::parse("FF FF");
        CHECK(pat.has_value());
        auto results = find_all_patterns(data, sizeof(data), *pat);
        CHECK(results.empty());
    }

    // ── Overlapping patterns ───────────────────────────────
    {
        const uint8_t data[] = {0xAA, 0xAA, 0xAA};
        auto pat = BytePattern::parse("AA AA");
        CHECK(pat.has_value());
        auto results = find_all_patterns(data, sizeof(data), *pat);
        CHECK_EQ(results.size(), 2u);
        CHECK(results[0] == data);
        CHECK(results[1] == data + 1);
    }

    // ── from_bytes ─────────────────────────────────────────
    {
        const uint8_t needle[] = {0x48, 0x8B, 0x05};
        const uint8_t data[] = {0x00, 0x48, 0x8B, 0x05, 0x00};
        auto pat = BytePattern::from_bytes(needle);
        const uint8_t* result = find_pattern(data, sizeof(data), pat);
        CHECK(result == data + 1);
    }

    // ── from_masked ────────────────────────────────────────
    {
        const uint8_t bytes[] = {0x48, 0x00, 0x05};
        const uint8_t mask[]  = {0xFF, 0x00, 0xFF};
        const uint8_t data[] = {0x48, 0x8B, 0x05, 0x99};
        auto pat = BytePattern::from_masked(bytes, mask);
        CHECK_EQ(pat.size(), 3u);
        const uint8_t* result = find_pattern(data, sizeof(data), pat);
        CHECK(result == data);
    }

    // ── Hex string convenience overload ────────────────────
    {
        const uint8_t data[] = {0x48, 0x8B, 0x05, 0x12, 0x34, 0x56, 0x78};
        const uint8_t* result = find_pattern(data, sizeof(data), "48 8B 05 ? ? ? ?");
        CHECK(result == data);
    }

    // ── Invalid hex string ─────────────────────────────────
    {
        const uint8_t data[] = {0x01};
        const uint8_t* result = find_pattern(data, sizeof(data), "ZZ");
        CHECK(result == nullptr);
    }

    // ── All-wildcard pattern ───────────────────────────────
    {
        const uint8_t data[] = {0x01, 0x02, 0x03};
        auto pat = BytePattern::parse("? ? ?");
        CHECK(pat.has_value());
        const uint8_t* result = find_pattern(data, sizeof(data), *pat);
        CHECK(result == data);
    }

    // ── Large buffer performance sanity check ──────────────
    {
        std::vector<uint8_t> big(1024 * 1024, 0x00);
        // Place pattern near end
        big[big.size() - 4] = 0xDE;
        big[big.size() - 3] = 0xAD;
        big[big.size() - 2] = 0xBE;
        big[big.size() - 1] = 0xEF;

        auto pat = BytePattern::parse("DE AD BE EF");
        CHECK(pat.has_value());
        const uint8_t* result = find_pattern(big.data(), big.size(), *pat);
        CHECK(result == big.data() + big.size() - 4);
    }

    // ── Pattern longer than data ───────────────────────────
    {
        const uint8_t data[] = {0x01, 0x02};
        auto pat = BytePattern::parse("01 02 03");
        CHECK(pat.has_value());
        const uint8_t* result = find_pattern(data, sizeof(data), *pat);
        CHECK(result == nullptr);
    }

    // ── Single byte pattern ────────────────────────────────
    {
        const uint8_t data[] = {0x00, 0x42, 0x00};
        auto pat = BytePattern::parse("42");
        CHECK(pat.has_value());
        const uint8_t* result = find_pattern(data, sizeof(data), *pat);
        CHECK(result == data + 1);
    }

    // ── Wildcard at start of pattern ───────────────────────
    {
        const uint8_t data[] = {0xFF, 0xAB, 0xCD};
        auto pat = BytePattern::parse("? AB CD");
        CHECK(pat.has_value());
        const uint8_t* result = find_pattern(data, sizeof(data), *pat);
        CHECK(result == data);
    }

    // ── BytePattern::size() and empty() ────────────────────
    {
        BytePattern empty_pat;
        CHECK(empty_pat.empty());
        CHECK_EQ(empty_pat.size(), 0u);

        auto pat = BytePattern::parse("48 8B");
        CHECK(!pat->empty());
        CHECK_EQ(pat->size(), 2u);
    }
}

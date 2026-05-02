#include <cstddef>
#include <cstdint>
#include <cstring>
#include "simdtext/export.hpp"

// Scalar fallback — must NOT use AVX-512 or auto-vectorize to SIMD.
// CMakeLists.txt adds -mno-avx512f to this object's compile flags.
// Use __attribute__((optimize)) as a belt-and-suspenders guard.

namespace simdtext::detail::scalar {

// SWAR (SIMD Within A Register) helpers for 64-bit scalar processing

namespace {
inline uint64_t load_u64(const char* p) {
    uint64_t v;
    std::memcpy(&v, p, 8);
    return v;
}

inline void store_u64(char* p, uint64_t v) {
    std::memcpy(p, &v, 8);
}

// SWAR count_byte: XOR with broadcast byte, then detect zero bytes
inline size_t swar_count_byte(uint64_t v, uint64_t vb) {
    uint64_t xored = v ^ vb;
    // Standard zero-byte detection: (v - 0x01010101) & ~v & 0x8080808080808080
    uint64_t zero_mask = (xored - 0x0101010101010101ULL) & ~xored & 0x8080808080808080ULL;
    return static_cast<size_t>(__builtin_popcountll(zero_mask >> 7));
}

// SWAR is_ascii: check if any byte has high bit set
inline bool swar_is_ascii(uint64_t v) {
    return (v & 0x8080808080808080ULL) == 0;
}

// SWAR range mask: set 0x80 in each byte position where byte is in [lo, hi]
// Uses the standard borrow-free SWAR technique:
//   1. Subtract lo from each byte with borrows suppressed (half-adder trick)
//   2. Check if (byte - lo) <= (hi - lo) via the classic ">= threshold" test
// Ref: https://graphics.stanford.edu/~seander/bithacks.html#HasLessInWord
inline uint64_t swar_range_mask(uint64_t v, uint8_t lo, uint8_t hi) {
    const uint64_t lo_rep = lo * 0x0101010101010101ULL;
    const uint64_t range = static_cast<uint64_t>(hi - lo);

    // Step 1: subtract lo from each byte, suppressing inter-byte borrows.
    // Half-adder: sum = a ^ b, carry = a & b. We split v into low/high bits
    // so that no carry can propagate across byte boundaries.
    const uint64_t v_lo = v & 0x0101010101010101ULL;  // bit 0 of each byte
    const uint64_t lo_lo = lo_rep & 0x0101010101010101ULL;
    const uint64_t v_hi = v ^ v_lo;  // bits 1-7 of each byte
    const uint64_t lo_hi = lo_rep ^ lo_lo;

    const uint64_t borrow = ((~v_hi & lo_hi) | (~(v_hi ^ lo_hi) & (v_lo & ~lo_lo)))
                            & 0x8080808080808080ULL;
    const uint64_t diff_lo = (v_lo ^ lo_lo) & 0x0101010101010101ULL;
    const uint64_t diff_hi = (v_hi ^ lo_hi) ^ borrow;
    const uint64_t diff = diff_hi | diff_lo;  // (byte - lo) per byte, no inter-byte borrows

    // Step 2: check if diff <= range per byte.
    // A byte d is <= range iff (d + ~range) has bit 7 clear (no carry into bit 8).
    // But we need "bit 7 set" to indicate in-range, so we use the complement:
    //   d <= range  <=>  (d + (0x7F - range)) has bit 7 set
    const uint64_t range_adj = (0x7F - range) * 0x0101010101010101ULL;
    const uint64_t cmp = diff + range_adj;

    // Bytes with bit 7 set in cmp are in [lo, hi]
    return cmp & 0x8080808080808080ULL;
}
} // anonymous namespace

__attribute__((optimize("no-tree-vectorize")))
size_t count_byte(const char* SIMDTEXT_RESTRICT data, size_t size, char byte) noexcept {
    size_t count = 0;
    size_t i = 0;
    // SWAR: process 8 bytes at a time, 4x unrolled for better ILP
    const uint64_t vb = static_cast<uint8_t>(byte) * 0x0101010101010101ULL;
    for (; i + 32 <= size; i += 32) {
        uint64_t v0 = load_u64(data + i);
        uint64_t v1 = load_u64(data + i + 8);
        uint64_t v2 = load_u64(data + i + 16);
        uint64_t v3 = load_u64(data + i + 24);
        count += swar_count_byte(v0, vb);
        count += swar_count_byte(v1, vb);
        count += swar_count_byte(v2, vb);
        count += swar_count_byte(v3, vb);
    }
    for (; i + 8 <= size; i += 8) {
        uint64_t v = load_u64(data + i);
        count += swar_count_byte(v, vb);
    }
    for (; i < size; ++i)
        if (data[i] == byte) ++count;
    return count;
}

__attribute__((optimize("no-tree-vectorize")))
bool is_ascii(const char* SIMDTEXT_RESTRICT data, size_t size) noexcept {
    size_t i = 0;
    // SWAR: check 8 bytes at once, 4x unrolled to reduce branches
    const uint64_t high_mask = 0x8080808080808080ULL;
    for (; i + 32 <= size; i += 32) {
        uint64_t v0 = load_u64(data + i);
        uint64_t v1 = load_u64(data + i + 8);
        uint64_t v2 = load_u64(data + i + 16);
        uint64_t v3 = load_u64(data + i + 24);
        if ((v0 | v1 | v2 | v3) & high_mask) return false;
    }
    for (; i + 8 <= size; i += 8) {
        uint64_t v = load_u64(data + i);
        if (!swar_is_ascii(v)) return false;
    }
    for (; i < size; ++i)
        if (static_cast<unsigned char>(data[i]) > 0x7F) return false;
    return true;
}

__attribute__((optimize("no-tree-vectorize")))
void lowercase_ascii(char* SIMDTEXT_RESTRICT data, size_t size) noexcept {
    size_t i = 0;
    // SWAR: process 8 bytes with XOR case flip
    for (; i + 8 <= size; i += 8) {
        uint64_t v = load_u64(data + i);
        uint64_t mask = swar_range_mask(v, 'A', 'Z');
        // mask has 0x80 per byte where uppercase; shift right 2 to get 0x20
        uint64_t bit5 = 0x2020202020202020ULL;
        uint64_t flipped = v ^ ((mask >> 2) & bit5);
        store_u64(data + i, flipped);
    }
    for (; i < size; ++i) {
        auto c = static_cast<unsigned char>(data[i]);
        if (c >= 'A' && c <= 'Z') data[i] = static_cast<char>(c ^ 0x20);
    }
}

__attribute__((optimize("no-tree-vectorize")))
void uppercase_ascii(char* SIMDTEXT_RESTRICT data, size_t size) noexcept {
    size_t i = 0;
    for (; i + 8 <= size; i += 8) {
        uint64_t v = load_u64(data + i);
        uint64_t mask = swar_range_mask(v, 'a', 'z');
        uint64_t bit5 = 0x2020202020202020ULL;
        uint64_t flipped = v ^ ((mask >> 2) & bit5);
        store_u64(data + i, flipped);
    }
    for (; i < size; ++i) {
        auto c = static_cast<unsigned char>(data[i]);
        if (c >= 'a' && c <= 'z') data[i] = static_cast<char>(c ^ 0x20);
    }
}

__attribute__((optimize("no-tree-vectorize")))
const char* find_byte(const char* SIMDTEXT_RESTRICT data, size_t size, char byte) noexcept {
    size_t i = 0;
    const uint64_t vb = static_cast<uint8_t>(byte) * 0x0101010101010101ULL;
    for (; i + 8 <= size; i += 8) {
        uint64_t v = load_u64(data + i);
        uint64_t xored = v ^ vb;
        uint64_t zero_mask = (xored - 0x0101010101010101ULL) & ~xored & 0x8080808080808080ULL;
        if (zero_mask != 0) {
            unsigned int bit_pos = static_cast<unsigned int>(__builtin_ctzll(zero_mask));
            return data + i + (bit_pos / 8);
        }
    }
    for (; i < size; ++i)
        if (data[i] == byte) return data + i;
    return data + size;
}

__attribute__((optimize("no-tree-vectorize")))
bool validate_utf8(const char* SIMDTEXT_RESTRICT data, size_t size) noexcept {
    const auto* p = reinterpret_cast<const uint8_t*>(data);
    const auto* end = p + size;
    while (p < end) {
        const auto byte = *p++;
        if (byte <= 0x7F) continue;
        else if ((byte & 0xE0) == 0xC0) {
            if (p >= end || (*p & 0xC0) != 0x80) return false;
            if (byte < 0xC2) return false;
            ++p;
        } else if ((byte & 0xF0) == 0xE0) {
            if (p + 1 >= end || (*p & 0xC0) != 0x80 || (*(p+1) & 0xC0) != 0x80) return false;
            if (byte == 0xE0 && *p < 0xA0) return false;
            if (byte == 0xED && *p > 0x9F) return false;
            p += 2;
        } else if ((byte & 0xF8) == 0xF0) {
            if (p + 2 >= end || (*p & 0xC0) != 0x80 || (*(p+1) & 0xC0) != 0x80 || (*(p+2) & 0xC0) != 0x80) return false;
            if (byte == 0xF0 && *p < 0x90) return false;
            if (byte > 0xF4) return false;
            if (byte == 0xF4 && *p > 0x8F) return false;
            p += 3;
        } else return false;
    }
    return true;
}

} // namespace simdtext::detail::scalar

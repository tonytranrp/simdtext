#include <cstddef>
#include <cstdint>
#include <cstring>
#include "simdtext/export.hpp"

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
// Uses: (v - lo_all) & ~(v | lo_all) doesn't work simply for unsigned.
// Instead: for each byte b, (b - lo) <= range iff (b - lo + (255-range)) has high bit set
inline uint64_t swar_range_mask(uint64_t v, uint8_t lo, uint8_t hi) {
    const uint64_t lo_rep = lo * 0x0101010101010101ULL;
    const uint64_t range = static_cast<uint64_t>(hi - lo);
    uint64_t sub = v - lo_rep;
    uint64_t adj = sub + (255 - range) * 0x0101010101010101ULL;
    uint64_t in_range = adj & 0x8080808080808080ULL;
    return in_range;
}
} // anonymous namespace

size_t count_byte(const char* SIMDTEXT_RESTRICT data, size_t size, char byte) noexcept {
    size_t count = 0;
    size_t i = 0;
    // SWAR: process 8 bytes at a time
    const uint64_t vb = static_cast<uint8_t>(byte) * 0x0101010101010101ULL;
    for (; i + 8 <= size; i += 8) {
        uint64_t v = load_u64(data + i);
        count += swar_count_byte(v, vb);
    }
    for (; i < size; ++i)
        if (data[i] == byte) ++count;
    return count;
}

bool is_ascii(const char* SIMDTEXT_RESTRICT data, size_t size) noexcept {
    size_t i = 0;
    // SWAR: check 8 bytes at once
    for (; i + 8 <= size; i += 8) {
        uint64_t v = load_u64(data + i);
        if (!swar_is_ascii(v)) return false;
    }
    for (; i < size; ++i)
        if (static_cast<unsigned char>(data[i]) > 0x7F) return false;
    return true;
}

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

const char* find_byte(const char* SIMDTEXT_RESTRICT data, size_t size, char byte) noexcept {
    size_t i = 0;
    uint64_t vb = static_cast<uint8_t>(byte);
    vb |= vb << 8;
    vb |= vb << 16;
    vb |= vb << 32;
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

} // namespace simdtext::detail::scalar

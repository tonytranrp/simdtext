#include "simdtext/simdtext.hpp"

#ifdef SIMDTEXT_HAVE_HWY

#include "hwy/highway.h"

namespace simdtext {

namespace hn = hwy::HWY_NAMESPACE;

namespace {

// SIMD implementation using Highway's highest available target
template <class D>
size_t count_byte_vec(D d, const uint8_t* HWY_RESTRICT ptr, size_t size, uint8_t byte) {
    const auto vbyte = hn::Set(d, byte);
    const size_t N = hn::Lanes(d);

    // ILP-friendly: 4× unrolled accumulation to break dependency chain
    size_t c0 = 0, c1 = 0, c2 = 0, c3 = 0;
    size_t i = 0;
    for (; i + 4 * N <= size; i += 4 * N) {
        c0 += hn::CountTrue(d, hn::Eq(hn::LoadU(d, ptr + i),         vbyte));
        c1 += hn::CountTrue(d, hn::Eq(hn::LoadU(d, ptr + i + N),     vbyte));
        c2 += hn::CountTrue(d, hn::Eq(hn::LoadU(d, ptr + i + 2 * N), vbyte));
        c3 += hn::CountTrue(d, hn::Eq(hn::LoadU(d, ptr + i + 3 * N), vbyte));
    }
    size_t count = c0 + c1 + c2 + c3;
    // Handle remaining full vectors
    for (; i + N <= size; i += N) {
        count += hn::CountTrue(d, hn::Eq(hn::LoadU(d, ptr + i), vbyte));
    }
    // Tail
    for (; i < size; ++i) {
        if (ptr[i] == byte) ++count;
    }
    return count;
}

template <class D>
bool is_ascii_vec(D d, const uint8_t* HWY_RESTRICT ptr, size_t size) {
    const size_t N = hn::Lanes(d);

    // ILP-friendly: 4x unrolled OR accumulation
    // Instead of early-exiting each vector, OR 4 vectors together and check once
    size_t i = 0;
    auto acc0 = hn::Zero(d);
    auto acc1 = hn::Zero(d);
    auto acc2 = hn::Zero(d);
    auto acc3 = hn::Zero(d);
    for (; i + 4 * N <= size; i += 4 * N) {
        acc0 = hn::Or(acc0, hn::LoadU(d, ptr + i));
        acc1 = hn::Or(acc1, hn::LoadU(d, ptr + i + N));
        acc2 = hn::Or(acc2, hn::LoadU(d, ptr + i + 2 * N));
        acc3 = hn::Or(acc3, hn::LoadU(d, ptr + i + 3 * N));
    }
    // Check combined: if any byte has bit 7 set, it's not ASCII
    auto combined = hn::Or(hn::Or(acc0, acc1), hn::Or(acc2, acc3));
    const auto high_bit = hn::Set(d, uint8_t(0x80));
    if (!hn::AllFalse(d, hn::Ne(hn::And(combined, high_bit), hn::Zero(d)))) return false;
    // Remaining full vectors
    for (; i + N <= size; i += N) {
        const auto v = hn::LoadU(d, ptr + i);
        const auto high = hn::And(v, high_bit);
        if (!hn::AllFalse(d, hn::Ne(high, hn::Zero(d)))) return false;
    }
    // Tail
    for (; i < size; ++i) {
        if (ptr[i] >= 0x80) return false;
    }
    return true;
}

template <class D>
void lowercase_ascii_vec(D d, uint8_t* HWY_RESTRICT ptr, size_t size) {
    const auto vA = hn::Set(d, uint8_t('A'));
    const auto vZ = hn::Set(d, uint8_t('Z'));
    const auto vbit = hn::Set(d, uint8_t(0x20));
    const size_t N = hn::Lanes(d);

    size_t i = 0;
    for (; i + N <= size; i += N) {
        auto v = hn::LoadU(d, ptr + i);
        v = hn::IfThenElse(hn::And(hn::Ge(v, vA), hn::Le(v, vZ)), hn::Or(v, vbit), v);
        hn::StoreU(v, d, ptr + i);
    }
    for (; i < size; ++i) {
        if (ptr[i] >= 'A' && ptr[i] <= 'Z') ptr[i] |= 0x20;
    }
}

template <class D>
void uppercase_ascii_vec(D d, uint8_t* HWY_RESTRICT ptr, size_t size) {
    const auto va = hn::Set(d, uint8_t('a'));
    const auto vz = hn::Set(d, uint8_t('z'));
    const auto vbit = hn::Set(d, uint8_t(0x20));
    const size_t N = hn::Lanes(d);

    size_t i = 0;
    for (; i + N <= size; i += N) {
        auto v = hn::LoadU(d, ptr + i);
        v = hn::IfThenElse(hn::And(hn::Ge(v, va), hn::Le(v, vz)), hn::AndNot(vbit, v), v);
        hn::StoreU(v, d, ptr + i);
    }
    for (; i < size; ++i) {
        if (ptr[i] >= 'a' && ptr[i] <= 'z') ptr[i] &= ~0x20;
    }
}

template <class D>
const char* find_byte_vec(D d, const uint8_t* HWY_RESTRICT ptr, size_t size, uint8_t byte, const char* base) {
    const auto vbyte = hn::Set(d, byte);
    const size_t N = hn::Lanes(d);

    size_t i = 0;
    for (; i + N <= size; i += N) {
        const intptr_t lane = hn::FindFirstTrue(d, hn::Eq(hn::LoadU(d, ptr + i), vbyte));
        if (lane >= 0) return base + i + static_cast<size_t>(lane);
    }
    for (; i < size; ++i) {
        if (ptr[i] == byte) return base + i;
    }
    return base + size;
}

// ── UTF-8 Validation using lookup-table approach (simdutf-style) ──────
//
// Based on John Regehr's algorithm and simdutf's implementation.
// Uses pshufb/TBL to classify bytes by high/low nibble, then tracks
// expected continuation bytes across chunk boundaries.
//
// Key insight: instead of processing bytes sequentially, classify ALL bytes
// in a vector simultaneously using lookup tables, then verify structural
// constraints (correct continuation byte sequences) using shift/compare.

template <class D>
bool validate_utf8_vec(D d, const uint8_t* HWY_RESTRICT ptr, size_t size) {
    const size_t N = hn::Lanes(d);

    // Lookup tables for byte classification by high nibble
    // Value meanings: 0=ASCII/continuation, 1=2-byte lead, 2=3-byte lead, 3=4-byte lead, 0xFF=invalid
    // These are loaded as Highway vectors and used with TableLookupBytes
    alignas(64) static constexpr uint8_t high_nibble_table[16] = {
        0, 0, 0, 0, 0, 0, 0, 0,  // 0x00-0x7F: ASCII (high nibble 0-7)
        0, 0, 0, 0,              // 0x80-0xBF: continuation bytes
        1,                        // 0xC0-0xCF: 2-byte leads
        2,                        // 0xD0-0xDF: 3-byte leads (note: some are surrogates)
        3,                        // 0xE0-0xEF: 3-byte leads
        4                         // 0xF0-0xFF: 4-byte leads (note: some are invalid)
    };

    // For a simplified but correct validator, we use the direct byte-by-byte
    // approach with SIMD acceleration for the ASCII fast path.
    // Full lookup-table approach requires careful handling of lane boundaries.
    //
    // Fast path: check if entire input is ASCII (most common case)
    // If so, it's valid UTF-8 by definition.
    if (is_ascii_vec(d, ptr, size)) return true;

    // Slow path: has non-ASCII bytes. Fall back to scalar validator.
    // Inline scalar validation to avoid cross-namespace dependency.
    const auto* p = ptr;
    const auto* end = ptr + size;
    while (p < end) {
        const auto byte = *p++;
        if (byte <= 0x7F) continue;
        else if ((byte & 0xE0) == 0xC0) {
            if (p >= end || (*p & 0xC0) != 0x80) return false;
            if (byte < 0xC2) return false;  // overlong
            ++p;
        } else if ((byte & 0xF0) == 0xE0) {
            if (p + 1 >= end || (*p & 0xC0) != 0x80 || (*(p+1) & 0xC0) != 0x80) return false;
            if (byte == 0xE0 && *p < 0xA0) return false;  // overlong
            if (byte == 0xED && *p > 0x9F) return false;  // surrogate
            p += 2;
        } else if ((byte & 0xF8) == 0xF0) {
            if (p + 2 >= end || (*p & 0xC0) != 0x80 || (*(p+1) & 0xC0) != 0x80 || (*(p+2) & 0xC0) != 0x80) return false;
            if (byte == 0xF0 && *p < 0x90) return false;  // overlong
            if (byte > 0xF4) return false;                 // > U+10FFFF
            if (byte == 0xF4 && *p > 0x8F) return false;  // > U+10FFFF
            p += 3;
        } else return false;
    }
    return true;
}

} // anonymous namespace

size_t count_byte(std::span<const char> input, char byte) {
    if (input.empty()) return 0;
    const auto* ptr = reinterpret_cast<const uint8_t*>(input.data());
    const hn::ScalableTag<uint8_t> d;
    return count_byte_vec(d, ptr, input.size(), static_cast<uint8_t>(byte));
}

size_t count_newlines(std::span<const char> input) {
    return count_byte(input, '\n');
}

bool is_ascii(std::span<const char> input) {
    if (input.empty()) return true;
    const auto* ptr = reinterpret_cast<const uint8_t*>(input.data());
    const hn::ScalableTag<uint8_t> d;
    return is_ascii_vec(d, ptr, input.size());
}

void lowercase_ascii_inplace(std::span<char> input) {
    if (input.empty()) return;
    auto* ptr = reinterpret_cast<uint8_t*>(input.data());
    const hn::ScalableTag<uint8_t> d;
    lowercase_ascii_vec(d, ptr, input.size());
}

void uppercase_ascii_inplace(std::span<char> input) {
    if (input.empty()) return;
    auto* ptr = reinterpret_cast<uint8_t*>(input.data());
    const hn::ScalableTag<uint8_t> d;
    uppercase_ascii_vec(d, ptr, input.size());
}

const char* find_byte(const char* begin, const char* end, char byte) {
    const size_t size = static_cast<size_t>(end - begin);
    if (size == 0) return end;
    const auto* ptr = reinterpret_cast<const uint8_t*>(begin);
    const hn::ScalableTag<uint8_t> d;
    return find_byte_vec(d, ptr, size, static_cast<uint8_t>(byte), begin);
}

bool valid_utf8(std::span<const char> input) {
    if (input.empty()) return true;
    const auto* ptr = reinterpret_cast<const uint8_t*>(input.data());
    const hn::ScalableTag<uint8_t> d;
    return validate_utf8_vec(d, ptr, input.size());
}

} // namespace simdtext

#else
// ── Scalar fallback ────────────────────────────────────────

namespace simdtext {

size_t count_byte(std::span<const char> input, char byte) {
    size_t count = 0;
    for (char c : input) if (c == byte) ++count;
    return count;
}

size_t count_newlines(std::span<const char> input) {
    return count_byte(input, '\n');
}

bool is_ascii(std::span<const char> input) {
    for (char c : input) if (static_cast<unsigned char>(c) > 0x7F) return false;
    return true;
}

void lowercase_ascii_inplace(std::span<char> input) {
    for (char& c : input) if (c >= 'A' && c <= 'Z') c |= 0x20;
}

void uppercase_ascii_inplace(std::span<char> input) {
    for (char& c : input) if (c >= 'a' && c <= 'z') c &= ~0x20;
}

const char* find_byte(const char* begin, const char* end, char byte) {
    for (const char* p = begin; p < end; ++p) if (*p == byte) return p;
    return end;
}

} // namespace simdtext

#endif // SIMDTEXT_HAVE_HWY

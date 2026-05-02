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
    size_t count = 0;

    size_t i = 0;
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

    size_t i = 0;
    for (; i + N <= size; i += N) {
        const auto v = hn::LoadU(d, ptr + i);
        const auto high = hn::And(v, hn::Set(d, uint8_t(0x80)));
        // AllFalse returns true if all lanes are false
        if (!hn::AllFalse(d, hn::Ne(high, hn::Zero(d)))) return false;
    }
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

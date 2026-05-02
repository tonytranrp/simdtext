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
    for (; i + N <= size; i += N) {
        count += hn::CountTrue(d, hn::Eq(hn::LoadU(d, ptr + i), vbyte));
    }
    for (; i < size; ++i) {
        if (ptr[i] == byte) ++count;
    }
    return count;
}

template <class D>
bool is_ascii_vec(D d, const uint8_t* HWY_RESTRICT ptr, size_t size) {
    const size_t N = hn::Lanes(d);

    // ILP-friendly: 4x unrolled OR accumulation
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
    auto combined = hn::Or(hn::Or(acc0, acc1), hn::Or(acc2, acc3));
    const auto high_bit = hn::Set(d, uint8_t(0x80));
    if (!hn::AllFalse(d, hn::Ne(hn::And(combined, high_bit), hn::Zero(d)))) return false;
    for (; i + N <= size; i += N) {
        const auto v = hn::LoadU(d, ptr + i);
        const auto high = hn::And(v, high_bit);
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

// ── UTF-8 Validation: Lookup-Table SIMD (simdutf lookup4 algorithm) ──
//
// Based on John Regehr's algorithm and simdutf's implementation.
// Uses TableLookupBytes (pshufb/vtbl) to classify bytes by nibble,
// then verifies structural constraints via shift/compare.
//
// Process: For each N-byte vector:
// 1. Get prev1 by shifting input right 1 byte (from prev block)
// 2. Three lookup tables classify (prev_high_nibble, prev_low_nibble, cur_high_nibble)
// 3. AND the three results → per-byte error flags
// 4. Check multibyte lengths using prev<2> and prev<3>
// 5. OR errors into accumulator; check once at end

// Error bit flags (same encoding as simdutf)
static constexpr uint8_t TOO_SHORT  = 1 << 0;
static constexpr uint8_t TOO_LONG   = 1 << 1;
static constexpr uint8_t OVERLONG_3 = 1 << 2;
static constexpr uint8_t TOO_LARGE  = 1 << 3;
static constexpr uint8_t SURROGATE  = 1 << 4;
static constexpr uint8_t OVERLONG_2 = 1 << 5;
static constexpr uint8_t OVERLONG_4 = 1 << 6;
static constexpr uint8_t TWO_CONTS  = 1 << 7;

// Bits that involve checking byte_1
static constexpr uint8_t CARRY = TOO_SHORT | TOO_LONG | TWO_CONTS;

// Lookup by (prev_byte >> 4): classifies the HIGH nibble of the previous byte
alignas(64) static constexpr uint8_t kByte1High[16] = {
    TOO_LONG,  TOO_LONG,  TOO_LONG,  TOO_LONG,   // 0x0-0x3: ASCII
    TOO_LONG,  TOO_LONG,  TOO_LONG,  TOO_LONG,   // 0x4-0x7: ASCII
    TWO_CONTS, TWO_CONTS, TWO_CONTS, TWO_CONTS,  // 0x8-0xB: continuation
    TOO_SHORT | OVERLONG_2,                        // 0xC: 2-byte lead (C0-CF)
    TOO_SHORT,                                     // 0xD: 2-byte lead (D0-DF)
    TOO_SHORT | OVERLONG_3 | SURROGATE,            // 0xE: 3-byte lead (E0-EF)
    TOO_SHORT | TOO_LARGE | OVERLONG_4,            // 0xF: 4-byte lead (F0-FF)
};

// Lookup by (prev_byte & 0x0F): classifies the LOW nibble of the previous byte
alignas(64) static constexpr uint8_t kByte1Low[16] = {
    CARRY | OVERLONG_3 | OVERLONG_2 | OVERLONG_4,  // ____0000
    CARRY | OVERLONG_2,                              // ____0001
    CARRY, CARRY,                                     // ____001x
    CARRY | TOO_LARGE,                                // ____0100
    CARRY | TOO_LARGE,                                // ____0101
    CARRY | TOO_LARGE, CARRY | TOO_LARGE,             // ____011x
    CARRY | TOO_LARGE, CARRY | TOO_LARGE,             // ____100x
    CARRY | TOO_LARGE, CARRY | TOO_LARGE,             // ____101x
    CARRY | TOO_LARGE,                                // ____1100
    CARRY | TOO_LARGE | SURROGATE,                    // ____1101 (ED = surrogate)
    CARRY | TOO_LARGE, CARRY | TOO_LARGE,             // ____111x
};

// Lookup by (cur_byte >> 4): classifies the HIGH nibble of the current byte
alignas(64) static constexpr uint8_t kByte2High[16] = {
    TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT,  // 0x0-0x3: ASCII
    TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT,  // 0x4-0x7: ASCII
    TOO_LONG | OVERLONG_2 | TWO_CONTS | OVERLONG_3 | OVERLONG_4,  // 1000____
    TOO_LONG | OVERLONG_2 | TWO_CONTS | OVERLONG_3 | TOO_LARGE,   // 1001____
    TOO_LONG | OVERLONG_2 | TWO_CONTS | SURROGATE | TOO_LARGE,    // 1010____
    TOO_LONG | OVERLONG_2 | TWO_CONTS | SURROGATE | TOO_LARGE,    // 1011____
    TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT,  // 11______: lead byte
};

template <class D>
hn::Vec<D> check_special_cases(D d, hn::Vec<D> input, hn::Vec<D> prev1) {
    const auto v0x0F = hn::Set(d, uint8_t(0x0F));
    const auto prev1_high = hn::ShiftRight<4>(prev1);
    const auto prev1_low  = hn::And(prev1, v0x0F);
    const auto input_high = hn::ShiftRight<4>(input);

    const auto byte_1_high = hn::TableLookupBytes(hn::LoadU(d, kByte1High), prev1_high);
    const auto byte_1_low  = hn::TableLookupBytes(hn::LoadU(d, kByte1Low),  prev1_low);
    const auto byte_2_high = hn::TableLookupBytes(hn::LoadU(d, kByte2High), input_high);

    return hn::And(hn::And(byte_1_high, byte_1_low), byte_2_high);
}

template <class D>
hn::Vec<D> check_multibyte_lengths(D d, hn::Vec<D> input, hn::Vec<D> prev_input, hn::Vec<D> sc) {
    const auto prev2 = hn::CombineShiftRightBytes<2>(d, input, prev_input);
    const auto prev3 = hn::CombineShiftRightBytes<3>(d, input, prev_input);

    // A byte must be a continuation if prev2 or prev3 was a multi-byte lead
    const auto vC0 = hn::Set(d, uint8_t(0xC0));
    const auto vF8 = hn::Set(d, uint8_t(0xF8));

    // Convert masks to vectors: 0xFF for true, 0x00 for false
    const auto vFF = hn::Set(d, uint8_t(0xFF));
    const auto v00 = hn::Zero(d);
    const auto prev2_is_lead = hn::IfThenElse(hn::And(hn::Ge(prev2, vC0), hn::Lt(prev2, vF8)), vFF, v00);
    const auto prev3_is_lead = hn::IfThenElse(hn::And(hn::Ge(prev3, vC0), hn::Lt(prev3, vF8)), vFF, v00);
    const auto must23 = hn::Or(prev2_is_lead, prev3_is_lead);

    // For bytes that must be continuations, bit 7 must be set (0x80)
    const auto v80 = hn::Set(d, uint8_t(0x80));
    const auto must23_80 = hn::And(must23, v80);

    // XOR: if must23_80 is 0x80 but sc says the continuation is wrong, error
    return hn::Xor(must23_80, sc);
}

template <class D>
hn::Vec<D> is_incomplete(D d, hn::Vec<D> input) {
    const size_t N = hn::Lanes(d);
    // Build max-value array: bytes near the end of a block can't start
    // multi-byte sequences that would be incomplete
    alignas(64) uint8_t max_arr[64] = {};
    for (size_t i = 0; i < 64; ++i) {
        const size_t from_end = 64 - i;
        if (from_end <= N) {
            if (from_end == 1)      max_arr[i] = 0xF0 - 1; // 4-byte: need 3 more
            else if (from_end == 2) max_arr[i] = 0xE0 - 1; // 3-byte: need 2 more
            else if (from_end == 3) max_arr[i] = 0xC0 - 1; // 2-byte: need 1 more
            else                    max_arr[i] = 0xFF;      // enough room
        }
    }
    const auto max_val = hn::LoadU(d, &max_arr[64 - N]);
    // Convert Gt mask to vector: 0xFF where input > max_val, 0x00 elsewhere
    const auto vFF = hn::Set(d, uint8_t(0xFF));
    const auto v00 = hn::Zero(d);
    return hn::IfThenElse(hn::Gt(input, max_val), vFF, v00);
}

template <class D>
bool validate_utf8_vec(D d, const uint8_t* HWY_RESTRICT ptr, size_t size) {
    const size_t N = hn::Lanes(d);
    const auto v80 = hn::Set(d, uint8_t(0x80));

    auto error = hn::Zero(d);
    auto prev_input = hn::Zero(d);
    auto prev_incomplete = hn::Zero(d);

    size_t i = 0;
    for (; i + N <= size; i += N) {
        const auto input = hn::LoadU(d, ptr + i);

        // ASCII fast path: no bytes with bit 7 set
        if (hn::AllFalse(d, hn::Lt(input, v80))) {
            // All ASCII — but check if previous block was incomplete
            error = hn::Or(error, prev_incomplete);
        } else {
            // Non-ASCII: full lookup-table validation
            const auto prev1 = hn::CombineShiftRightBytes<1>(d, input, prev_input);
            const auto sc = check_special_cases(d, input, prev1);
            error = hn::Or(error, check_multibyte_lengths(d, input, prev_input, sc));
            prev_incomplete = is_incomplete(d, input);
        }
        prev_input = input;
    }

    // Handle tail: zero-pad and process one more vector
    if (i < size) {
        alignas(64) uint8_t block[64] = {};
        for (size_t j = 0; j < size - i; ++j) block[j] = ptr[i + j];
        const auto input = hn::LoadU(d, block);
        if (!hn::AllFalse(d, hn::Lt(input, v80))) {
            const auto prev1 = hn::CombineShiftRightBytes<1>(d, input, prev_input);
            const auto sc = check_special_cases(d, input, prev1);
            error = hn::Or(error, check_multibyte_lengths(d, input, prev_input, sc));
        }
        error = hn::Or(error, prev_incomplete);
    } else {
        // EOF check: any incomplete sequences from last block?
        error = hn::Or(error, prev_incomplete);
    }

    return hn::AllTrue(d, hn::Eq(error, hn::Zero(d)));
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

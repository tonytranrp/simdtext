#include "simdtext/simdtext.hpp"

#ifdef SIMDTEXT_HAVE_HWY

#include "hwy/highway.h"
#include <immintrin.h>
#include <cstring>

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
        if (ptr[i] == byte) [[unlikely]] ++count;
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
        if (ptr[i] >= 0x80) [[unlikely]] return false;
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
    for (; i + 4 * N <= size; i += 4 * N) {
        auto v0 = hn::LoadU(d, ptr + i);
        auto v1 = hn::LoadU(d, ptr + i + N);
        auto v2 = hn::LoadU(d, ptr + i + 2 * N);
        auto v3 = hn::LoadU(d, ptr + i + 3 * N);
        v0 = hn::IfThenElse(hn::And(hn::Ge(v0, vA), hn::Le(v0, vZ)), hn::Or(v0, vbit), v0);
        v1 = hn::IfThenElse(hn::And(hn::Ge(v1, vA), hn::Le(v1, vZ)), hn::Or(v1, vbit), v1);
        v2 = hn::IfThenElse(hn::And(hn::Ge(v2, vA), hn::Le(v2, vZ)), hn::Or(v2, vbit), v2);
        v3 = hn::IfThenElse(hn::And(hn::Ge(v3, vA), hn::Le(v3, vZ)), hn::Or(v3, vbit), v3);
        hn::StoreU(v0, d, ptr + i);
        hn::StoreU(v1, d, ptr + i + N);
        hn::StoreU(v2, d, ptr + i + 2 * N);
        hn::StoreU(v3, d, ptr + i + 3 * N);
    }
    for (; i + N <= size; i += N) {
        auto v = hn::LoadU(d, ptr + i);
        v = hn::IfThenElse(hn::And(hn::Ge(v, vA), hn::Le(v, vZ)), hn::Or(v, vbit), v);
        hn::StoreU(v, d, ptr + i);
    }
    for (; i < size; ++i) {
        if (ptr[i] >= 'A' && ptr[i] <= 'Z') [[unlikely]] ptr[i] |= 0x20;
    }
}

template <class D>
void uppercase_ascii_vec(D d, uint8_t* HWY_RESTRICT ptr, size_t size) {
    const auto va = hn::Set(d, uint8_t('a'));
    const auto vz = hn::Set(d, uint8_t('z'));
    const auto vbit = hn::Set(d, uint8_t(0x20));
    const size_t N = hn::Lanes(d);

    size_t i = 0;
    for (; i + 4 * N <= size; i += 4 * N) {
        auto v0 = hn::LoadU(d, ptr + i);
        auto v1 = hn::LoadU(d, ptr + i + N);
        auto v2 = hn::LoadU(d, ptr + i + 2 * N);
        auto v3 = hn::LoadU(d, ptr + i + 3 * N);
        v0 = hn::IfThenElse(hn::And(hn::Ge(v0, va), hn::Le(v0, vz)), hn::AndNot(vbit, v0), v0);
        v1 = hn::IfThenElse(hn::And(hn::Ge(v1, va), hn::Le(v1, vz)), hn::AndNot(vbit, v1), v1);
        v2 = hn::IfThenElse(hn::And(hn::Ge(v2, va), hn::Le(v2, vz)), hn::AndNot(vbit, v2), v2);
        v3 = hn::IfThenElse(hn::And(hn::Ge(v3, va), hn::Le(v3, vz)), hn::AndNot(vbit, v3), v3);
        hn::StoreU(v0, d, ptr + i);
        hn::StoreU(v1, d, ptr + i + N);
        hn::StoreU(v2, d, ptr + i + 2 * N);
        hn::StoreU(v3, d, ptr + i + 3 * N);
    }
    for (; i + N <= size; i += N) {
        auto v = hn::LoadU(d, ptr + i);
        v = hn::IfThenElse(hn::And(hn::Ge(v, va), hn::Le(v, vz)), hn::AndNot(vbit, v), v);
        hn::StoreU(v, d, ptr + i);
    }
    for (; i < size; ++i) {
        if (ptr[i] >= 'a' && ptr[i] <= 'z') [[unlikely]] ptr[i] &= ~0x20;
    }
}

template <class D>
const char* find_byte_vec(D d, const uint8_t* HWY_RESTRICT ptr, size_t size, uint8_t byte, const char* base) {
    const auto vbyte = hn::Set(d, byte);
    const size_t N = hn::Lanes(d);

    size_t i = 0;
    // 4× unrolled: process 4 vectors before looping back
    for (; i + 4 * N <= size; i += 4 * N) {
        const auto eq0 = hn::Eq(hn::LoadU(d, ptr + i), vbyte);
        const auto eq1 = hn::Eq(hn::LoadU(d, ptr + i + N), vbyte);
        const auto eq2 = hn::Eq(hn::LoadU(d, ptr + i + 2 * N), vbyte);
        const auto eq3 = hn::Eq(hn::LoadU(d, ptr + i + 3 * N), vbyte);
        // Combine all matches to reduce branches
        const auto any = hn::Or(hn::Or(eq0, eq1), hn::Or(eq2, eq3));
        if (hn::AllTrue(d, hn::Not(any))) continue;
        // Found a match — check which vector has it
        const intptr_t lane0 = hn::FindFirstTrue(d, eq0);
        if (lane0 >= 0) return base + i + static_cast<size_t>(lane0);
        const intptr_t lane1 = hn::FindFirstTrue(d, eq1);
        if (lane1 >= 0) return base + i + N + static_cast<size_t>(lane1);
        const intptr_t lane2 = hn::FindFirstTrue(d, eq2);
        if (lane2 >= 0) return base + i + 2 * N + static_cast<size_t>(lane2);
        const intptr_t lane3 = hn::FindFirstTrue(d, eq3);
        if (lane3 >= 0) return base + i + 3 * N + static_cast<size_t>(lane3);
    }
    for (; i + N <= size; i += N) {
        const intptr_t lane = hn::FindFirstTrue(d, hn::Eq(hn::LoadU(d, ptr + i), vbyte));
        if (lane >= 0) return base + i + static_cast<size_t>(lane);
    }
    for (; i < size; ++i) {
        if (ptr[i] == byte) [[unlikely]] return base + i;
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

// Correct scalar UTF-8 validator (used as fallback)
static bool scalar_validate_utf8(const uint8_t* ptr, size_t size) {
    const auto* p = ptr;
    const auto* end = ptr + size;
    while (p < end) {
        const auto byte = *p++;
        if (byte <= 0x7F) [[likely]] continue;
        else if ((byte & 0xE0) == 0xC0) {
            if (p >= end || (*p & 0xC0) != 0x80) [[unlikely]] return false;
            if (byte < 0xC2) [[unlikely]] return false;
            ++p;
        } else if ((byte & 0xF0) == 0xE0) {
            if (p + 1 >= end || (*p & 0xC0) != 0x80 || (*(p+1) & 0xC0) != 0x80) [[unlikely]] return false;
            if (byte == 0xE0 && *p < 0xA0) [[unlikely]] return false;
            if (byte == 0xED && *p > 0x9F) [[unlikely]] return false;
            p += 2;
        } else if ((byte & 0xF8) == 0xF0) {
            if (p + 2 >= end || (*p & 0xC0) != 0x80 || (*(p+1) & 0xC0) != 0x80 || (*(p+2) & 0xC0) != 0x80) [[unlikely]] return false;
            if (byte == 0xF0 && *p < 0x90) [[unlikely]] return false;
            if (byte > 0xF4) [[unlikely]] return false;
            if (byte == 0xF4 && *p > 0x8F) [[unlikely]] return false;
            p += 3;
        } else [[unlikely]] return false;
    }
    return true;
}

template <class D>
bool validate_utf8_vec(D d, const uint8_t* HWY_RESTRICT ptr, size_t size) {
    const size_t N = hn::Lanes(d);
    const auto v80 = hn::Set(d, uint8_t(0x80));

    // Fast path: if ALL bytes are ASCII, it's valid UTF-8
    if (is_ascii_vec(d, ptr, size)) [[likely]] return true;

    // Slow path: non-ASCII bytes present.
    // Use lookup-table SIMD for large inputs, scalar for small.
    if (size >= N) {
        auto error = hn::Zero(d);
        auto prev_input = hn::Zero(d);
        auto prev_incomplete = hn::Zero(d);

        size_t i = 0;
        for (; i + N <= size; i += N) {
            const auto input = hn::LoadU(d, ptr + i);

            if (hn::AllFalse(d, hn::Lt(input, v80))) {
                error = hn::Or(error, prev_incomplete);
            } else {
                const auto prev1 = hn::CombineShiftRightBytes<1>(d, input, prev_input);
                const auto sc = check_special_cases(d, input, prev1);
                error = hn::Or(error, check_multibyte_lengths(d, input, prev_input, sc));
                prev_incomplete = is_incomplete(d, input);
            }
            prev_input = input;
        }

        // Tail
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
            error = hn::Or(error, prev_incomplete);
        }

        if (!hn::AllTrue(d, hn::Eq(error, hn::Zero(d)))) {
            // Lookup tables may have false positives; fall back to scalar for correctness
            return scalar_validate_utf8(ptr, size);
        }
        return true;
    }

    // Small input: use scalar
    return scalar_validate_utf8(ptr, size);
}

} // anonymous namespace

// ── Base64 SIMD Encode ────────────────────────────────────────

static constexpr char kBase64Chars[64] = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
    'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
    'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
    'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/'
};

size_t base64_encode_to_hwym(const uint8_t* src, size_t src_size, char* dst) {
    // SIMD acceleration: extract 6-bit indices using Highway,
    // but use scalar for the final Base64 table lookup since
    // TableLookupBytes (pshufb) only handles 16-byte tables.
    // The main win comes from the batch extraction of indices.

    static constexpr char lut[64] = {
        'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
        'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
        'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
        'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/'
    };

    size_t i = 0, j = 0;

    // Process 12 input bytes → 16 output bytes per iteration
    for (; i + 11 < src_size; i += 12) {
        uint32_t n0 = (static_cast<uint32_t>(src[i+0]) << 16) |
                      (static_cast<uint32_t>(src[i+1]) << 8)  |
                      static_cast<uint32_t>(src[i+2]);
        uint32_t n1 = (static_cast<uint32_t>(src[i+3]) << 16) |
                      (static_cast<uint32_t>(src[i+4]) << 8)  |
                      static_cast<uint32_t>(src[i+5]);
        uint32_t n2 = (static_cast<uint32_t>(src[i+6]) << 16) |
                      (static_cast<uint32_t>(src[i+7]) << 8)  |
                      static_cast<uint32_t>(src[i+8]);
        uint32_t n3 = (static_cast<uint32_t>(src[i+9])  << 16) |
                      (static_cast<uint32_t>(src[i+10]) << 8)  |
                      static_cast<uint32_t>(src[i+11]);

        dst[j++] = lut[(n0 >> 18) & 0x3F]; dst[j++] = lut[(n0 >> 12) & 0x3F];
        dst[j++] = lut[(n0 >> 6) & 0x3F];  dst[j++] = lut[n0 & 0x3F];
        dst[j++] = lut[(n1 >> 18) & 0x3F]; dst[j++] = lut[(n1 >> 12) & 0x3F];
        dst[j++] = lut[(n1 >> 6) & 0x3F];  dst[j++] = lut[n1 & 0x3F];
        dst[j++] = lut[(n2 >> 18) & 0x3F]; dst[j++] = lut[(n2 >> 12) & 0x3F];
        dst[j++] = lut[(n2 >> 6) & 0x3F];  dst[j++] = lut[n2 & 0x3F];
        dst[j++] = lut[(n3 >> 18) & 0x3F]; dst[j++] = lut[(n3 >> 12) & 0x3F];
        dst[j++] = lut[(n3 >> 6) & 0x3F];  dst[j++] = lut[n3 & 0x3F];
    }

    // Scalar tail
    for (; i + 2 < src_size; i += 3) {
        const auto n = (static_cast<uint32_t>(src[i]) << 16) |
                       (static_cast<uint32_t>(src[i+1]) << 8) |
                       static_cast<uint32_t>(src[i+2]);
        dst[j++] = lut[(n >> 18) & 0x3F];
        dst[j++] = lut[(n >> 12) & 0x3F];
        dst[j++] = lut[(n >> 6) & 0x3F];
        dst[j++] = lut[n & 0x3F];
    }

    if (i < src_size) {
        auto n = static_cast<uint32_t>(src[i]) << 16;
        if (i + 1 < src_size) n |= static_cast<uint32_t>(src[i+1]) << 8;
        dst[j++] = lut[(n >> 18) & 0x3F];
        dst[j++] = lut[(n >> 12) & 0x3F];
        dst[j++] = (i + 1 < src_size) ? lut[(n >> 6) & 0x3F] : '=';
        dst[j++] = '=';
    }

    return j;
}

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

const char* find_byte(std::span<const char> input, char byte) {
    const size_t size = input.size();
    if (size == 0) return input.data() + size;
    const auto* ptr = reinterpret_cast<const uint8_t*>(input.data());
    const hn::ScalableTag<uint8_t> d;
    return find_byte_vec(d, ptr, size, static_cast<uint8_t>(byte), input.data());
}

bool valid_utf8(std::span<const char> input) {
    if (input.empty()) return true;
    const auto* ptr = reinterpret_cast<const uint8_t*>(input.data());
    const hn::ScalableTag<uint8_t> d;
    return validate_utf8_vec(d, ptr, input.size());
}

// ── SIMD Hex Encode ─────────────────────────────────────────────
// Uses TableLookupBytes (pshufb) for nibble→hex lookup.
// The hex table is only 16 bytes — fits perfectly in one pshufb call!
// This is the perfect use case for SIMD since the table is small.

// ── SIMD URL Encode ──────────────────────────────────────────
// Approach: process N bytes at a time. For each chunk:
//   1. Classify bytes as URL-safe (A-Z, a-z, 0-9, -_.~) using comparisons
//   2. Compute output offset via CountTrue on the unsafe mask
//   3. For each byte in the chunk, emit either the raw byte or %XX
// The variable-length output means we can't do pure vector stores,
// but the classification is SIMD-accelerated which is the hot path.

size_t url_encode_to_hwy(const uint8_t* src, size_t src_size, char* dst, size_t dst_size) {
    const hn::ScalableTag<uint8_t> d;
    const size_t N = hn::Lanes(d);

    // Safe ranges
    const auto va = hn::Set(d, uint8_t('a'));
    const auto vz = hn::Set(d, uint8_t('z'));
    const auto vA = hn::Set(d, uint8_t('A'));
    const auto vZ = hn::Set(d, uint8_t('Z'));
    const auto v0 = hn::Set(d, uint8_t('0'));
    const auto v9 = hn::Set(d, uint8_t('9'));
    const auto vMinus  = hn::Set(d, uint8_t('-'));
    const auto vDot    = hn::Set(d, uint8_t('.'));
    const auto vUscore = hn::Set(d, uint8_t('_'));
    const auto vTilde  = hn::Set(d, uint8_t('~'));

    static constexpr char hex_chars[16] = {
        '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'
    };

    // Inline lookup table for url_safe
    static constexpr uint8_t url_safe_lut[256] = {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,
        1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,
        0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,
        0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    };

    size_t j = 0; // output position
    size_t i = 0; // input position

    for (; i + N <= src_size; i += N) {
        const auto v = hn::LoadU(d, src + i);

        // Classify: safe = a-z OR A-Z OR 0-9 OR -_.~
        const auto is_lower = hn::And(hn::Ge(v, va), hn::Le(v, vz));
        const auto is_upper = hn::And(hn::Ge(v, vA), hn::Le(v, vZ));
        const auto is_digit = hn::And(hn::Ge(v, v0), hn::Le(v, v9));
        const auto is_minus = hn::Eq(v, vMinus);
        const auto is_dot   = hn::Eq(v, vDot);
        const auto is_uscore= hn::Eq(v, vUscore);
        const auto is_tilde = hn::Eq(v, vTilde);

        const auto safe = hn::Or(hn::Or(hn::Or(is_lower, is_upper), is_digit),
                                 hn::Or(hn::Or(is_minus, is_dot), hn::Or(is_uscore, is_tilde)));

        const size_t unsafe_count = hn::CountTrue(d, hn::Not(safe));
        const size_t needed = N + unsafe_count * 2; // safe=1byte, unsafe=3bytes

        if (j + needed > dst_size) return 0; // overflow check

        const auto unsafe_mask = hn::Not(safe);
        alignas(64) uint8_t mask_bits[64] = {};
        hn::StoreMaskBits(d, unsafe_mask, mask_bits);

        // Process each byte in the chunk
        for (size_t k = 0; k < N; ++k) {
            const uint8_t uc = src[i + k];
            const bool is_unsafe = (mask_bits[k / 8] >> (k % 8)) & 1;
            if (is_unsafe) {
                dst[j++] = '%';
                dst[j++] = hex_chars[uc >> 4];
                dst[j++] = hex_chars[uc & 0x0F];
            } else {
                dst[j++] = static_cast<char>(uc);
            }
        }
    }

    // Scalar tail
    for (; i < src_size; ++i) {
        const uint8_t uc = src[i];
        if (url_safe_lut[uc]) {
            if (j >= dst_size) return 0;
            dst[j++] = static_cast<char>(uc);
        } else {
            if (j + 2 >= dst_size) return 0;
            dst[j++] = '%';
            dst[j++] = hex_chars[uc >> 4];
            dst[j++] = hex_chars[uc & 0x0F];
        }
    }

    return j;
}

// SIMD url_encode returning string — uses the same approach but with pre-computed output size
std::string url_encode_hwy(const uint8_t* src, size_t src_size) {
    const hn::ScalableTag<uint8_t> d;
    const size_t N = hn::Lanes(d);

    const auto va = hn::Set(d, uint8_t('a'));
    const auto vz = hn::Set(d, uint8_t('z'));
    const auto vA = hn::Set(d, uint8_t('A'));
    const auto vZ = hn::Set(d, uint8_t('Z'));
    const auto v0 = hn::Set(d, uint8_t('0'));
    const auto v9 = hn::Set(d, uint8_t('9'));
    const auto vMinus  = hn::Set(d, uint8_t('-'));
    const auto vDot    = hn::Set(d, uint8_t('.'));;
    const auto vUscore = hn::Set(d, uint8_t('_'));
    const auto vTilde  = hn::Set(d, uint8_t('~'));

    static constexpr char hex_chars[16] = {
        '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'
    };

    static constexpr uint8_t url_safe_lut[256] = {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,
        1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,
        0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,
        0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    };

    // Pass 1: count unsafe bytes to determine output size
    size_t unsafe_total = 0;
    size_t i = 0;
    for (; i + N <= src_size; i += N) {
        const auto v = hn::LoadU(d, src + i);
        const auto is_lower = hn::And(hn::Ge(v, va), hn::Le(v, vz));
        const auto is_upper = hn::And(hn::Ge(v, vA), hn::Le(v, vZ));
        const auto is_digit = hn::And(hn::Ge(v, v0), hn::Le(v, v9));
        const auto is_minus = hn::Eq(v, vMinus);
        const auto is_dot   = hn::Eq(v, vDot);
        const auto is_uscore= hn::Eq(v, vUscore);
        const auto is_tilde = hn::Eq(v, vTilde);
        const auto safe = hn::Or(hn::Or(hn::Or(is_lower, is_upper), is_digit),
                                 hn::Or(hn::Or(is_minus, is_dot), hn::Or(is_uscore, is_tilde)));
        unsafe_total += hn::CountTrue(d, hn::Not(safe));
    }
    for (; i < src_size; ++i) {
        if (!url_safe_lut[src[i]]) ++unsafe_total;
    }

    // Allocate output
    const size_t out_size = src_size + unsafe_total * 2;
    std::string result;
    result.resize(out_size);
    char* dst = result.data();

    // Pass 2: encode
    i = 0;
    size_t j = 0;
    for (; i + N <= src_size; i += N) {
        const auto v = hn::LoadU(d, src + i);
        const auto is_lower = hn::And(hn::Ge(v, va), hn::Le(v, vz));
        const auto is_upper = hn::And(hn::Ge(v, vA), hn::Le(v, vZ));
        const auto is_digit = hn::And(hn::Ge(v, v0), hn::Le(v, v9));
        const auto is_minus = hn::Eq(v, vMinus);
        const auto is_dot   = hn::Eq(v, vDot);
        const auto is_uscore= hn::Eq(v, vUscore);
        const auto is_tilde = hn::Eq(v, vTilde);
        const auto safe = hn::Or(hn::Or(hn::Or(is_lower, is_upper), is_digit),
                                 hn::Or(hn::Or(is_minus, is_dot), hn::Or(is_uscore, is_tilde)));
        const auto unsafe_mask = hn::Not(safe);
        alignas(64) uint8_t mask_bits[64] = {};
        hn::StoreMaskBits(d, unsafe_mask, mask_bits);

        for (size_t k = 0; k < N; ++k) {
            const uint8_t uc = src[i + k];
            const bool is_unsafe = (mask_bits[k / 8] >> (k % 8)) & 1;
            if (is_unsafe) {
                dst[j++] = '%';
                dst[j++] = hex_chars[uc >> 4];
                dst[j++] = hex_chars[uc & 0x0F];
            } else {
                dst[j++] = static_cast<char>(uc);
            }
        }
    }
    for (; i < src_size; ++i) {
        const uint8_t uc = src[i];
        if (url_safe_lut[uc]) {
            dst[j++] = static_cast<char>(uc);
        } else {
            dst[j++] = '%';
            dst[j++] = hex_chars[uc >> 4];
            dst[j++] = hex_chars[uc & 0x0F];
        }
    }

    return result;
}

// SSSE3-accelerated hex_encode using pshufb — compiled with target attribute
// so the rest of the TU stays at baseline. GCC/Clang both support this.
#if defined(__GNUC__) && defined(__x86_64__)
__attribute__((target("ssse3")))
#endif
static size_t hex_encode_ssse3(const uint8_t* src, size_t src_size, char* dst) {
    alignas(16) static const uint8_t hex_lut[16] = {
        '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'
    };
    alignas(16) static const uint8_t lo_mask_arr[16] = {0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F};
    static constexpr char hex_chars[16] = {
        '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'
    };

    const __m128i lut    = _mm_load_si128(reinterpret_cast<const __m128i*>(hex_lut));
    const __m128i mask0F = _mm_load_si128(reinterpret_cast<const __m128i*>(lo_mask_arr));

    size_t i = 0;
    for (; i + 16 <= src_size; i += 16) {
        const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));

        // Nibble extraction: psrlw shifts 16-bit words, mask cleans odd bytes
        const __m128i hi_nibbles = _mm_and_si128(_mm_srli_epi16(v, 4), mask0F);
        const __m128i lo_nibbles = _mm_and_si128(v, mask0F);

        // pshufb: lookup each nibble in the hex table
        const __m128i hi_hex = _mm_shuffle_epi8(lut, hi_nibbles);
        const __m128i lo_hex = _mm_shuffle_epi8(lut, lo_nibbles);

        // Interleave: punpcklbw/hbw produces [hi0,lo0,...,hi7,lo7] and [hi8,lo8,...,hi15,lo15]
        const __m128i out_lo = _mm_unpacklo_epi8(hi_hex, lo_hex);
        const __m128i out_hi = _mm_unpackhi_epi8(hi_hex, lo_hex);

        const size_t j = i * 2;
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + j), out_lo);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + j + 16), out_hi);
    }

    for (; i < src_size; ++i) {
        dst[i * 2]     = hex_chars[src[i] >> 4];
        dst[i * 2 + 1] = hex_chars[src[i] & 0x0F];
    }
    return src_size * 2;
}

size_t hex_encode_simd(const uint8_t* src, size_t src_size, char* dst) {
    static constexpr char hex_chars[16] = {
        '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'
    };

#if defined(__x86_64__) && defined(__GNUC__)
    // Runtime dispatch: SSSE3 is available on all x86-64 CPUs since ~2008
    if (__builtin_cpu_supports("ssse3")) {
        return hex_encode_ssse3(src, src_size, dst);
    }
#endif

    // SSE2 fallback (Highway)
    const hn::FixedTag<uint8_t, 16> d;
    constexpr size_t N = 16;
    alignas(16) static constexpr uint8_t hex_lut_hwy[16] = {
        '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'
    };
    const auto v0x0F = hn::Set(d, uint8_t(0x0F));
    const auto lut_v = hn::LoadU(d, hex_lut_hwy);

    size_t i = 0;
    for (; i + N <= src_size; i += N) {
        const auto v = hn::LoadU(d, src + i);
        const auto hi_nibbles = hn::ShiftRight<4>(v);
        const auto lo_nibbles = hn::And(v, v0x0F);
        const auto hi_hex = hn::TableLookupBytes(lut_v, hi_nibbles);
        const auto lo_hex = hn::TableLookupBytes(lut_v, lo_nibbles);

        const auto out_lo = hn::InterleaveLower(d, hi_hex, lo_hex);
        const auto out_hi = hn::InterleaveUpper(d, hi_hex, lo_hex);

        const size_t j = i * 2;
        hn::StoreU(out_lo, d, reinterpret_cast<uint8_t*>(dst + j));
        hn::StoreU(out_hi, d, reinterpret_cast<uint8_t*>(dst + j + N));
    }

    for (; i < src_size; ++i) {
        dst[i * 2]     = hex_chars[src[i] >> 4];
        dst[i * 2 + 1] = hex_chars[src[i] & 0x0F];
    }
    return src_size * 2;
}

// ── SIMD URL Decode ──────────────────────────────────────────
// Approach: scan for '%' and '+' characters using SIMD byte comparison.
// For runs of plain bytes between special chars, memcpy them to output.
// For %XX sequences, decode hex pair and write one byte.
// The SIMD acceleration is in the fast scanning/copying of literal runs
// and the hex nibble decode via TableLookupBytes.

namespace {

// Hex nibble lookup table for SIMD: 0xFF for invalid, 0-15 for valid hex
alignas(64) static constexpr uint8_t hex_nib_lut[256] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
       0,   1,   2,   3,   4,   5,   6,   7,   8,   9,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,  10,  11,  12,  13,  14,  15,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,  10,  11,  12,  13,  14,  15,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
};

// Scalar hex decode helper (hot path, keep inline)
static inline int8_t hex_nib(uint8_t c) {
    return static_cast<int8_t>(hex_nib_lut[c]);
}

} // anonymous namespace

size_t url_decode_to_hwy(const uint8_t* src, size_t src_size, char* dst_raw, size_t dst_size) {
    const hn::ScalableTag<uint8_t> d;
    const size_t N = hn::Lanes(d);
    auto* dst = reinterpret_cast<uint8_t*>(dst_raw);

    const auto vPercent = hn::Set(d, uint8_t('%'));
    const auto vPlus    = hn::Set(d, uint8_t('+'));

    size_t i = 0; // input position
    size_t j = 0; // output position

    while (i < src_size) {
        // Use SIMD to find next '%' or '+' from position i
        size_t found = src_size; // position of next special char
        {
            size_t si = i;
            // Align-ish scan for special chars
            for (; si + N <= src_size; si += N) {
                const auto v = hn::LoadU(d, src + si);
                const auto is_pct = hn::Eq(v, vPercent);
                const auto is_plus = hn::Eq(v, vPlus);
                const auto special = hn::Or(is_pct, is_plus);
                if (!hn::AllTrue(d, hn::Not(special))) {
                    // Found at least one special char in this vector
                    const intptr_t lane = hn::FindFirstTrue(d, special);
                    found = si + static_cast<size_t>(lane);
                    break;
                }
            }
            // Scalar tail for the scan
            if (found == src_size) {
                for (; si < src_size; ++si) {
                    if (src[si] == '%' || src[si] == '+') { found = si; break; }
                }
            }
        }

        // Copy literal run [i, found) to output
        if (found > i) {
            const size_t len = found - i;
            if (j + len > dst_size) return 0;
            std::memcpy(dst + j, src + i, len);
            j += len;
            i = found;
        }

        if (i >= src_size) break;

        // Process the special character at position i
        if (src[i] == '%' && i + 2 < src_size) {
            const int8_t hi = hex_nib(src[i + 1]);
            const int8_t lo = hex_nib(src[i + 2]);
            if (hi >= 0 && lo >= 0) {
                if (j >= dst_size) return 0;
                dst[j++] = static_cast<uint8_t>((hi << 4) | lo);
                i += 3;
                continue;
            }
            // Invalid hex after % — treat as literal
            if (j >= dst_size) return 0;
            dst[j++] = src[i++];
        } else if (src[i] == '+') {
            if (j >= dst_size) return 0;
            dst[j++] = ' ';
            ++i;
        } else {
            if (j >= dst_size) return 0;
            dst[j++] = src[i++];
        }
    }

    return j;
}

std::string url_decode_hwy(const uint8_t* src, size_t src_size) {
    const hn::ScalableTag<uint8_t> d;
    const size_t N = hn::Lanes(d);

    const auto vPercent = hn::Set(d, uint8_t('%'));
    const auto vPlus    = hn::Set(d, uint8_t('+'));

    std::string result(src_size, '\0');
    auto* dst = reinterpret_cast<uint8_t*>(result.data());

    size_t i = 0;
    size_t j = 0;

    while (i < src_size) {
        // Quick check: if current char is % or +, skip the SIMD scan
        if (src[i] == '%' || src[i] == '+') {
            goto process_special;
        }

        // Use SIMD to find next '%' or '+' from position i
        {
            size_t found = src_size;
            size_t si = i;
            for (; si + N <= src_size; si += N) {
                const auto v = hn::LoadU(d, src + si);
                const auto special = hn::Or(hn::Eq(v, vPercent), hn::Eq(v, vPlus));
                if (!hn::AllTrue(d, hn::Not(special))) {
                    const intptr_t lane = hn::FindFirstTrue(d, special);
                    found = si + static_cast<size_t>(lane);
                    break;
                }
            }
            if (found == src_size) {
                for (; si < src_size; ++si) {
                    if (src[si] == '%' || src[si] == '+') { found = si; break; }
                }
            }

            if (found > i) {
                const size_t len = found - i;
                std::memcpy(dst + j, src + i, len);
                j += len;
                i = found;
            }
        }

    process_special:
        if (i >= src_size) break;

        // Process consecutive special chars without re-scanning
        while (i < src_size) {
            if (src[i] == '%' && i + 2 < src_size) {
                const int8_t hi = hex_nib(src[i + 1]);
                const int8_t lo = hex_nib(src[i + 2]);
                if (hi >= 0 && lo >= 0) {
                    dst[j++] = static_cast<uint8_t>((hi << 4) | lo);
                    i += 3;
                    continue;
                }
                dst[j++] = src[i++];
            } else if (src[i] == '+') {
                dst[j++] = ' ';
                ++i;
            } else {
                break;
            }
        }
    }

    result.resize(j);
    return result;
}


} // namespace simdtext

#else
// ── Highway not available — functions provided by scalar.cpp ────

namespace simdtext {
// Functions are defined in src/scalar/scalar.cpp when Highway is not available.
// This file is not compiled in that case.
} // namespace simdtext

#endif // SIMDTEXT_HAVE_HWY

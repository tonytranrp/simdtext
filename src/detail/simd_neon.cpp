#include <arm_neon.h>
#include <cstddef>

namespace simdtext::detail::neon {

size_t count_byte(const char* data, size_t size, char byte) {
    const uint8x16_t vbyte = vdupq_n_u8(static_cast<uint8_t>(byte));
    size_t count = 0;
    size_t i = 0;
    // 4x unrolled: accumulate across 4 vectors to hide horizontal reduction latency
    for (; i + 64 <= size; i += 64) {
        uint8x16_t c0 = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i));
        uint8x16_t c1 = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i + 16));
        uint8x16_t c2 = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i + 32));
        uint8x16_t c3 = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i + 48));
        uint8x16_t e0 = vceqq_u8(c0, vbyte);
        uint8x16_t e1 = vceqq_u8(c1, vbyte);
        uint8x16_t e2 = vceqq_u8(c2, vbyte);
        uint8x16_t e3 = vceqq_u8(c3, vbyte);
        // Combine: OR the results and count once (0xFF per match)
        uint8x16_t combined = vorrq_u8(vorrq_u8(e0, e1), vorrq_u8(e2, e3));
        // Actually, OR doesn't give us counts. We need to sum individually.
        // Use pairwise add to accumulate counts from all 4 vectors.
        uint8x16_t ones0 = vshrq_n_u8(e0, 7);
        uint8x16_t ones1 = vshrq_n_u8(e1, 7);
        uint8x16_t ones2 = vshrq_n_u8(e2, 7);
        uint8x16_t ones3 = vshrq_n_u8(e3, 7);
        // Sum across all 64 bytes
        uint16x8_t sum8 = vpadalq_u8(vpadalq_u8(vdupq_n_u16(0), ones0), ones1);
        uint16x8_t sum8b = vpadalq_u8(vpadalq_u8(vdupq_n_u16(0), ones2), ones3);
        uint32x4_t sum16 = vpadalq_u16(vpadalq_u16(vdupq_n_u32(0), sum8), sum8b);
        uint64x2_t sum32 = vpadalq_u32(vdupq_n_u64(0), sum16);
        count += vgetq_lane_u64(sum32, 0) + vgetq_lane_u64(sum32, 1);
    }
    for (; i + 16 <= size; i += 16) {
        uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i));
        uint8x16_t eq = vceqq_u8(chunk, vbyte);
        uint8x16_t ones = vshrq_n_u8(eq, 7);
        uint16x8_t sum8 = vpadalq_u8(vdupq_n_u16(0), ones);
        uint32x4_t sum16 = vpadalq_u16(vdupq_n_u32(0), sum8);
        uint64x2_t sum32 = vpadalq_u32(vdupq_n_u64(0), sum16);
        count += vgetq_lane_u64(sum32, 0) + vgetq_lane_u64(sum32, 1);
    }
    for (; i < size; ++i)
        if (data[i] == byte) ++count;
    return count;
}

bool is_ascii(const char* data, size_t size) {
    size_t i = 0;
    // 4x unrolled: accumulate OR across 4 vectors before checking
    for (; i + 64 <= size; i += 64) {
        uint8x16_t c0 = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i));
        uint8x16_t c1 = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i + 16));
        uint8x16_t c2 = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i + 32));
        uint8x16_t c3 = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i + 48));
        uint8x16_t ored = vorrq_u8(vorrq_u8(c0, c1), vorrq_u8(c2, c3));
        // Single instruction: vmaxvq_u8 does the horizontal max in one op
        if (vmaxvq_u8(ored) > 0x7F)
            return false;
    }
    for (; i + 16 <= size; i += 16) {
        uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i));
        if (vmaxvq_u8(chunk) > 0x7F)
            return false;
    }
    for (; i < size; ++i)
        if (static_cast<unsigned char>(data[i]) > 0x7F) return false;
    return true;
}

void lowercase_ascii(char* data, size_t size) {
    const uint8x16_t vA = vdupq_n_u8('A');
    const uint8x16_t vZ = vdupq_n_u8('Z');
    const uint8x16_t vbit5 = vdupq_n_u8(0x20);
    size_t i = 0;
    for (; i + 16 <= size; i += 16) {
        uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i));
        uint8x16_t ge_A = vcgeq_u8(chunk, vA);
        uint8x16_t le_Z = vcleq_u8(chunk, vZ);
        uint8x16_t is_upper = vandq_u8(ge_A, le_Z);
        // XOR with bit 5 to flip case
        uint8x16_t lowered = veorq_u8(chunk, vandq_u8(is_upper, vbit5));
        vst1q_u8(reinterpret_cast<uint8_t*>(data + i), lowered);
    }
    for (; i < size; ++i) {
        auto c = static_cast<unsigned char>(data[i]);
        if (c >= 'A' && c <= 'Z') data[i] = static_cast<char>(c ^ 0x20);
    }
}

void uppercase_ascii(char* data, size_t size) {
    const uint8x16_t va = vdupq_n_u8('a');
    const uint8x16_t vz = vdupq_n_u8('z');
    const uint8x16_t vbit5 = vdupq_n_u8(0x20);
    size_t i = 0;
    for (; i + 16 <= size; i += 16) {
        uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i));
        uint8x16_t ge_a = vcgeq_u8(chunk, va);
        uint8x16_t le_z = vcleq_u8(chunk, vz);
        uint8x16_t is_lower = vandq_u8(ge_a, le_z);
        uint8x16_t uppered = veorq_u8(chunk, vandq_u8(is_lower, vbit5));
        vst1q_u8(reinterpret_cast<uint8_t*>(data + i), uppered);
    }
    for (; i < size; ++i) {
        auto c = static_cast<unsigned char>(data[i]);
        if (c >= 'a' && c <= 'z') data[i] = static_cast<char>(c ^ 0x20);
    }
}

// ── NEON count_code_points ─────────────────────────────────────
// Count code points = count bytes that are NOT continuation bytes (10xxxxxx)
// Continuation bytes: (byte & 0xC0) == 0x80, i.e. 0x80–0xBF

size_t count_code_points(const char* data, size_t size) {
    const uint8x16_t vC0 = vdupq_n_u8(0xC0);
    const uint8x16_t v80 = vdupq_n_u8(0x80);
    size_t i = 0;
    size_t count = 0;

    // 4x unrolled
    for (; i + 64 <= size; i += 64) {
        uint8x16_t c0 = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i));
        uint8x16_t c1 = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i + 16));
        uint8x16_t c2 = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i + 32));
        uint8x16_t c3 = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i + 48));
        // Non-continuation: (byte & 0xC0) != 0x80
        // Continuation: vandq + vceqq → 0xFF where (byte & 0xC0) == 0x80
        uint8x16_t cont0 = vceqq_u8(vandq_u8(c0, vC0), v80);
        uint8x16_t cont1 = vceqq_u8(vandq_u8(c1, vC0), v80);
        uint8x16_t cont2 = vceqq_u8(vandq_u8(c2, vC0), v80);
        uint8x16_t cont3 = vceqq_u8(vandq_u8(c3, vC0), v80);
        // Non-continuation count = 16 - continuation count per chunk
        // Count continuation bytes (0xFF → 1 after shift)
        uint8x16_t ones0 = vshrq_n_u8(cont0, 7);
        uint8x16_t ones1 = vshrq_n_u8(cont1, 7);
        uint8x16_t ones2 = vshrq_n_u8(cont2, 7);
        uint8x16_t ones3 = vshrq_n_u8(cont3, 7);
        // Sum continuation bytes across all 4 vectors
        uint16x8_t s8a = vpadalq_u8(vpadalq_u8(vdupq_n_u16(0), ones0), ones1);
        uint16x8_t s8b = vpadalq_u8(vpadalq_u8(vdupq_n_u16(0), ones2), ones3);
        uint32x4_t s16 = vpadalq_u16(vpadalq_u16(vdupq_n_u32(0), s8a), s8b);
        uint64x2_t s32 = vpadalq_u32(vdupq_n_u64(0), s16);
        size_t cont_count = vgetq_lane_u64(s32, 0) + vgetq_lane_u64(s32, 1);
        count += 64 - cont_count;
    }
    for (; i + 16 <= size; i += 16) {
        uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i));
        uint8x16_t cont = vceqq_u8(vandq_u8(chunk, vC0), v80);
        uint8x16_t ones = vshrq_n_u8(cont, 7);
        count += 16 - static_cast<size_t>(vaddvq_u8(ones));
    }
    for (; i < size; ++i)
        if ((static_cast<unsigned char>(data[i]) & 0xC0) != 0x80) ++count;
    return count;
}

// ── NEON validate_utf8 ─────────────────────────────────────────

bool validate_utf8(const char* data, size_t size) {
    const uint8x16_t vhigh = vdupq_n_u8(0x80);
    int expected_cont = 0;
    uint8_t prev_lead_byte = 0;
    uint8_t prev_lead_class = 0;
    uint8_t first_cont_byte = 0;

    size_t i = 0;
    for (; i + 16 <= size; i += 16) {
        uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i));
        // Fast path: if all ASCII and not expecting continuation bytes, skip
        if (vmaxvq_u8(chunk) <= 0x7F) {
            if (expected_cont > 0) return false;
            continue;
        }
        // Non-ASCII: process byte-by-byte within the chunk
        const auto* p = reinterpret_cast<const uint8_t*>(data + i);
        const auto* chunk_end = p + 16;
        while (p < chunk_end) {
            const auto byte = *p++;
            if (byte <= 0x7F) {
                if (expected_cont > 0) return false;
            } else if ((byte & 0xE0) == 0xC0) {
                if (expected_cont > 0) return false;
                if (byte < 0xC2) return false;
                expected_cont = 1;
                prev_lead_class = 2;
                prev_lead_byte = byte;
            } else if ((byte & 0xF0) == 0xE0) {
                if (expected_cont > 0) return false;
                expected_cont = 2;
                prev_lead_class = 3;
                prev_lead_byte = byte;
                first_cont_byte = 0;
            } else if ((byte & 0xF8) == 0xF0) {
                if (expected_cont > 0) return false;
                if (byte > 0xF4) return false;
                expected_cont = 3;
                prev_lead_class = 4;
                prev_lead_byte = byte;
                first_cont_byte = 0;
            } else if ((byte & 0xC0) == 0x80) {
                if (expected_cont == 0) return false;
                if (expected_cont == prev_lead_class - 1) {
                    first_cont_byte = byte;
                    if (prev_lead_byte == 0xE0 && byte < 0xA0) return false;
                    if (prev_lead_byte == 0xED && byte > 0x9F) return false;
                    if (prev_lead_byte == 0xF0 && byte < 0x90) return false;
                    if (prev_lead_byte == 0xF4 && byte > 0x8F) return false;
                }
                --expected_cont;
            } else {
                return false;
            }
        }
    }

    // Scalar tail
    const auto* p = reinterpret_cast<const uint8_t*>(data + i);
    const auto* end = reinterpret_cast<const uint8_t*>(data + size);
    while (p < end) {
        const auto byte = *p++;
        if (byte <= 0x7F) {
            if (expected_cont > 0) return false;
        } else if ((byte & 0xE0) == 0xC0) {
            if (expected_cont > 0) return false;
            if (byte < 0xC2) return false;
            expected_cont = 1;
            prev_lead_byte = byte;
            prev_lead_class = 2;
        } else if ((byte & 0xF0) == 0xE0) {
            if (expected_cont > 0) return false;
            expected_cont = 2;
            prev_lead_class = 3;
            prev_lead_byte = byte;
            first_cont_byte = 0;
        } else if ((byte & 0xF8) == 0xF0) {
            if (expected_cont > 0) return false;
            if (byte > 0xF4) return false;
            expected_cont = 3;
            prev_lead_class = 4;
            prev_lead_byte = byte;
            first_cont_byte = 0;
        } else if ((byte & 0xC0) == 0x80) {
            if (expected_cont == 0) return false;
            if (expected_cont == prev_lead_class - 1) {
                first_cont_byte = byte;
                if (prev_lead_byte == 0xE0 && byte < 0xA0) return false;
                if (prev_lead_byte == 0xED && byte > 0x9F) return false;
                if (prev_lead_byte == 0xF0 && byte < 0x90) return false;
                if (prev_lead_byte == 0xF4 && byte > 0x8F) return false;
            }
            --expected_cont;
        } else {
            return false;
        }
    }
    return expected_cont == 0;
}

const char* find_byte(const char* data, size_t size, char byte) {
    const uint8x16_t vbyte = vdupq_n_u8(static_cast<uint8_t>(byte));
    size_t i = 0;
    for (; i + 16 <= size; i += 16) {
        uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i));
        uint8x16_t eq = vceqq_u8(chunk, vbyte);
        // Fast check: vminvq_u8 returns 0xFF iff any lane matched
        if (vminvq_u8(eq) == 0xFF) {
            // Shift right by 7 to get 0/1 per lane, then extract as two uint64
            uint8x16_t bits = vshrq_n_u8(eq, 7);
            uint64_t lo = vgetq_lane_u64(vreinterpretq_u64_u8(bits), 0);
            uint64_t hi = vgetq_lane_u64(vreinterpretq_u64_u8(bits), 1);
            // Pack LSB of each byte into a 16-bit mask (scalar integer ops, not lane extracts)
            uint16_t mask = 0;
            uint64_t m = lo;
            for (int b = 0; b < 8; ++b) { mask |= static_cast<uint16_t>((m & 1) << b); m >>= 8; }
            m = hi;
            for (int b = 0; b < 8; ++b) { mask |= static_cast<uint16_t>((m & 1) << (8 + b)); m >>= 8; }
            unsigned int bit_pos = static_cast<unsigned int>(__builtin_ctz(mask));
            return data + i + bit_pos;
        }
    }
    // Scalar tail
    for (; i < size; ++i)
        if (data[i] == byte) return data + i;
    return data + size;
}

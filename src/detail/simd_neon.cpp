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
        uint8x16_t high = vandq_u8(ored, vdupq_n_u8(0x80));
        uint64x2_t reduced = vpaddlq_u32(vpaddlq_u16(vpaddlq_u8(high)));
        if (vgetq_lane_u64(reduced, 0) != 0 || vgetq_lane_u64(reduced, 1) != 0)
            return false;
    }
    for (; i + 16 <= size; i += 16) {
        uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i));
        uint8x16_t high = vandq_u8(chunk, vdupq_n_u8(0x80));
        uint64x2_t reduced = vpaddlq_u32(vpaddlq_u16(vpaddlq_u8(high)));
        if (vgetq_lane_u64(reduced, 0) != 0 || vgetq_lane_u64(reduced, 1) != 0)
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

const char* find_byte(const char* data, size_t size, char byte) {
    const uint8x16_t vbyte = vdupq_n_u8(static_cast<uint8_t>(byte));
    size_t i = 0;
    for (; i + 16 <= size; i += 16) {
        uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i));
        uint8x16_t eq = vceqq_u8(chunk, vbyte);
        // Extract match bits into a 16-bit mask using NEON narrowing operations.
        // Shift each match byte right by 7 to get 0/1, then pack into a bitmask.
        uint8x16_t bits = vshrq_n_u8(eq, 7);
        // Use shrn + sli to pack 16 bytes into a 64-bit value preserving positions.
        // Method: extract each lane's bit and accumulate into a uint16_t mask.
        uint16x8_t paired = vpaddlq_u8(bits);         // 8x uint16, each is sum of 2 adjacent bits
        uint32x4_t paired2 = vpaddlq_u16(paired);     // 4x uint32, each is sum of 4 adjacent bits
        uint64x2_t paired3 = vpaddlq_u32(paired2);    // 2x uint64, each is sum of 8 adjacent bits
        // Reconstruct the 16-bit mask from the pairwise sums
        // Lane 0 has bits 0-7, lane 1 has bits 8-15 (as counts 0-8)
        // We need the original bit positions, so use a different approach:
        // Use vshrn_n_u16 to narrow and extract.
        // Actually, let's use a direct extraction with vgetq_lane:
        uint16_t mask = 0;
        mask |= static_cast<uint16_t>(vgetq_lane_u8(bits, 0));
        mask |= static_cast<uint16_t>(vgetq_lane_u8(bits, 1)) << 1;
        mask |= static_cast<uint16_t>(vgetq_lane_u8(bits, 2)) << 2;
        mask |= static_cast<uint16_t>(vgetq_lane_u8(bits, 3)) << 3;
        mask |= static_cast<uint16_t>(vgetq_lane_u8(bits, 4)) << 4;
        mask |= static_cast<uint16_t>(vgetq_lane_u8(bits, 5)) << 5;
        mask |= static_cast<uint16_t>(vgetq_lane_u8(bits, 6)) << 6;
        mask |= static_cast<uint16_t>(vgetq_lane_u8(bits, 7)) << 7;
        mask |= static_cast<uint16_t>(vgetq_lane_u8(bits, 8)) << 8;
        mask |= static_cast<uint16_t>(vgetq_lane_u8(bits, 9)) << 9;
        mask |= static_cast<uint16_t>(vgetq_lane_u8(bits, 10)) << 10;
        mask |= static_cast<uint16_t>(vgetq_lane_u8(bits, 11)) << 11;
        mask |= static_cast<uint16_t>(vgetq_lane_u8(bits, 12)) << 12;
        mask |= static_cast<uint16_t>(vgetq_lane_u8(bits, 13)) << 13;
        mask |= static_cast<uint16_t>(vgetq_lane_u8(bits, 14)) << 14;
        mask |= static_cast<uint16_t>(vgetq_lane_u8(bits, 15)) << 15;
        if (mask != 0) {
            unsigned int bit_pos = static_cast<unsigned int>(__builtin_ctz(mask));
            return data + i + bit_pos;
        }
    }
    // Scalar tail
    for (; i < size; ++i)
        if (data[i] == byte) return data + i;
    return data + size;
}

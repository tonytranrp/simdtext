#include <arm_neon.h>
#include <cstddef>

namespace simdtext::detail::neon {

size_t count_byte(const char* data, size_t size, char byte) {
    const uint8x16_t vbyte = vdupq_n_u8(static_cast<uint8_t>(byte));
    size_t count = 0;
    size_t i = 0;
    for (; i + 16 <= size; i += 16) {
        uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i));
        uint8x16_t eq = vceqq_u8(chunk, vbyte);
        // Sum the 16 comparison results (each 0 or 255) and divide by 255
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
    for (; i + 16 <= size; i += 16) {
        uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i));
        // Check if any byte has high bit set
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
    size_t i = 0;
    for (; i + 16 <= size; i += 16) {
        uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i));
        uint8x16_t ge_a = vcgeq_u8(chunk, vdupq_n_u8('A'));
        uint8x16_t le_z = vcleq_u8(chunk, vdupq_n_u8('Z'));
        uint8x16_t is_upper = vandq_u8(ge_a, le_z);
        // Add 32 where is_upper is 0xFF, add 0 where it's 0x00
        uint8x16_t delta = vandq_u8(is_upper, vdupq_n_u8(32));
        uint8x16_t lowered = vaddq_u8(chunk, delta);
        vst1q_u8(reinterpret_cast<uint8_t*>(data + i), lowered);
    }
    for (; i < size; ++i) {
        auto c = static_cast<unsigned char>(data[i]);
        if (c >= 'A' && c <= 'Z') data[i] = static_cast<char>(c + 32);
    }
}

void uppercase_ascii(char* data, size_t size) {
    size_t i = 0;
    for (; i + 16 <= size; i += 16) {
        uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i));
        uint8x16_t ge_a = vcgeq_u8(chunk, vdupq_n_u8('a'));
        uint8x16_t le_z = vcleq_u8(chunk, vdupq_n_u8('z'));
        uint8x16_t is_lower = vandq_u8(ge_a, le_z);
        // Subtract 32 where is_lower is 0xFF
        uint8x16_t delta = vandq_u8(is_lower, vdupq_n_u8(32));
        uint8x16_t uppered = vsubq_u8(chunk, delta);
        vst1q_u8(reinterpret_cast<uint8_t*>(data + i), uppered);
    }
    for (; i < size; ++i) {
        auto c = static_cast<unsigned char>(data[i]);
        if (c >= 'a' && c <= 'z') data[i] = static_cast<char>(c - 32);
    }
}

const char* find_byte(const char* data, size_t size, char byte) {
    const uint8x16_t vbyte = vdupq_n_u8(static_cast<uint8_t>(byte));
    size_t i = 0;
    for (; i + 16 <= size; i += 16) {
        uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i));
        uint8x16_t eq = vceqq_u8(chunk, vbyte);
        // Extract match positions: compress 16 comparison results into 16 bits
        uint16_t mask = vshrn_n_u16(vreinterpretq_u16_u8(eq), 4);
        // Actually, let's use a simpler approach
        // Move the high bits of each lane into a u64 and scan
        uint8x16_t shifted = vshrq_n_u8(eq, 7);
        uint64x2_t packed = vpaddlq_u32(vpaddlq_u16(vpaddlq_u8(shifted)));
        // This doesn't give us position. Use scalar extraction instead.
        // Store and check
        uint8_t results[16];
        vst1q_u8(results, eq);
        for (int j = 0; j < 16; ++j) {
            if (results[j] == 0xFF) {
                return data + i + j;
            }
        }
    }
    for (; i < size; ++i)
        if (data[i] == byte) return data + i;
    return data + size;
}

} // namespace simdtext::detail::neon

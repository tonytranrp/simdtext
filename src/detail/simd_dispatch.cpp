#include "simdtext/detail/cpu_detect.hpp"
#include <cstddef>

namespace simdtext::detail {

// Forward declarations — scalar
namespace scalar {
size_t count_byte(const char* data, size_t size, char byte);
bool is_ascii(const char* data, size_t size);
void lowercase_ascii(char* data, size_t size);
void uppercase_ascii(char* data, size_t size);
const char* find_byte(const char* data, size_t size, char byte);
bool validate_utf8(const char* data, size_t size);
}

// Forward declarations — x86 SSE2/AVX2 (only linked on x86)
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
namespace sse2 {
size_t count_byte(const char* data, size_t size, char byte);
bool is_ascii(const char* data, size_t size);
void lowercase_ascii(char* data, size_t size);
void uppercase_ascii(char* data, size_t size);
const char* find_byte(const char* data, size_t size, char byte);
bool validate_utf8(const char* data, size_t size);
}

namespace avx2 {
size_t count_byte(const char* data, size_t size, char byte);
bool is_ascii(const char* data, size_t size);
void lowercase_ascii(char* data, size_t size);
void uppercase_ascii(char* data, size_t size);
const char* find_byte(const char* data, size_t size, char byte);
bool validate_utf8(const char* data, size_t size);
}

#if defined(__AVX512BW__)
namespace avx512 {
size_t count_byte(const char* data, size_t size, char byte);
bool is_ascii(const char* data, size_t size);
void lowercase_ascii(char* data, size_t size);
void uppercase_ascii(char* data, size_t size);
const char* find_byte(const char* data, size_t size, char byte);
}
#endif
#endif

// Forward declarations — ARM NEON (only linked on ARM)
#if defined(__aarch64__) || defined(__ARM_NEON)
namespace neon {
size_t count_byte(const char* data, size_t size, char byte);
bool is_ascii(const char* data, size_t size);
void lowercase_ascii(char* data, size_t size);
void uppercase_ascii(char* data, size_t size);
const char* find_byte(const char* data, size_t size, char byte);
}
#endif

// ── Dispatch functions ─────────────────────────────────────

size_t count_byte_dispatch(const char* data, size_t size, char byte) {
    const auto& f = detect_cpu();
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#if defined(__AVX512BW__)
    if (f.avx512bw) return avx512::count_byte(data, size, byte);
#endif
    if (f.avx2)    return avx2::count_byte(data, size, byte);
    if (f.sse2)    return sse2::count_byte(data, size, byte);
#elif defined(__aarch64__) || defined(__ARM_NEON)
    if (f.neon)    return neon::count_byte(data, size, byte);
#endif
    return scalar::count_byte(data, size, byte);
}

bool is_ascii_dispatch(const char* data, size_t size) {
    const auto& f = detect_cpu();
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#if defined(__AVX512BW__)
    if (f.avx512bw) return avx512::is_ascii(data, size);
#endif
    if (f.avx2)    return avx2::is_ascii(data, size);
    if (f.sse2)    return sse2::is_ascii(data, size);
#elif defined(__aarch64__) || defined(__ARM_NEON)
    if (f.neon)    return neon::is_ascii(data, size);
#endif
    return scalar::is_ascii(data, size);
}

void lowercase_ascii_dispatch(char* data, size_t size) {
    const auto& f = detect_cpu();
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#if defined(__AVX512BW__)
    if (f.avx512bw) return avx512::lowercase_ascii(data, size);
#endif
    if (f.avx2)    return avx2::lowercase_ascii(data, size);
    if (f.sse2)    return sse2::lowercase_ascii(data, size);
#elif defined(__aarch64__) || defined(__ARM_NEON)
    if (f.neon)    return neon::lowercase_ascii(data, size);
#endif
    return scalar::lowercase_ascii(data, size);
}

void uppercase_ascii_dispatch(char* data, size_t size) {
    const auto& f = detect_cpu();
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#if defined(__AVX512BW__)
    if (f.avx512bw) return avx512::uppercase_ascii(data, size);
#endif
    if (f.avx2)    return avx2::uppercase_ascii(data, size);
    if (f.sse2)    return sse2::uppercase_ascii(data, size);
#elif defined(__aarch64__) || defined(__ARM_NEON)
    if (f.neon)    return neon::uppercase_ascii(data, size);
#endif
    return scalar::uppercase_ascii(data, size);
}

const char* find_byte_dispatch(const char* data, size_t size, char byte) {
    const auto& f = detect_cpu();
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#if defined(__AVX512BW__)
    if (f.avx512bw) return avx512::find_byte(data, size, byte);
#endif
    if (f.avx2)    return avx2::find_byte(data, size, byte);
    if (f.sse2)    return sse2::find_byte(data, size, byte);
#elif defined(__aarch64__) || defined(__ARM_NEON)
    if (f.neon)    return neon::find_byte(data, size, byte);
#endif
    return scalar::find_byte(data, size, byte);
}

// ── UTF-8 validation dispatch ──────────────────────────────

bool validate_utf8_dispatch(const char* data, size_t size) {
    const auto& f = detect_cpu();
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    if (f.avx2)    return avx2::validate_utf8(data, size);
    if (f.sse2)    return sse2::validate_utf8(data, size);
#endif
    return scalar::validate_utf8(data, size);
}

} // namespace simdtext::detail

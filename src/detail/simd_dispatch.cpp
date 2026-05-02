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
}

// Forward declarations — SSE2
namespace sse2 {
size_t count_byte(const char* data, size_t size, char byte);
bool is_ascii(const char* data, size_t size);
void lowercase_ascii(char* data, size_t size);
void uppercase_ascii(char* data, size_t size);
const char* find_byte(const char* data, size_t size, char byte);
}

// Forward declarations — AVX2
namespace avx2 {
size_t count_byte(const char* data, size_t size, char byte);
bool is_ascii(const char* data, size_t size);
void lowercase_ascii(char* data, size_t size);
void uppercase_ascii(char* data, size_t size);
const char* find_byte(const char* data, size_t size, char byte);
}

// ── Dispatch functions ─────────────────────────────────────

size_t count_byte_dispatch(const char* data, size_t size, char byte) {
    const auto& f = detect_cpu();
    if (f.avx2)    return avx2::count_byte(data, size, byte);
    if (f.sse2)    return sse2::count_byte(data, size, byte);
    return scalar::count_byte(data, size, byte);
}

bool is_ascii_dispatch(const char* data, size_t size) {
    const auto& f = detect_cpu();
    if (f.avx2)    return avx2::is_ascii(data, size);
    if (f.sse2)    return sse2::is_ascii(data, size);
    return scalar::is_ascii(data, size);
}

void lowercase_ascii_dispatch(char* data, size_t size) {
    const auto& f = detect_cpu();
    if (f.avx2)    return avx2::lowercase_ascii(data, size);
    if (f.sse2)    return sse2::lowercase_ascii(data, size);
    return scalar::lowercase_ascii(data, size);
}

void uppercase_ascii_dispatch(char* data, size_t size) {
    const auto& f = detect_cpu();
    if (f.avx2)    return avx2::uppercase_ascii(data, size);
    if (f.sse2)    return sse2::uppercase_ascii(data, size);
    return scalar::uppercase_ascii(data, size);
}

const char* find_byte_dispatch(const char* data, size_t size, char byte) {
    const auto& f = detect_cpu();
    if (f.avx2)    return avx2::find_byte(data, size, byte);
    if (f.sse2)    return sse2::find_byte(data, size, byte);
    return scalar::find_byte(data, size, byte);
}

} // namespace simdtext::detail

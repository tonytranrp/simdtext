#pragma once

#include <cstdint>
#include <cstddef>

namespace simdtext::detail {

struct CpuFeatures {
    bool sse2    = false;
    bool sse41   = false;
    bool avx2    = false;
    bool avx512bw = false;
    bool neon    = false;  // ARM
};

/// Detect and cache CPU features. Returns a reference to the cached result.
const CpuFeatures& detect_cpu();

/// UTF-8 validation dispatch (selects best available implementation).
bool validate_utf8_dispatch(const char* data, size_t size);

} // namespace simdtext::detail

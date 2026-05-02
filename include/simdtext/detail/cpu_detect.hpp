#pragma once

#include <cstdint>

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

} // namespace simdtext::detail

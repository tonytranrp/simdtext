#include "simdtext/detail/cpu_detect.hpp"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace simdtext::detail {

namespace {

CpuFeatures do_detect() {
    CpuFeatures f;

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

#if defined(_MSC_VER)
    int regs[4];
    __cpuid(regs, 1);
    f.sse2  = (regs[3] & (1 << 26)) != 0;
    f.sse41 = (regs[2] & (1 << 19)) != 0;

    __cpuid(regs, 7);
    f.avx2    = (regs[1] & (1 << 5)) != 0;
    f.avx512bw = (regs[1] & (1 << 30)) != 0;
#elif defined(__GNUC__) || defined(__clang__)
    f.sse2     = __builtin_cpu_supports("sse2");
    f.sse41    = __builtin_cpu_supports("sse4.1");
    f.avx2     = __builtin_cpu_supports("avx2");
    f.avx512bw = __builtin_cpu_supports("avx512bw");
#endif

#elif defined(__aarch64__) || defined(__ARM_NEON)
    f.neon = true;
#endif

    return f;
}

} // anonymous namespace

const CpuFeatures& detect_cpu() {
    static CpuFeatures cached = do_detect();
    return cached;
}

} // namespace simdtext::detail

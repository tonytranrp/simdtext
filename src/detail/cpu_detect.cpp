#include "simdtext/detail/cpu_detect.hpp"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace simdtext::detail {

namespace {

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

#if defined(_MSC_VER)
// MSVC: use __cpuid intrinsic
static void cpuid(int regs[4], int level) {
    __cpuid(regs, level);
}
static void cpuidex(int regs[4], int level, int count) {
    __cpuidex(regs, level, count);
}
#elif defined(__GNUC__) || defined(__clang__)
// GCC/Clang: use inline asm for CPUID — avoids __builtin_cpu_supports
// which may require -march=native or GCC startup objects on some Clang builds.
static void cpuid(int regs[4], int level) {
    int eax = 0, ebx = 0, ecx = 0, edx = 0;
#if defined(__i386__) && defined(__PIC__)
    // PIC on 32-bit: save ebx
    __asm__ __volatile__(
        "mov %%ebx, %%edi\n\t"
        "cpuid\n\t"
        "xchg %%edi, %%ebx"
        : "=a"(eax), "=D"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(level)
    );
#else
    __asm__ __volatile__(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(level)
    );
#endif
    regs[0] = eax; regs[1] = ebx; regs[2] = ecx; regs[3] = edx;
}

static void cpuidex(int regs[4], int level, int count) {
    int eax = 0, ebx = 0, ecx = 0, edx = 0;
#if defined(__i386__) && defined(__PIC__)
    __asm__ __volatile__(
        "mov %%ebx, %%edi\n\t"
        "cpuid\n\t"
        "xchg %%edi, %%ebx"
        : "=a"(eax), "=D"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(level), "c"(count)
    );
#else
    __asm__ __volatile__(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(level), "c"(count)
    );
#endif
    regs[0] = eax; regs[1] = ebx; regs[2] = ecx; regs[3] = edx;
}
#endif

#endif // x86

CpuFeatures do_detect() {
    CpuFeatures f;

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    int regs[4];

    // CPUID leaf 1
    cpuid(regs, 1);
    f.sse2  = (regs[3] & (1 << 26)) != 0;
    f.sse41 = (regs[2] & (1 << 19)) != 0;

    // CPUID leaf 7, sub-leaf 0
    cpuidex(regs, 7, 0);
    f.avx2     = (regs[1] & (1 << 5)) != 0;
    f.avx512bw = (regs[1] & (1 << 30)) != 0;

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

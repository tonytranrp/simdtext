#pragma once

// Shared library export macros
#if defined(SIMDTEXT_SHARED)
    #if defined(_WIN32)
        #ifdef SIMDTEXT_BUILDING
            #define SIMDTEXT_API __declspec(dllexport)
        #else
            #define SIMDTEXT_API __declspec(dllimport)
        #endif
    #else
        #define SIMDTEXT_API __attribute__((visibility("default")))
    #endif
#else
    #define SIMDTEXT_API
#endif

// C++20 Module support
#ifdef SIMDTEXT_MODULE
    #define SIMDTEXT_EXPORT export
#else
    #define SIMDTEXT_EXPORT
#endif

// Force inline for hot paths
#if defined(_MSC_VER)
    #define SIMDTEXT_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    #define SIMDTEXT_FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define SIMDTEXT_FORCE_INLINE inline
#endif

// SIMD target attributes for per-function dispatch
#if defined(__GNUC__)
    #define SIMDTEXT_TARGET_SSE2 __attribute__((target("sse2")))
    #define SIMDTEXT_TARGET_SSE41 __attribute__((target("sse4.1")))
    #define SIMDTEXT_TARGET_AVX2 __attribute__((target("avx2,bmi")))
    #define SIMDTEXT_TARGET_AVX512BW __attribute__((target("avx512bw,avx512f")))
    #define SIMDTEXT_TARGET_NEON __attribute__((target("+neon")))
#elif defined(_MSC_VER)
    #define SIMDTEXT_TARGET_SSE2
    #define SIMDTEXT_TARGET_SSE41
    #define SIMDTEXT_TARGET_AVX2
    #define SIMDTEXT_TARGET_AVX512BW
    #define SIMDTEXT_TARGET_NEON
#else
    #define SIMDTEXT_TARGET_SSE2
    #define SIMDTEXT_TARGET_SSE41
    #define SIMDTEXT_TARGET_AVX2
    #define SIMDTEXT_TARGET_AVX512BW
    #define SIMDTEXT_TARGET_NEON
#endif

// Deprecation macro
#if defined(__has_cpp_attribute)
    #if __has_cpp_attribute(deprecated)
        #define SIMDTEXT_DEPRECATED(msg) [[deprecated(msg)]]
    #endif
#endif
#if !defined(SIMDTEXT_DEPRECATED)
    #define SIMDTEXT_DEPRECATED(msg)
#endif

// Nodiscard
#define SIMDTEXT_NODISCARD [[nodiscard]]

// Cold function hint
#if defined(__GNUC__) || defined(__clang__)
    #define SIMDTEXT_COLD __attribute__((cold))
#else
    #define SIMDTEXT_COLD
#endif

// Unreachable
#if __has_builtin(__builtin_unreachable)
    #define SIMDTEXT_UNREACHABLE() __builtin_unreachable()
#else
    #define SIMDTEXT_UNREACHABLE() do { } while(0)
#endif

// Restrict pointer aliasing hint
#if defined(__GNUC__) || defined(__clang__)
    #define SIMDTEXT_RESTRICT __restrict__
#elif defined(_MSC_VER)
    #define SIMDTEXT_RESTRICT __restrict
#else
    #define SIMDTEXT_RESTRICT
#endif

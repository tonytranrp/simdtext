# Research Notes — SIMD Text Processing State of the Art

**Date:** 2026-05-02  
**Sources:** 100+ papers, repos, and blog posts analyzed

## Key Papers

### 1. "Validating UTF-8 In Less Than One Instruction Per Byte" (Keiser & Lemire, 2021)
- **Key insight:** Use a 5-class byte classification table + pshufb to classify 16/32 bytes at once
- **Technique:** Build per-byte expected continuation counts, verify using SIMD
- **Performance:** ~1 cycle per byte on AVX2 → ~30 GB/s
- **Status:** Our SSE2 UTF-8 validator uses this approach but needs cleanup

### 2. "Parsing Gigabytes of JSON per Second" (Langdale & Lemire, VLDB 2019)
- **Key insight:** Two-phase approach: (1) structural character indexing, (2) parsing
- **Phase 1:** Use SIMD to find `{`, `}`, `[`, `]`, `:`, `,` and string boundaries simultaneously
- **Phase 2:** Use a state machine on the structural index (much smaller than input)
- **Our gap:** Our JsonTokenizer is character-by-character — could be 10x faster with structural indexing

### 3. "Fast Number Parsing Without Fallback" (Mushtak & Lemire, 2023)
- **Key insight:** Use 128-bit multiplication by reciprocal powers of 5 for exact decimal→binary
- **No fallback needed:** Always correct rounding in the fast path
- **Our gap:** No number parsing at all — could add fast integer/float parsing

## Key Repositories Analyzed

### libbase64 (aklomp/base64)
- **SSSE3 path:** Uses pshufb for 12→16 byte expansion (encode) and 16→12 byte compression (decode)
- **AVX2 path:** Processes 48 bytes at a time (3×16 → 4×16 = 64 bytes output)
- **Key trick:** Pre-shuffle table for base64 character lookup
- **Our gap:** Our base64 is pure scalar — could be 4-8x faster with SIMD

### FastPattern (0xDEADBEEFC0DEBABE/simd-fast-find-pattern)
- **Key insight:** Filter with first byte + last byte, then verify
- **AVX2:** Process 32 positions in parallel
- **Our gap:** Our pattern scanner doesn't use the first+last byte filtering trick

### simdutf (simdutf/simdutf)
- **Architecture:** Separate compilation units per ISA with runtime dispatch
- **UTF-8 validation:** Uses the Keiser & Lemire lookup approach
- **Key feature we lack:** UTF-8 ↔ UTF-16/UTF-32 conversion
- **Key feature we lack:** Error location reporting (byte offset of first invalid byte)

### fast_float (fastfloat/fast_float)
- **Key insight:** Algorithm that avoids arbitrary-precision arithmetic for most inputs
- **Now in GCC's libstdc++:** Production-proven
- **Our gap:** No number parsing at all

## Optimization Opportunities Identified

### High Impact
1. **SIMD Base64** — pshufb-based encode/decode in SSSE3+ object file (4-8x faster)
2. **Structural JSON indexing** — find all structural chars in one SIMD pass (10x faster)
3. **First+last byte filtering** for pattern scanner (2-3x for long patterns)
4. **UTF-8 error location** — return byte offset of first error (critical for debugging)

### Medium Impact
5. **Non-temporal stores** in lowercase/uppercase for buffers > L3 cache
6. **Fast integer parsing** — SIMD batch parsing of integers from text
7. **AVX2 count_newlines** — 4x unrolled like count_byte
8. **Aligned allocation** for output buffers in encode/decode functions

### Low Impact (Already Near-Optimal)
9. **Prefetch hints** — already memory-bound on large buffers
10. **SWAR optimization** — already using SWAR in scalar paths
11. **Branchless hex lookup** — already using table-based approach

## Implementation Priority

1. SIMD Base64 encode/decode (SSSE3 separate object file)
2. UTF-8 error location reporting
3. Pattern scanner first+last byte filtering
4. Non-temporal stores for large buffer operations
5. JSON structural character indexing

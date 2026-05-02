# Disassembly Analysis — simdtext

**Date:** 2026-05-02  
**Compiler:** GCC 13.3.0, `-O2`  
**Target:** x86_64 (Vultr VPS, AVX2 + AVX-512BW)

## Key Findings

### 1. SSE2 count_byte — FIXED ✅
**Before:** 4 function calls to `popcount` per 64-byte iteration (lines 85, 90, 9a, a5 in original disassembly).  
**Cause:** SSE2 object compiled with `-mno-avx`, so `__builtin_popcount` couldn't emit `popcnt` instruction → falls back to `__popcountsi2` library call.  
**Fix:** Replaced with SWAR (SIMD Within A Register) popcount — pure arithmetic, fully inlined.  
**After:** ~20 ALU instructions per 64 bytes, zero function calls. Register `xmm2` no longer spilled to stack.

**Hot loop instruction sequence (64 bytes/iteration):**
```
movdqu  xmm1, [rcx+r9-0x40]   ; load 16 bytes
pcmpeqb xmm1, xmm0             ; compare
pmovmskb r11d, xmm1             ; extract mask
movdqu  xmm1, [rcx+r9-0x30]   ; load next 16 bytes
shr     ebx, 1                  ; SWAR popcount interleaved
and     ebx, 0x55555555         ; with next vector load
pcmpeqb xmm1, xmm0             ; ... pipelined beautifully
```

### 2. AVX2 count_byte — GOOD ✅
**No issues found.** Hardware `popcnt` inlines directly. 128 bytes per iteration (4x vpcmpeqb + 4x vpmovmskb + 4x popcnt + add).  
**Throughput:** ~104 GiB/s at 1MB (L2 cache).

**Hot loop instruction sequence (128 bytes/iteration):**
```
vpcmpeqb ymm0, ymm2, [rcx+r8-0x80]   ; compare 32 bytes
vpmovmskb r11d, ymm0                    ; extract mask
popcnt   r11d, r11d                      ; count bits
vpcmpeqb ymm0, ymm2, [rcx+r8-0x60]   ; next 32 bytes
...
add     eax, r11d                        ; accumulate
```

### 3. Scalar count_byte — Auto-vectorized by compiler
**Finding:** GCC auto-vectorized the SWAR scalar loop into SSE2 code!  
**Issue:** The auto-vectorized version uses a very inefficient byte→word→dword expansion via `punpcklbw`/`punpcklwd` (~30+ instructions per 16 bytes) instead of our hand-written pcmpeqb+pmovmskb+popcount (~5 instructions).  
**Impact:** Low — the SSE2 path is always preferred when available. The scalar path only runs on non-x86/ARM platforms without SIMD.  
**Status:** Acceptable as fallback.

### 4. hex_decode_table — BUG FIXED ✅
**Finding:** The agent-generated lookup table incorrectly mapped `G-Z` (0x47-0x5A) and `g-z` (0x67-0x7A) to values 16-35 instead of -1.  
**Impact:** `%GG` would be decoded as a valid byte instead of being passed through.  
**Fix:** Table now only maps `0-9`, `A-F`, `a-f` to valid values; everything else is -1.

### 5. AVX2 lowercase/uppercase — XOR case flip ✅
**Assembly confirms** branchless XOR pattern: `cmpgt_epi8` → `and` → `xor` with `0x20` mask. No branches in the hot path.

### 6. Pattern scanner — SIMD first-byte scan ✅
SSE2 and AVX2 paths use vectorized first-byte scan, then scalar verification.  
**Optimization opportunity:** Pre-compute compact (offset, byte) pairs for non-wildcard positions to avoid `pat_mask[j] != 0x00` check in verification loop.

## Summary of Changes Applied

| File | Change | Impact |
|------|--------|--------|
| `simd_sse2.cpp` | SWAR popcount replaces `__builtin_popcount` | Eliminates 4 function calls per 64B iteration |
| `simd_sse2.cpp` | Added `#pragma GCC target("sse2")` + `no-tree-vectorize` | Prevents auto-vectorization interference |
| `simd_scalar.cpp` | Removed `no-avx512*` pragma flags | Fixes memcpy inlining failure |
| `simd_avx2.cpp` | Simplified pragma to `avx2,bmi,popcnt,lzcnt` | Fixes build with GCC 13 |
| `url.cpp` | Fixed hex_decode_table (only 0-9, A-F, a-f valid) | Bug fix: %GG no longer decoded as valid |

# simdtext Performance Audit: Comparison with Industry Libraries

**Date:** 2026-05-02  
**Auditor:** C++ Performance Agent  
**simdtext version:** current HEAD

---

## Executive Summary

simdtext has a solid SIMD foundation with multi-ISA dispatch (scalar/SWAR → SSE2 → AVX2 → AVX-512 → NEON → Highway), good loop unrolling, and correct implementations. However, it falls behind industry leaders in several critical areas:

1. **UTF-8 validation is scalar-within-SIMD** — the biggest performance gap vs. simdutf
2. **Base64 encode/decode is entirely scalar** — no SIMD at all vs. aklomp/base64
3. **URL encode/decode is entirely scalar** — missing SIMD acceleration
4. **Highway backend lacks unrolling and accumulation optimizations** — leaves performance on the table
5. **SWAR patterns are good but could be tighter** — minor improvements possible

Estimated performance gaps on typical ASCII-heavy workloads:
| Operation | simdtext | Best-in-class | Gap |
|-----------|----------|---------------|-----|
| UTF-8 validate | ~2-4 GB/s | ~12+ GB/s (simdutf) | 3-6× |
| Base64 encode | scalar (~1 GB/s) | ~30+ GB/s (aklomp AVX2) | 30× |
| Base64 decode | scalar (~1 GB/s) | ~20+ GB/s (aklomp AVX2) | 20× |
| URL encode | scalar (~0.5 GB/s) | ~5+ GB/s (SIMD) | 10× |
| count_byte | ~20+ GB/s | ~20+ GB/s | ~1× |
| is_ascii | ~30+ GB/s | ~30+ GB/s | ~1× |

---

## 1. UTF-8 Validation — The Biggest Gap

### simdtext Approach

simdtext's `validate_utf8` in both SSE2 and AVX2 paths uses a **scalar-within-SIMD-loop** strategy:

```
for each 32-byte chunk:
  if all ASCII → skip (fast path)
  else → process byte-by-byte within chunk (slow path)
```

The slow path processes each byte individually with branches:
```cpp
while (cp < chunk_end) {
    const auto byte = *cp++;
    if (byte <= 0x7F) { ... }
    else if ((byte & 0xE0) == 0xC0) { ... }
    else if ((byte & 0xF0) == 0xE0) { ... }
    else if ((byte & 0xF8) == 0xF0) { ... }
    else if ((byte & 0xC0) == 0x80) { ... }
}
```

This is essentially a scalar UTF-8 validator running inside a SIMD loop. For any input containing non-ASCII (CJK text, emoji, etc.), performance drops to scalar speeds.

### simdutf Approach — Lookup-Based Fully SIMD

simdutf uses the **lookup4 algorithm** (from John Keiser's work), which is fully SIMD:

1. **`check_special_cases(prev1, input)`** — uses `pshufb`/`tbl` lookup tables to classify byte pairs:
   - Creates bitmasks for: TOO_SHORT, TOO_LONG, OVERLONG_2, OVERLONG_3, SURROGATE, TWO_CONTS, TOO_LARGE
   - High nibble of previous byte → lookup → byte_1_high
   - Low nibble of previous byte → lookup → byte_1_low
   - High nibble of current byte → lookup → byte_2_high
   - AND all three → per-byte error flags

2. **`check_multibyte_lengths(input, prev_input, sc)`** — validates multi-byte sequence lengths using `prev<2>` and `prev<3>` cross-chunk alignment

3. **Accumulating checker** — ORs errors across chunks, only checks at the end:
   ```cpp
   struct utf8_checker {
       simd8<uint8_t> error;
       void check_utf8_bytes(input, prev_input) {
           this->error |= check_multibyte_lengths(...);
       }
       bool errors() { return !this->error.any_bits_set_anywhere(); }
   };
   ```

**Key advantages of simdutf's approach:**
- **Zero branches in hot path** — pure SIMD operations
- **Processes 64 bytes at a time** (simd8x64)
- **Cross-chunk alignment** with `prev<1>`, `prev<2>`, `prev<3>` handles multi-byte sequences spanning chunks
- **Deferred error checking** — accumulate errors, check once at end
- **Handles all edge cases in SIMD** — overlong, surrogates, too-large codepoints

### Recommendation

**Implement the lookup4 algorithm.** This is the single highest-impact optimization available. The approach:

1. Create 3 lookup tables (byte_1_high, byte_1_low, byte_2_high) as `alignas(64) static const uint8_t[]`
2. Use `pshufb` (SSE2/AVX2) / `tbl` (NEON) / Highway `TableLookupBytes`
3. Implement `prev<N>()` for cross-chunk alignment (shift in bytes from previous chunk)
4. Accumulate error vector, check once at end
5. Keep the ASCII fast-path (skip validation for all-ASCII chunks)

For Highway, this maps directly to `hn::TableLookupBytes()` + `hn::ShiftLeftBytes()` / `hn::CombineShiftRightBytes()`.

---

## 2. Base64 Encode/Decode — No SIMD At All

### simdtext Approach

Both `base64_encode_to` and `base64_decode_to` are **pure scalar** with lookup tables:

```cpp
// Encode: 3 bytes → 4 chars, one triplet at a time
for (; i + 2 < input.size(); i += 3) {
    const auto n = (src[i] << 16) | (src[i+1] << 8) | src[i+2];
    dst[j++] = base64_chars[(n >> 18) & 0x3F];
    dst[j++] = base64_chars[(n >> 12) & 0x3F];
    dst[j++] = base64_chars[(n >> 6) & 0x3F];
    dst[j++] = base64_chars[n & 0x3F];
}
```

### aklomp/base64 Approach — Full SIMD

aklomp/base64 has optimized SIMD paths for SSE2, AVX2, AVX-512, NEON, and even WASM SIMD.

**AVX2 encode** uses `enc_reshuffle()` + `enc_translate()`:
- Load 24 bytes of input (produces 32 bytes of base64)
- Reshuffle: pack 3-byte groups into 4×6-bit groups using SIMD shuffles
- Translate: lookup base64 alphabet using `pshufb`
- Store 32 bytes of output
- Processes 24→32 bytes per iteration at ~30 GB/s

**AVX2 decode** uses nibble-based lookup:
- Two lookup tables (`lut_lo`, `lut_hi`) classify valid base64 characters via `pshufb`
- `lut_roll` table maps characters to delta values
- Add delta to input → reverse the base64 encoding
- Reshuffle packed 12-byte output from 16-byte input
- Validates input in-SIMD (early exit on invalid chars via `_mm256_testz_si256`)

### simdutf Base64 Approach

simdutf also has SIMD Base64 with a `block64` abstraction:
- Process 64 base64 chars → 48 output bytes per iteration
- Uses `to_base64_mask` for validation + compression
- Handles garbage characters with `compress_block`
- Supports both standard and URL-safe base64

### Recommendation

**Implement SIMD Base64.** Two options:

**Option A (Recommended): Port aklomp/base64's approach** — it's simpler, battle-tested, and well-documented. The reshuffle+translate pattern maps well to Highway's `TableLookupBytes` + `Shuffle`.

**Option B: Port simdutf's block64 approach** — more complex but handles edge cases like embedded garbage.

For Highway implementation:
```cpp
// Encode reshuffle: pack 3 bytes → 4 base64 indices
// Use hn::Shuffle2301 + hn::TableLookupBytes for byte rearrangement
// Use hn::TableLookupBytes with base64 alphabet for final translation

// Decode: nibble-based lookup with validation
// Use hn::And(hn::ShiftRight<4>, mask_0F) for high nibbles
// Use hn::And(input, mask_0F) for low nibbles
// Two pshufb lookups for classification + delta
```

---

## 3. URL Encode — Missing SIMD

### simdtext Approach

Pure scalar with lookup table:
```cpp
for (size_t i = 0; i < input.size(); ++i) {
    const uint8_t uc = src[i];
    if (url_safe_table[uc]) {
        dst[j++] = static_cast<char>(uc);  // copy safe byte
    } else {
        dst[j++] = '%';
        dst[j++] = url_hex[uc >> 4];
        dst[j++] = url_hex[uc & 0x0F];     // percent-encode
    }
}
```

### How SIMD Can Accelerate This

URL encoding has a data-dependent output size (1 or 3 bytes per input byte), making full SIMD challenging. However, several techniques exist:

1. **SIMD classification** — identify all URL-safe bytes in a vector, then:
   - If all safe → memcpy the whole vector (fast path for ASCII text)
   - If any unsafe → fall back to scalar for that chunk

2. **Two-pass approach** (used by optimized implementations):
   - Pass 1: SIMD scan to identify safe/unsafe boundaries
   - Pass 2: memcpy safe runs + percent-encode unsafe bytes

3. **ada-url approach** — uses SIMD for URL parsing (finding `:`, `/`, `?`, `#` delimiters), not for percent-encoding. Their speed comes from avoiding allocations and using offset-based component storage.

### Recommendation

**Add SIMD fast path for URL encode:**
```cpp
// Fast path: if entire vector is URL-safe, memcpy
for (; i + N <= size; i += N) {
    auto v = LoadU(d, ptr + i);
    auto safe = TableLookupBytes(url_safe_lut, v);  // 0x00=unsafe, 0xFF=safe
    if (AllTrue(d, Eq(safe, Set(d, 0xFF)))) {
        memcpy(dst + j, ptr + i, N);
        j += N;
    } else {
        // Scalar fallback for this chunk
        for (size_t k = 0; k < N; ++k) { /* ... */ }
    }
}
```

For ASCII-heavy URLs (the common case), this avoids per-byte branching entirely.

---

## 4. Highway Backend — Missing Optimizations

### Current Highway Implementation

The Highway backend (`simd_hwy.cpp`) is **minimal** — it's a basic port that processes one vector width at a time with no unrolling:

```cpp
template <class D>
size_t count_byte_vec(D d, const uint8_t* ptr, size_t size, uint8_t byte) {
    const auto vbyte = hn::Set(d, byte);
    const size_t N = hn::Lanes(d);
    size_t count = 0;
    size_t i = 0;
    for (; i + N <= size; i += N) {
        count += hn::CountTrue(d, hn::Eq(hn::LoadU(d, ptr + i), vbyte));
    }
    // ...
}
```

### Missing Optimizations vs. Native Backends

| Optimization | Native AVX2 | Highway | Impact |
|-------------|-------------|---------|--------|
| 4× loop unrolling | ✅ (128 bytes/iter) | ❌ (N bytes/iter) | ~30-40% throughput |
| OR-accumulation for is_ascii | ✅ (4 vectors → 1 branch) | ❌ (1 branch/vector) | ~20-30% throughput |
| CountTrue accumulation | ❌ (per-vector popcount) | ✅ (CountTrue) | Highway slightly better |
| Non-temporal stores | ✅ (>2MB) | ❌ | Matters for large buffers |
| Tail handling | SSE2 fallback | Scalar | Similar |

### Recommendation

**Add 4× unrolling to Highway backend:**

```cpp
template <class D>
bool is_ascii_vec(D d, const uint8_t* HWY_RESTRICT ptr, size_t size) {
    const size_t N = hn::Lanes(d);
    const size_t N4 = N * 4;
    size_t i = 0;
    for (; i + N4 <= size; i += N4) {
        auto v0 = hn::LoadU(d, ptr + i);
        auto v1 = hn::LoadU(d, ptr + i + N);
        auto v2 = hn::LoadU(d, ptr + i + 2 * N);
        auto v3 = hn::LoadU(d, ptr + i + 3 * N);
        auto ored = hn::Or(hn::Or(v0, v1), hn::Or(v2, v3));
        // Check high bits once instead of 4 times
        if (!hn::AllTrue(d, hn::Eq(hn::And(ored, hn::Set(d, uint8_t(0x80))),
                                     hn::Zero(d)))) {
            return false;
        }
    }
    // ... remaining vectors and tail
}
```

Also, `is_ascii_vec` currently uses `Ne(high, Zero)` instead of the simpler:
```cpp
// Better: just check if any byte has bit 7 set
if (hn::AnyTrue(d, hn::Lt(v, hn::Zero(d)))) return false;
// Or even simpler with Highway's built-in:
// hn::FindFirstTrue(d, hn::Lt(v, hn::Zero(d)))
```

---

## 5. SWAR Techniques — Good but Minor Improvements

### Current SWAR (scalar) Implementation

simdtext's SWAR is solid:
- `swar_count_byte`: XOR + zero-byte detection → popcount — **correct and efficient**
- `swar_is_ascii`: AND with 0x8080808080808080 — **optimal**
- `swar_range_mask`: subtraction-based range check — **correct**

### Minor Issues

1. **`swar_range_mask` for case conversion is overly complex** — the `(mask >> 2) & bit5` pattern works but requires shifting the range mask. A simpler approach:

   ```cpp
   // Instead of: mask = swar_range_mask(v, 'A', 'Z'); flipped = v ^ ((mask >> 2) & bit5);
   // Direct: detect uppercase and create the 0x20 mask in one step
   uint64_t upper = v & 0x4040404040404040ULL;  // bit 6 set = alpha
   uint64_t lower_check = (v + 0x1F1F1F1F1F1F1F1FULL) & 0x4040404040404040ULL;  
   // This isn't simpler. The current approach is fine.
   ```

   Actually, the current approach is already good. No change needed.

2. **`swar_count_byte` popcount** — using `__builtin_popcountll(zero_mask >> 7)` is correct but could be marginally faster with a direct table:
   
   ```cpp
   // Alternative: count the 0x80 bits directly
   return __builtin_popcountll(zero_mask) >> 3;  // Each match has exactly one 0x80 bit
   ```
   Wait — `zero_mask` has 0x80 per matching byte, so `popcountll(zero_mask) >> 3` gives the count. But `popcountll(zero_mask >> 7)` is equivalent. No meaningful difference.

3. **SWAR `find_byte` uses `__builtin_ctzll(zero_mask) / 8`** — the division by 8 can be replaced with a shift:
   ```cpp
   return data + i + (__builtin_ctzll(zero_mask) >> 3);
   ```
   Marginal, but free.

### Recommendation

The SWAR implementations are already well-done. Only minor cleanups available. No significant performance impact.

---

## 6. NEON Backend — Missing UTF-8 and count_code_points

### Current State

The NEON backend (`simd_neon.cpp`) implements: `count_byte`, `is_ascii`, `lowercase_ascii`, `uppercase_ascii`, and a partial `find_byte`. It's **missing**:
- `validate_utf8`
- `count_code_points`

The dispatch layer falls back to scalar for these on ARM. This is a significant gap for ARM server workloads (AWS Graviton, Apple Silicon).

### Recommendation

**Port count_code_points to NEON** — it's straightforward:
```cpp
size_t count_code_points(const char* data, size_t size) {
    const uint8x16_t v80 = vdupq_n_u8(0x80);
    const uint8x16_t vC0 = vdupq_n_u8(0xC0);
    size_t count = 0, i = 0;
    for (; i + 64 <= size; i += 64) {
        uint8x16_t c0 = vld1q_u8(data + i);
        // ... (4x unrolled)
        // Continuation byte: (byte & 0xC0) == 0x80
        uint8x16_t cont0 = vandq_u8(vcgeq_u8(c0, v80), vcltq_u8(c0, vC0));
        // Sum non-continuation = 16 - popcount(cont)
        // Use NEON pairwise add for counting
    }
}
```

**Port validate_utf8 to NEON** using the lookup4 approach with `vqtbl1q_u8`.

---

## 7. Dispatch Architecture — Compile-Time vs. Runtime

### Current Approach

simdtext uses a **hybrid** approach:
- Compile-time: `#if defined(__AVX512BW__)` gates AVX-512 code
- Runtime: `detect_cpu()` with function pointers (implicitly via if-else in dispatch.cpp)

### simdutf's Approach

simdutf uses a **fully runtime** dispatch with separate compilation units:
- Each ISA gets its own `.cpp` compiled with the appropriate `-march` flags
- A dispatch table selects implementation at first call
- This means the same binary works on any x86 CPU, detecting AVX2/AVX-512 at runtime

### Issue

simdtext's `#if defined(__AVX512BW__)` means the AVX-512 path is **compiled out** unless the whole library is built with AVX-512 flags. This defeats the purpose of runtime detection.

### Recommendation

**Adopt simdutf's multi-compilation-unit approach:**
- Compile `simd_avx512.cpp` with `-mavx512bw -mavx512f`
- Compile `simd_avx2.cpp` with `-mavx2 -mno-avx512f`
- Compile `simd_sse2.cpp` with `-msse2`
- Compile `simd_scalar.cpp` with no SIMD flags
- Runtime dispatch picks the best available path

This is already partially done (the `#pragma GCC target` directives), but the `#if defined(__AVX512BW__)` guard at file level still prevents compilation on machines without AVX-512 at build time.

---

## 8. Specific Code Quality Issues

### 8.1 SSE2 UTF-8: Dead Code

In `simd_sse2.cpp`, the `validate_utf8` function has ~30 lines of dead SIMD code that's never executed:

```cpp
// Classify using range comparisons
__m128i hi_nib = ...;
__m128i lo_nib = ...;
// ... lots of comparisons ...
(void)chunk; (void)hi_nib; (void)lo_nib; (void)v7f; // cast-to-void silencing
// Falls through to scalar anyway
```

This is clearly a work-in-progress that should be cleaned up or replaced with the lookup4 algorithm.

### 8.2 NEON find_byte: Incomplete

The NEON `find_byte` implementation appears truncated (file ends mid-comment). It likely doesn't compile or has a bug.

### 8.3 Duplicated Lookup Tables

`hex_decode_table` is defined identically in both `encode.cpp` and `url.cpp` with a comment "duplicated for TU locality." This is fine for performance but should be documented or placed in a shared internal header.

### 8.4 Non-Temporal Stores Without SFENCE

The AVX2 `lowercase_ascii`/`uppercase_ascii` use `_mm256_stream_si256` for buffers >2MB but don't issue `_mm_sfence()` after the loop. This can cause correctness issues if the buffer is subsequently read by another thread or via non-streaming operations.

---

## 9. Priority-Ordered Optimization Roadmap

| Priority | Optimization | Expected Speedup | Effort |
|----------|-------------|-----------------|--------|
| 🔴 P0 | SIMD UTF-8 validation (lookup4) | 3-6× on non-ASCII | High |
| 🔴 P0 | SIMD Base64 encode/decode | 10-30× | Medium |
| 🟡 P1 | NEON count_code_points + validate_utf8 | 2-4× on ARM | Medium |
| 🟡 P1 | Highway 4× unrolling | 20-40% | Low |
| 🟡 P1 | SIMD URL encode fast-path | 2-5× on ASCII | Medium |
| 🟢 P2 | Fix AVX-512 compile-time gating | Enables AVX-512 | Low |
| 🟢 P2 | Clean up SSE2 dead code | Maintainability | Low |
| 🟢 P2 | SFENCE after non-temporal stores | Correctness | Trivial |
| ⚪ P3 | SWAR find_byte: `/8` → `>>3` | Cosmetic | Trivial |

---

## 10. Key Techniques to Adopt from Each Library

### From simdutf
- ✅ **Lookup4 UTF-8 validation** — `pshufb`/`tbl` based byte-pair classification
- ✅ **Deferred error accumulation** — OR error vectors, check once at end
- ✅ **Cross-chunk prev<N>() alignment** — handle multi-byte sequences spanning chunks
- ✅ **64-byte block processing** — `simd8x64` processes 64 bytes per iteration
- ✅ **Bytemask counting** for `count_code_points` — accumulates counters in SIMD registers, avoids per-vector horizontal reduction

### From aklomp/base64
- ✅ **Reshuffle + translate** pattern for Base64 encode — SIMD byte rearrangement
- ✅ **Nibble-based lookup** for Base64 decode — `pshufb` with classification tables
- ✅ **Delta-based decode** — add correction values instead of full lookup
- ✅ **In-SIMD validation** — `_mm256_testz_si256` for early exit on invalid input

### From Google Highway
- ✅ **`IfThenElse` for case conversion** — cleaner than AND/XOR pattern (already used)
- ✅ **`CountTrue` vs movemask+popcount** — Highway's approach is more portable
- ⚠️ **But unrolling still needed** — Highway doesn't auto-unroll; manual unrolling is required

### From ada-url
- ✅ **Offset-based component storage** — avoids allocations (for URL parsing, not encoding)
- ✅ **SIMD delimiter search** — finding `:`, `/`, `?`, `#`, `@` in URLs

### From libhat
- ✅ **Pattern scanning with SIMD** — techniques applicable to `find_byte` and `contains_char`
- ✅ **AVX-512 masked load for tail** — already used in simdtext's AVX-512 path ✅

---

## Appendix A: simdutf count_code_points Bytemask Technique

simdutf has an alternative `count_code_points_bytemask` that's interesting:

```cpp
// Instead of: count += N - popcount(mask) per vector
// Do: accumulate continuation counters in SIMD registers
auto local = vector_u8::zero();
for (; pos + 4*N <= size; pos += 4*N) {
    local -= vector_u8(mask0);  // each continuation byte subtracts 1
    local -= vector_u8(mask1);
    local -= vector_u8(mask2);
    local -= vector_u8(mask3);
    // Periodically reduce to avoid u8 overflow (255/4 = 63 iterations)
    if (iterations == max_iterations) {
        counters += sum_8bytes(local);
        local = vector_u8::zero();
    }
}
```

This avoids per-vector `popcount` / `CountTrue` calls, replacing them with SIMD subtract + periodic horizontal reduction. Could be worth porting for very large inputs.

---

*End of audit report.*

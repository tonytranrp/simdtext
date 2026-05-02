# Research: SIMD Optimization Techniques for simdtext v2
## Compiled 2026-05-02

### Sources
- **simdutf** (github.com/simdutf/simdutf) — UTF-8 validation, encoding conversion
- **aklomp/base64** (github.com/aklomp/base64) — AVX-512 Base64 encoding
- **simdjson** (arXiv:1902.08318) — JSON parsing with SIMD
- **libhat** (github.com/mrexodia/libhat) — Pattern scanning with SIMD
- **Google Highway** (github.com/google/highway) — Portable SIMD

---

## 1. UTF-8 Validation: Lookup-Table Algorithm (simdutf)

### The Algorithm (from `src/generic/utf8_validation/utf8_lookup4_algorithm.h`)

simdutf uses a **lookup-table approach** based on John Regehr's algorithm:
1. Process 64 bytes at a time (`simd8x64<uint8_t>`)
2. ASCII fast path: if all 64 bytes are ASCII, skip detailed check (just check prev_incomplete)
3. Non-ASCII path: split into 2 or 4 chunks (16/32 bytes each), check sequentially
4. For each chunk, use `check_special_cases()` with THREE lookup tables:
   - `byte_1_high`: lookup by (prev1 >> 4) — classifies lead byte type
   - `byte_1_low`: lookup by (prev1 & 0x0F) — fine-grained classification
   - `byte_2_high`: lookup by (input >> 4) — classifies current byte type
5. AND the three lookup results together → error bits per lane
6. Check multibyte lengths using `prev<2>()` and `prev<3>()` shifts
7. OR all error accumulators; check at end

### Key Error Bits
```cpp
TOO_SHORT  = 1<<0  // Lead byte followed by lead byte/ASCII
TOO_LONG   = 1<<1  // ASCII followed by continuation
OVERLONG_3 = 1<<2  // E0 80-9F (overlong 3-byte)
TOO_LARGE  = 1<<3  // F4 90+ (beyond U+10FFFF)
SURROGATE  = 1<<4  // ED A0-BF (UTF-16 surrogate)
OVERLONG_2 = 1<<5  // C0-C1 80-BF (overlong 2-byte)
OVERLONG_4 = 1<<6  // F0 80-8F (overlong 4-byte)
TWO_CONTS  = 1<<7  // Continuation followed by continuation
```

### Highway Implementation Strategy
- Use `TableLookupBytes()` for `lookup_16()` (maps to `pshufb`/`vtbl`)
- Use `Prev<1>()`, `Prev<2>()`, `Prev<3>()` for inter-lane carry
- Use `Shr<4>()` for high nibble extraction
- Use `And()` for nibble masking
- Process N bytes per vector (32 for AVX2, 16 for SSE2, 16 for NEON)

### Expected Performance
- ASCII: ~14 GB/s (already have this with is_ascii_vec)
- Mixed UTF-8: ~10-12 GB/s (simdutf achieves this)
- Our current scalar: ~0.5-1 GB/s (10-20× slower!)

---

## 2. Base64 Encoding: AVX-512 Multishift Technique (aklomp/base64)

### The Algorithm
Instead of traditional bit-shift + mask approach:
1. **Permutexvar**: Reorder bytes within 512-bit register to prepare for grouping
2. **Multishift**: `_mm512_multishift_epi64_epi8()` performs per-byte shifts in a single instruction
   - Shift amounts: `[48, 54, 36, 42, 16, 22, 4, 10]` (per 8 bytes in a 64-bit lane)
   - This extracts all 6-bit groups simultaneously!
3. **Permutexvar**: Use the 6-bit values as shuffle indices into a 64-byte lookup table

### Key Insight
`multishift_epi64_epi8` is unique to AVX-512 and allows:
- **Single-instruction bit extraction** — no separate shift+mask+or sequence
- **No intermediate registers** — direct from input to 6-bit groups
- **Combined reshuffle+translate** — fewer instructions overall

### For Highway
Highway doesn't have `multishift` yet, so for AVX-512 we'd need:
- Fallback to shift+mask+or sequence for SSE2/AVX2/NEON
- Use `TableLookupBytes()` for the final Base64 table lookup
- Consider `CombineShiftRightBytes` + `ShiftLeft` + `And` + `Or` sequences

### Expected Performance
- AVX-512: ~30 GB/s encoding, ~40 GB/s decoding
- AVX2: ~15 GB/s encoding, ~20 GB/s decoding
- Our current scalar: ~0.5 GB/s (30-80× slower!)

---

## 3. Pattern Scanning: libhat Techniques

### Key Techniques
- **AVX-512 VBMI**: Use `vpermb` for single-instruction pattern matching
- **Shuffle-based**: Use `pshufb` to compare against pattern at multiple offsets
- **Mask aggregation**: OR masks across offsets, then `FindFirstTrue`
- **Chunked processing**: Process 64 bytes at a time, carry state across chunks

### For simdtext
Our `pattern.hpp` could use:
- `TableLookupBytes()` for shuffle-based comparison
- Multiple offset comparison in parallel
- `FindFirstTrue` for fast first-match reporting

---

## 4. simdjson Paper Key Insights

From "Parsing Gigabytes of JSON per Second" (Lemire et al., VLDB 2019):
- **Structural characterization**: Use SIMD to identify structural characters (`{`, `}`, `:`, `,`) in a single pass
- **Backslash skipping**: Use `pshufb` to detect and skip escaped characters
- **Branchless validation**: Accumulate errors in a mask, check once at the end
- **64-byte blocks**: Process exactly 64 bytes per iteration (4×16 or 2×32)
- **Cached tables**: Pre-load lookup tables into registers, not memory

### For simdtext's JSON tokenizer
- Use `TableLookupBytes()` for structural character identification
- Branchless validation with error accumulation
- 64-byte block processing

---

## 5. Highway API Mapping for simdutf Techniques

| simdutf Operation | Highway Equivalent | Notes |
|---|---|---|
| `lookup_16<table>()` | `TableLookupBytes(vec, table)` | Maps to `pshufb`/`vtbl` |
| `prev<N>()` | `CombineShiftRightBytes` + `ShiftLeft` | Need manual implementation |
| `shr<4>()` | `Shr<4>()` | Direct equivalent |
| `simd8x64` (64 bytes) | Process 2-4 vectors | Highway uses per-ISA vector width |
| `any_bits_set_anywhere()` | `FindFirstTrue(d, mask) >= 0` | Or `!AllFalse(d, mask)` |
| `must_be_2_3_continuation()` | Custom: check prev2/prev3 for lead bytes | Uses `prev<2>` + `prev<3>` |

### Highway Prev Implementation
```cpp
template <int N, class D>
Vec<D> Prev(D d, Vec<D> v, Vec<D> prev) {
    // Shift right by N bytes, pulling from prev
    return CombineShiftRightBytes<N>(d, v, prev);
}
```

---

## 6. Optimization Priority Matrix

| # | Operation | Current | Target | Technique | Impact |
|---|-----------|---------|--------|-----------|--------|
| 1 | UTF-8 validate | Scalar ~1 GB/s | ~12 GB/s | Lookup-table (simdutf) | **CRITICAL** |
| 2 | Base64 encode | Scalar ~0.5 GB/s | ~15 GB/s | Shift+mask+lookup (AVX2) | **HIGH** |
| 3 | Base64 decode | Scalar ~0.5 GB/s | ~20 GB/s | Lookup+reshuffle | **HIGH** |
| 4 | Hex encode | Scalar ~1 GB/s | ~10 GB/s | Nibble extract+lookup | **MEDIUM** |
| 5 | JSON tokenize | Scalar ~1 GB/s | ~8 GB/s | Structural char detection | **MEDIUM** |
| 6 | Pattern scan | Scalar ~1 GB/s | ~8 GB/s | pshufb multi-offset | **MEDIUM** |
| 7 | URL decode | Scalar ~0.5 GB/s | ~6 GB/s | % detection + hex lookup | **LOW** |
| 8 | CSV parse | Scalar ~2 GB/s | ~10 GB/s | Comma/newline SIMD | **LOW** |

---

## 7. Implementation Plan

### Phase 1: UTF-8 Lookup-Table Validator (HIGHEST PRIORITY)
- Implement `check_special_cases()` using Highway `TableLookupBytes`
- Implement `check_multibyte_lengths()` using `Prev<1/2/3>()`
- Implement `utf8_checker` class with error accumulation
- Implement `buf_block_reader` for 64-byte aligned processing
- Wire into `validate_utf8_vec()` in `simd_hwy.cpp`
- Expected: 10-15× speedup on mixed UTF-8, 2× on ASCII

### Phase 2: Base64 SIMD Encoding/Decoding
- Implement shift+mask+or reshuffling using Highway
- Implement `TableLookupBytes` for Base64 alphabet lookup
- Implement `multishift` fallback for non-AVX-512
- Add AVX-512 specific path with `multishift_epi64_epi8`
- Expected: 20-30× speedup

### Phase 3: Hex Encode SIMD
- Implement nibble extraction + parallel lookup
- Two `TableLookupBytes` calls for upper/lower nibbles
- Expected: 5-10× speedup

### Phase 4: JSON/XML Tokenizer SIMD
- Structural character detection using `TableLookupBytes`
- Branchless validation with mask accumulation
- Expected: 5-8× speedup

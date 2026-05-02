# simdtext Optimization Research Report

**Date:** 2026-05-02
**Purpose:** Deep research on SIMD text processing optimization, comparing simdtext against industry leaders, and identifying specific improvements.

---

## 1. UTF-8 Validation — The Critical Gap

### Current simdtext approach
`src/detail/simd_scalar.cpp::validate_utf8` — sequential byte-by-byte state machine with `__attribute__((optimize("no-tree-vectorize")))`.

### How simdutf does it (10× faster)
`src/generic/utf8_validation/utf8_lookup4_algorithm.h`:
- **Lookup-table state machine** using `pshufb`/`tbl` instructions
- Two separate lookups: one for high nibble, one for low nibble of each byte
- **Inter-lane carry**: `prev_in<1>`, `prev_in<2>`, `prev_in<3>` for cross-chunk validation
- Processes 32 bytes (AVX2) or 64 bytes (AVX-512) per iteration
- Based on John Regehr's algorithm from "A Fast UTF-8 Validator with SIMD"

### How to implement in simdtext
```cpp
// Highway-based UTF-8 validation using lookup tables
template <class D>
bool validate_utf8_hwy(D d, const uint8_t* data, size_t size) {
    // Lookup tables for high/low nibble classification
    // These classify bytes as: continuation (0x80), 2-byte lead (0xC0-0xDF),
    // 3-byte lead (0xE0-0xEF), 4-byte lead (0xF0-0xF7), or invalid
    const uint8_t high_table[16] = { /* ... */ };
    const uint8_t low_table[16] = { /* ... */ };
    
    // For each vector chunk:
    // 1. Split into high/low nibbles
    // 2. Lookup in tables using pshufb/TBL
    // 3. Check for invalid combinations
    // 4. Track expected continuation bytes across chunks
}
```

### Priority: CRITICAL
This is simdtext's biggest performance gap. The scalar UTF-8 validator is the slowest path in the library.

---

## 2. Base64 Encoding — AVX-512 VBMI2 Opportunity

### Current simdtext approach
Not yet examined in detail — need to check `src/encode/encode.cpp`.

### How aklomp/base64 does it (AVX-512)
`lib/arch/avx512/enc_reshuffle_translate.c`:
- `_mm512_multishift_epi64_epi8` — single instruction to do 6-bit extraction from 8-bit values
- `_mm512_permutexvar_epi8` — VBMI2 2-register byte shuffle (no 16-byte lane limit!)
- Eliminates the lane-crossing penalty that plagues AVX2 Base64
- **~2× faster than AVX2** on Ice Lake+ processors

### How simdutf does it (AVX2)
`src/haswell/avx2_base64.cpp`:
- `pshufb`-based lookup tables for validation and value conversion
- `_mm256_mulhi_epu16` for bit extraction (clever: multiply by powers of 2 to extract specific bit ranges)
- `compress` function using thintable for decode
- Handles padding correctly with masked loads/stores

### How to improve simdtext
1. Add AVX-512 VBMI2 path using `multishift` + `permutexvar` 
2. Use `pshufb`-based LUT approach for AVX2 (current approach may be slower)
3. Use `_mm256_mulhi_epu16` trick for 6-bit extraction instead of shift/merge

---

## 3. Byte Counting — Already Good, Minor Optimizations

### Current simdtext approach
- Highway: `count_byte_vec` using `hn::CountTrue(hn::Eq(LoadU, vbyte))` — correct and fast
- SSE2: `_mm_cmpeq_epi8` + `_mm_movemask_epi8` + `_popcnt64` — correct
- AVX2: Same pattern with `_mm256_` prefix — correct
- Scalar SWAR: XOR + zero-byte detection + `__builtin_popcountll` — correct

### Optimization: Reduce horizontal sum frequency
Currently accumulating into a single count each iteration. Better approach (from simdutf):
```cpp
// Accumulate into 4 separate counters for better ILP
size_t c0 = 0, c1 = 0, c2 = 0, c3 = 0;
for (; i + 4*N <= size; i += 4*N) {
    c0 += CountTrue(d, Eq(LoadU(d, ptr + i),     vbyte));
    c1 += CountTrue(d, Eq(LoadU(d, ptr + i + N),  vbyte));
    c2 += CountTrue(d, Eq(LoadU(d, ptr + i + 2*N),vbyte));
    c3 += CountTrue(d, Eq(LoadU(d, ptr + i + 3*N),vbyte));
}
return c0 + c1 + c2 + c3;
```
This eliminates the dependency chain on the accumulator and allows superscalar execution.

---

## 4. ASCII Case Conversion — Minor Improvement

### Current simdtext approach (SWAR)
`src/detail/simd_scalar.cpp::lowercase_ascii` — SWAR range mask + XOR with 0x20 bit. Correct.

### How simdutf does it
Uses `pshufb`-based lookup table for case conversion on AVX2+. This is sometimes faster because:
- Single `pshufb` per vector (no compare + blend)
- But needs a 32-byte lookup table in a register (2× `pshufb` on AVX2 since lanes)

### Recommendation
Keep current approach for Highway/SIMD (compare + OR 0x20). It's simpler and equally fast.
The SWAR approach is already optimal for scalar.

---

## 5. Pattern Scanning — Learn from libhat

### libhat key techniques
`src/Scanner.cpp` + `arch/x86/`:
- **Frequency-based pair selection**: Pre-computed score table for which byte pair to use as the primary comparison key (based on x86_64 instruction frequency analysis!)
- **AVX-512 path**: Uses `vpcmpb` + `kand` mask operations for pattern matching
- **AVX2 path**: Uses `pcmpeqb` + `pmovmskb` for byte comparison
- **Auto-resolve scanner**: Runtime dispatch based on CPU features (BMI, AVX2, AVX-512BW)
- **Alignment-aware**: Different scanning strategies for aligned vs unaligned patterns

### What simdtext should adopt
1. **Pair-based scanning**: Instead of comparing one byte at a time, find the best byte pair in the pattern and use it as the primary match criterion
2. **Mask-based AVX-512**: Use `vpcmpb` + `kand` instead of `pcmpeqb` + `pmovmskb`
3. **Score table**: Pre-compute which byte pair gives best selectivity

---

## 6. SWAR Techniques — Already Solid, One Enhancement

### Current simdtext SWAR
- `swar_count_byte`: XOR + zero-byte detection — correct, standard
- `swar_is_ascii`: High-bit mask check — correct
- `swar_range_mask`: Subtraction-based range check — correct and clever

### Enhancement: SWAR newline counting
For `count_newlines`, consider counting both `\n` and `\r\n` in one pass:
```cpp
// Count \n, subtract \r\n sequences
size_t nl = swar_count_byte(data, size, '\n');
size_t cr = swar_count_byte(data, size, '\r');
// Adjust: if \r is followed by \n, it's one line ending not two
// For raw count_newlines, just count \n (Unix standard)
```
Current approach is fine for `\n`-only counting.

---

## 7. Memory-Mapped File I/O — Enhancement Opportunities

### Current approach
`src/file/file.cpp` — uses `mmap`/`munmap` directly.

### Improvements
1. **Huge pages**: `mmap` with `MAP_HUGETLB` for files > 2MB — reduces TLB misses
2. **Prefetch**: `_mm_prefetch` with `_MM_HINT_T0` for sequential scanning
3. **Parallel chunked scanning**: Split file into cache-line-aligned chunks, process with threads
4. **Read-ahead**: `madvise(MADV_SEQUENTIAL)` to tell kernel to aggressive read-ahead
5. **Non-fault page check**: `mincore()` to check if pages are resident before processing

---

## 8. Highway Best Practices (from Highway docs and source)

### Runtime dispatch pattern
```cpp
// Correct Highway pattern for simdtext:
// 1. Declare functions with HWY_DECLARE in header
// 2. Define with HWY_ATTR in .cpp
// 3. Export with HWY_EXPORT
// 4. Call through HWY_DYNAMIC_DISPATCH

// In simd_hwy.cpp:
HWY_ATTR void CountByte(const uint8_t* data, size_t size, uint8_t byte, size_t* result) {
    // Implementation using HWY_NAMESPACE ops
}

HWY_EXPORT(CountByte);

// Public API:
size_t count_byte_hwy(const uint8_t* data, size_t size, uint8_t byte) {
    return HWY_DYNAMIC_DISPATCH(CountByte)(data, size, byte);
}
```

### Key Highway ops for text processing
- `hn::Eq` + `hn::CountTrue` — byte counting
- `hn::LoadU` / `hn::StoreU` — unaligned memory access
- `hn::TableLookupBytes` — `pshufb`/`tbl` for LUT-based operations
- `hn::IfThenElse` — conditional blend
- `hn::ConcatUpperLower` / `hn::ConcatLowerUpper` — cross-lane operations
- `hn::Prev` — inter-lane carry (critical for UTF-8 validation!)

---

## 9. Academic Papers & Key References

| Paper/Source | Key Insight | Relevance |
|---|---|---|
| "Parsing Gigabytes of JSON per Second" (Lemire, 2019) | SIMD JSON parsing with bit manipulation | General SIMD text patterns |
| "A Fast UTF-8 Validator with SIMD" (Regehr, 2020) | Lookup-table UTF-8 validation with pshufb | UTF-8 validation — critical |
| "Scanning HTML at Tens of GB/s" (Mousse, 2023) | Nibble-based classification with pshufb | All text scanning operations |
| Google Highway documentation | Portable SIMD best practices, runtime dispatch | All Highway usage |
| aklomp/base64 source | multishift + permutexvar for AVX-512 Base64 | Base64 optimization |
| libhat source | Frequency-based pair selection for pattern scanning | Pattern scanning |
| simdutf source | Inter-lane carry for cross-chunk validation, thintable | UTF-8, Base64 |

---

## 10. Priority Action Items

### CRITICAL (do first)
1. **Rewrite UTF-8 validation** — replace scalar state machine with simdutf-style lookup-table approach using Highway `TableLookupBytes` + `Prev` for inter-lane carry
2. **Add ILP-friendly counting** — 4× unrolled accumulation in count_byte/count_newlines

### HIGH
3. **Add AVX-512 VBMI2 Base64** — `multishift` + `permutexvar` path
4. **Improve pattern scanning** — libhat-style pair-based scanning with score tables
5. **Huge page mmap** — for FileScanner with files > 2MB

### MEDIUM
6. **madvise(MADV_SEQUENTIAL)** — for file scanning hints to kernel
7. **Parallel file scanning** — thread pool for chunked processing
8. **pshufb-based Base64 LUT** — for AVX2 encode/decode (replace current approach)
9. **Aligned load optimization** — use `Load` (not `LoadU`) when data is known-aligned

### LOW
10. **mincore pre-check** — for file scanning to avoid page faults in hot path
11. **SWAR \r\n counting** — optional Windows-style line ending support
12. **Benchmark on ARM NEON** — verify NEON path performance

---

## Sources

- simdutf: https://github.com/simdutf/simdutf
- Google Highway: https://github.com/google/highway
- aklomp/base64: https://github.com/aklomp/base64
- libhat: https://github.com/BasedInc/libhat
- fast_float: https://github.com/fastfloat/fast_float
- Ada URL: https://github.com/ada-url/ada
- Lemire blog: https://lemire.me/blog/
- Intel Intrinsics Guide: https://www.intel.com/content/www/us/en/docs/intrinsics-guide/

# simdtext Master Action Plan

**Compiled from:** 3 sub-agent audit reports + deep internet research
**Date:** 2026-05-02

---

## Phase 1: Critical Bug Fixes (DO NOW)

### 1.1 Fix NEON `find_byte` truncation
- **File:** `src/detail/simd_neon.cpp:108-121`
- **Issue:** Function body physically truncated, file ends mid-expression
- **Fix:** Complete the NEON `find_byte` implementation using `vceq_u8` + `vminv_u8` pattern

### 1.2 Fix SSE2 `__builtin_popcount` without popcnt target
- **File:** `src/detail/simd_sse2.cpp:383`
- **Issue:** `count_code_points` uses `__builtin_popcount` but file is compiled with only `sse2,ssse3`
- **Fix:** Replace with SWAR `popcount16` helper already used in `count_byte`, or add `popcnt` to pragma

### 1.3 Fix `Utf8Validator::validate()` accepting invalid UTF-8
- **File:** `src/scalar/scalar.cpp:~220`
- **Issue:** Streaming validator accepts overlong encodings and surrogates
- **Fix:** Add proper range checks for E0/A0-BF, ED/80-9F, F0/90-BF, F4/80-8F

### 1.4 Fix `replace_all` dead code (unused first `find_byte` call)
- **File:** `src/str/str.cpp:28`
- **Fix:** Remove the unused `find_byte` call on line 28

---

## Phase 2: Code Quality Cleanup (NEXT)

### 2.1 Unify dispatch architecture
- **Issue:** Two independent dispatch paths (`scalar.cpp` vs `simd_dispatch.cpp`)
- **Fix:** Make `scalar.cpp` call through `simd_dispatch.cpp` functions, or consolidate into one file

### 2.2 Remove dead SSE2 UTF-8 SIMD setup code
- **File:** `src/detail/simd_sse2.cpp:179-220`
- **Fix:** Remove ~40 lines of void-casted SIMD variables

### 2.3 Extract shared helpers
- `hex_decode_table` → `include/simdtext/detail/hex_table.hpp`
- `ctz32`/`ctz64`/`popcount` → `include/simdtext/detail/bitops.hpp`
- UTF-8 scalar validator → `include/simdtext/detail/utf8_scalar.hpp`

### 2.4 Remove dead `expected.hpp` polyfill
- Never used anywhere — delete or move to detail/

### 2.5 Fix naming inconsistencies
- Rename `simd_sse2.cpp` comment to reflect SSSE3 usage
- Unify `trim_ascii` vs `trim` whitespace definitions
- Remove `utf8_length` alias (keep `count_code_points` as primary)

---

## Phase 3: UTF-8 Validation Rewrite (HIGH PRIORITY OPTIMIZATION)

### 3.1 Implement simdutf-style lookup-table UTF-8 validator
- **Source:** simdutf's `utf8_lookup4_algorithm.h`
- **Technique:** High/low nibble lookup tables using `pshufb`/`tbl`, inter-lane carry with `Prev`
- **Highway implementation:** `hn::TableLookupBytes` + `hn::Prev<1/2/3>`
- **Expected speedup:** 5-15× over current scalar validator
- **Target:** SSE2, AVX2, AVX-512, NEON, Highway backends

### 3.2 Add AVX-512 and NEON `validate_utf8` / `count_code_points`
- Currently missing from both backends
- Implement via the new lookup-table approach

---

## Phase 4: Performance Optimizations

### 4.1 ILP-friendly counting (4× unrolled accumulation)
- `count_byte`, `count_newlines`: Use 4 separate counters for better ILP
- Expected improvement: 10-20% on large buffers

### 4.2 AVX-512 VBMI2 Base64
- `multishift` + `permutexvar` for Base64 encode
- Expected: ~2× over AVX2 on Ice Lake+

### 4.3 libhat-style pattern scanning
- Frequency-based pair selection
- AVX-512 `vpcmpb` + `kand` mask operations
- Score table for best matching byte pair

### 4.4 Huge page mmap for FileScanner
- `mmap` with `MAP_HUGETLB` for files > 2MB
- `madvise(MADV_SEQUENTIAL)` for read-ahead
- Reduces TLB misses on large files

### 4.5 `common_suffix_length` SIMD acceleration
- Currently pure scalar in `diff.cpp`
- Use same approach as `common_prefix_length`

---

## Phase 5: Documentation

### 5.1 Add `@param`/`@return` Doxygen to all public API functions
### 5.2 Write `docs/ARCHITECTURE.md` — dispatch model, build system, how to add backends
### 5.3 Add `@file`/`@brief` to missing headers
### 5.4 Document `BytePattern::parse()` failure modes
### 5.5 Add architecture diagram showing dispatch flow

---

## Phase 6: Testing

### 6.1 UTF-8 edge case tests
- Overlong encodings, surrogates, >U+10FFFF
- Streaming validator chunk boundary tests
- Empty input, single byte

### 6.2 Fuzz tests for csv/markdown/xml parsers
### 6.3 Backend-specific tests (SSE2/AVX2/AVX-512/NEON/HWY)
### 6.4 CI matrix with sanitizers (ASan, UBSan)

---

## Execution Order

1. ⚡ Phase 1 (Critical bugs) — 30 min
2. ⚡ Phase 2 (Code quality) — 1 hr
3. 🔥 Phase 3 (UTF-8 rewrite) — 2-3 hrs
4. 🚀 Phase 4 (Performance) — ongoing
5. 📝 Phase 5 (Documentation) — 1 hr
6. ✅ Phase 6 (Testing) — 1 hr

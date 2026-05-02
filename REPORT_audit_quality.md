# simdtext Code Quality & Redundancy Audit Report

**Date:** 2026-05-02  
**Auditor:** simd (subagent)  
**Scope:** All source files, headers, tests, benchmarks under `simdtext/` (excluding `build/`)

---

## Executive Summary

The codebase has significant code duplication across SIMD backends (SSE2, AVX2, AVX-512, NEON, HWY, scalar), a critical incomplete implementation in the NEON backend, dead code in the SSE2 UTF-8 validator, duplicated lookup tables, inconsistent dispatch architectures (two separate dispatch paths), missing API implementations in AVX-512 and NEON, and a compile-time bug where `__builtin_popcount` is used in an SSE2-only target. Naming inconsistencies exist between modules, and several documentation gaps were found.

**Critical Issues:** 3  
**High Severity:** 8  
**Medium Severity:** 10  
**Low Severity:** 7  

---

## 1. Code Duplication

### 1.1 ⚠️ CRITICAL — Dual dispatch architecture (scalar.cpp vs simd_dispatch.cpp)

**Files:** `src/scalar/scalar.cpp`, `src/detail/simd_dispatch.cpp`  
**Severity:** CRITICAL

The codebase has **two completely independent CPU dispatch paths** for the same functions:

- `src/scalar/scalar.cpp` dispatches `count_byte`, `is_ascii`, `lowercase_ascii_inplace`, `uppercase_ascii_inplace`, `find_byte` via direct calls to `detail::avx2::`, `detail::sse2::`, `detail::scalar::`
- `src/detail/simd_dispatch.cpp` dispatches the same functions via `*_dispatch` functions

Both paths detect CPU features and choose backends. `src/scalar/scalar.cpp` does NOT use the `*_dispatch` functions — it reimplements the dispatch logic. This means:
1. Any new backend (e.g., AVX-512) must be added to BOTH dispatch paths
2. The dispatch order/priority could diverge between the two
3. `simd_dispatch.cpp` supports AVX-512 but `scalar.cpp` does not

### 1.2 🔴 HIGH — Highway backend duplicates all scalar/SIMD functionality

**File:** `src/highway/simd_hwy.cpp`  
**Severity:** HIGH

The Highway (`HWY`) backend re-implements `count_byte`, `is_ascii`, `lowercase_ascii`, `uppercase_ascii`, `find_byte` using Google Highway SIMD — the exact same operations already implemented in SSE2/AVX2/AVX-512/NEON/scalar backends. This is a **fifth** implementation of the same functions.

When `SIMDTEXT_HAVE_HWY` is defined, the Highway implementations in `simd_hwy.cpp` are used; otherwise, the scalar fallbacks in the same file are used. But these Highway implementations compete with (and are redundant to) the entire `src/detail/` dispatch path. It's unclear which path is actually called at runtime.

### 1.3 🟡 MEDIUM — Duplicated `hex_decode_table`

**Files:** `src/encode/encode.cpp:7-24`, `src/url/url.cpp:34-51`  
**Severity:** MEDIUM

Both files define an identical `static constexpr std::array<int8_t, 256> hex_decode_table`. The comment in `url.cpp` says "same as encode.cpp, duplicated for TU locality" — this is intentional but still wasteful. Could be extracted to a shared internal header.

### 1.4 🟡 MEDIUM — Duplicated `ctz32` / `popcount` helpers

**Files:** `src/detail/simd_sse2.cpp:16-40`, `src/detail/simd_avx2.cpp:19-70`  
**Severity:** MEDIUM

Both SSE2 and AVX2 files define their own `ctz32()` helper with identical fallback implementations. AVX2 additionally has `ctz64()` and `popcount32()`/`popcount64()`. These could be in a shared `detail/bitops.hpp` header.

### 1.5 🟡 MEDIUM — Copy-pasted UTF-8 validation scalar loop

**Files:** `src/detail/simd_sse2.cpp:160-300`, `src/detail/simd_avx2.cpp:183-350`, `src/detail/simd_scalar.cpp:114-150`  
**Severity:** MEDIUM

The scalar UTF-8 validation loop (byte classification, continuation byte counting, range checks for overlong/surrogate/F4) is copy-pasted across all three x86 backends. The SSE2 and AVX2 "SIMD" UTF-8 validators actually fall back to byte-by-byte scalar processing within the SIMD loop for non-ASCII chunks. This ~30-line state machine should be extracted to a shared helper.

### 1.6 🟢 LOW — Duplicated `is_whitespace` / `is_ascii_whitespace`

**Files:** `src/str/str.cpp:10`, `src/scalar/scalar.cpp:89`  
**Severity:** LOW

Two different whitespace-checking functions:
- `str.cpp`: `is_whitespace()` — checks space, tab, newline, CR, formfeed, vertical tab
- `scalar.cpp`: `is_ascii_whitespace()` — checks only space, tab, CR, LF

They disagree on what counts as whitespace. `is_whitespace` includes `\f` and `\v`; `is_ascii_whitespace` does not. This is a functional inconsistency, not just duplication.

---

## 2. Dead Code

### 2.1 🔴 HIGH — SSE2 UTF-8 validator: 80+ lines of dead SIMD code

**File:** `src/detail/simd_sse2.cpp:179-220`  
**Severity:** HIGH

The SSE2 `validate_utf8()` function declares a 256-entry `class_table`, loads `class_lookup` and `class_lookup_hi`, creates comparison vectors (`vcont`, `vlead2`, `vlead3`, `vlead4`, `vinvalid`), computes `hi_nib`, `lo_nib`, and multiple range comparisons (`ge_80`, `le_bf`, etc.) — **then casts all of them to void** with `(void)chunk; (void)hi_nib;` etc. and falls back to scalar processing within the SIMD loop.

These ~40 lines of SIMD setup code are completely dead. The variables are computed and immediately discarded. This is clearly an incomplete attempt at a true SIMD UTF-8 validator that was abandoned in favor of the scalar-within-SIMD approach.

### 2.2 🟡 MEDIUM — `BytePattern::SBO_SIZE` declared but unused

**File:** `include/simdtext/pattern.hpp:48`  
**Severity:** MEDIUM

`static constexpr size_t SBO_SIZE = 64;` is declared with a comment "Small-buffer storage; heap-allocates only for patterns > 64 bytes" but the implementation in `src/pattern/pattern.cpp` uses `std::vector<uint8_t>` unconditionally. No small-buffer optimization exists.

### 2.3 🟢 LOW — Unused `first_cont_byte` in SSE2 UTF-8 validator

**File:** `src/detail/simd_sse2.cpp:162`  
**Severity:** LOW

`uint8_t first_cont_byte = 0;` is declared and assigned in the scalar tail (line 289) but its value is never read. It's a leftover from the state machine that was refactored to use `prev_lead_byte` directly.

### 2.4 🟢 LOW — `hex_val()` declared public but is internal

**File:** `include/simdtext/encode.hpp:48`  
**Severity:** LOW

`int hex_val(char c) noexcept;` is declared in the public header with a comment "Hex digit value (internal helper)" but it's part of the public API. It's defined in `url.cpp` and only used there. Should be in a detail header or removed from the public API.

---

## 3. Redundant Abstractions

### 3.1 🟡 MEDIUM — `expected.hpp` polyfill vs C++23 `std::expected`

**File:** `include/simdtext/expected.hpp`  
**Severity:** MEDIUM

The codebase includes a ~170-line `expected` polyfill. However, this polyfill is **never used** anywhere in the codebase — no header or source file includes `expected.hpp` or uses `simdtext::expected`. The `DecodeResult` struct in `types.hpp` is used instead. The polyfill is dead code.

### 3.2 🟡 MEDIUM — `SIMDTEXT_MODULE` / `SIMDTEXT_EXPORT` macros unused

**File:** `include/simdtext/export.hpp:19-22`  
**Severity:** MEDIUM

`SIMDTEXT_EXPORT` and the `SIMDTEXT_MODULE` guard are defined but never used in any header or source file. No C++20 module support is implemented.

---

## 4. Naming Inconsistencies

### 4.1 🔴 HIGH — Inconsistent case-conversion function names

**Files:** `include/simdtext/ascii.hpp`, `include/simdtext/scan.hpp`, `src/highway/simd_hwy.cpp`

| Module | Lowercase function | Uppercase function |
|--------|-------------------|-------------------|
| `ascii.hpp` | `lowercase_ascii_inplace` | `uppercase_ascii_inplace` |
| `scan.hpp` | (not declared) | (not declared) |
| `src/detail/simd_*.cpp` | `lowercase_ascii` | `uppercase_ascii` |
| `src/highway/simd_hwy.cpp` | `lowercase_ascii_inplace` | `uppercase_ascii_inplace` |
| `src/scalar/scalar.cpp` | `lowercase_ascii_inplace` | `uppercase_ascii_inplace` |
| C API | `simdtext_lowercase_ascii` | `simdtext_uppercase_ascii` |

The detail namespace uses `lowercase_ascii` (no `_inplace` suffix) while the public API uses `lowercase_ascii_inplace`. This is technically correct (the detail versions take raw pointers, the public ones take spans) but confusing.

### 4.2 🟡 MEDIUM — `utf8_length` vs `count_code_points`

**File:** `include/simdtext/utf8.hpp:49-51`

```cpp
SIMDTEXT_NODISCARD SIMDTEXT_API size_t count_code_points(std::string_view input) noexcept;
SIMDTEXT_NODISCARD SIMDTEXT_API size_t utf8_length(std::string_view input) noexcept;
```

Both functions have identical semantics (counting Unicode code points). `utf8_length` is just an alias that calls `count_code_points`. Having two names for the same operation is confusing. Pick one as the primary and make the other a documented alias.

### 4.3 🟢 LOW — Inconsistent function parameter naming

- `scan.hpp`: `count_byte(std::span<const char> input, char byte)` — parameter named `input`
- `detail/simd_*.cpp`: `count_byte(const char* data, size_t size, char byte)` — parameter named `data`
- `simd_hwy.cpp`: `count_byte(std::span<const char> input, char byte)` — parameter named `input`

The public API uses `input` while the internal detail namespace uses `data`. Not a bug, but inconsistent.

### 4.4 🟢 LOW — `trim_ascii` in `ascii.hpp` vs `trim` in `str.hpp`

Both headers offer trim functions with different semantics:
- `ascii.hpp`: `trim_ascii()` — trims space, tab, CR, LF only
- `str.hpp`: `trim()`, `trim_left()`, `trim_right()` — trims all C whitespace (includes \f, \v)

The naming doesn't make the difference clear. `trim_ascii` sounds like it trims ASCII whitespace, but `trim` also only handles ASCII whitespace — they just disagree on which ASCII characters count.

---

## 5. Missing or Incomplete Implementations

### 5.1 ⚠️ CRITICAL — NEON `find_byte()` is truncated/incomplete

**File:** `src/detail/simd_neon.cpp:108-121`  
**Severity:** CRITICAL

The `find_byte()` function body is **physically truncated** — the file ends mid-expression at line 121 with `for (; i + 16 <= si`. This means:
- NEON `find_byte` will not compile
- If NEON is the only available backend (ARM), the entire library fails to build
- The dispatch in `simd_dispatch.cpp` forwards to `neon::find_byte`, which is broken

### 5.2 ⚠️ CRITICAL — SSE2 `count_code_points` uses `__builtin_popcount` without popcnt target

**File:** `src/detail/simd_sse2.cpp:383`  
**Severity:** CRITICAL

The SSE2 file is compiled with `#pragma GCC target("sse2,ssse3")` (no popcnt), but `count_code_points` uses `__builtin_popcount` (lines 383-384). This will:
- On GCC/Clang: emit a call to `__popcountdi2` (slow library call), defeating the purpose of SIMD
- Potentially cause link failures on some platforms
- The SSE2 `count_byte` correctly uses a SWAR `popcount16` helper, but `count_code_points` doesn't

This is inconsistent with the file's own design philosophy (the comment says "SWAR popcount — pure arithmetic, no hardware popcnt needed").

### 5.3 🔴 HIGH — AVX-512 missing `validate_utf8` and `count_code_points`

**File:** `src/detail/simd_avx512.cpp`  
**Severity:** HIGH

The AVX-512 backend implements `count_byte`, `is_ascii`, `lowercase_ascii`, `uppercase_ascii`, `find_byte` but is missing `validate_utf8` and `count_code_points`. The dispatch in `simd_dispatch.cpp` does NOT route `validate_utf8_dispatch` or `count_code_points_dispatch` to the AVX-512 backend — it falls through to AVX2. This means AVX-512-capable CPUs can't use AVX-512 for UTF-8 validation.

### 5.4 🔴 HIGH — NEON missing `validate_utf8` and `count_code_points`

**File:** `src/detail/simd_neon.cpp`  
**Severity:** HIGH

The NEON backend only implements `count_byte`, `is_ascii`, `lowercase_ascii`, `uppercase_ascii`, and (broken) `find_byte`. There are no `validate_utf8` or `count_code_points` implementations. ARM platforms fall through to scalar for these operations.

### 5.5 🔴 HIGH — `Utf8Validator::validate()` incomplete range checks

**File:** `src/scalar/scalar.cpp` (within Utf8Validator)  
**Severity:** HIGH

The streaming `Utf8Validator::validate()` has a comment at line ~220:
```cpp
// Range checks for specific lead bytes
if (state_ == 2) {
    // After E0, next byte must be A0-BF
    // After ED, next byte must be 80-9F (no surrogates)
    // For simplicity, we just decrement and continue
    // The full range checks are handled by the SIMD validator
}
```

This means the streaming validator accepts overlong encodings and surrogates! It returns `true` for invalid UTF-8 like `\xE0\x80\x80` (overlong) or `\xED\xA0\x80` (surrogate). The non-streaming `validate_utf8_detailed()` correctly rejects these, so there's an inconsistency between the two APIs.

### 5.6 🟡 MEDIUM — `json_unescape_inplace` doesn't handle `\uXXXX` properly

**File:** `src/json/json.cpp:202`  
**Severity:** MEDIUM

The comment says: `// \uXXXX — write as-is for simplicity (full Unicode decode needs more logic)`. JSON string unescaping should decode `\uXXXX` sequences (including surrogate pairs), but the current implementation just copies the escape sequence verbatim.

---

## 6. Documentation Gaps

### 6.1 🔴 HIGH — No Doxygen `@param` / `@return` on any function

**Scope:** All headers  
**Severity:** HIGH (documentation)

While many headers have `@file` and `@brief` Doxygen comments, **no function** has `@param` or `@return` documentation. Examples:
- `count_byte(std::span<const char> input, char byte)` — no docs on what `byte` means (is it the byte value or byte index?)
- `base64_decode_to()` — `@return` says "bytes written" but doesn't document error cases
- `parallel_count_byte()` — has `@param`/`@return` in `parallel.hpp` (the exception!), but most other functions don't

### 6.2 🟡 MEDIUM — Missing `@file` / `@brief` on several headers

These headers lack file-level documentation:
- `include/simdtext/detail/cpu_detect.hpp`
- `include/simdtext/expected.hpp`
- `include/simdtext/export.hpp`
- `include/simdtext/types.hpp`
- `include/simdtext/version.hpp`
- `include/simdtext/c/simdtext.h`

### 6.3 🟡 MEDIUM — No architecture/design documentation

There's no `ARCHITECTURE.md`, `DESIGN.md`, or similar document explaining:
- The relationship between `src/scalar/scalar.cpp`, `src/detail/simd_dispatch.cpp`, and `src/highway/simd_hwy.cpp`
- Which dispatch path is active under which build configurations
- The compilation model (per-target object files with different `-march` flags)
- How to add a new SIMD backend

### 6.4 🟢 LOW — `BytePattern::parse()` doesn't document failure modes

**File:** `include/simdtext/pattern.hpp:31`

`static std::optional<BytePattern> parse(std::string_view hex_pattern);` — what makes it return `std::nullopt`? What hex formats are accepted? No documentation.

---

## 7. Test Coverage Gaps

### 7.1 🔴 HIGH — No tests for `xml` module

**Files:** `src/xml/xml.cpp` exists, but there's **no** `tests/test_xml.cpp` in the test directory. Wait — actually `tests/test_xml.cpp` does exist. Let me re-check...

Actually, the test file list shows `test_xml.cpp` IS present. However:

### 7.2 🔴 HIGH — No tests for `utf8` module (beyond what's in test_utf8.cpp)

The `Utf8Validator` streaming class, `validate_utf8_detailed()`, and `count_code_points()` — these should have edge case tests for:
- Overlong encodings (e.g., `\xC0\x80`, `\xE0\x80\x80`, `\xF0\x80\x80\x80`)
- Surrogates (`\xED\xA0\x80`)
- Codepoints above U+10FFFF (`\xF4\x90\x80\x80`)
- Incomplete sequences at chunk boundaries (streaming validator)
- Empty input
- Single-byte input

### 7.3 🟡 MEDIUM — No test for `src/detail/` backends directly

All tests go through the public API. There are no direct tests for:
- SSE2 backend
- AVX2 backend
- AVX-512 backend
- NEON backend
- Highway backend
- SWAR scalar backend

This means backend-specific bugs (like the truncated NEON `find_byte` or the SSE2 `__builtin_popcount` issue) would not be caught by CI unless run on the appropriate hardware.

### 7.4 🟡 MEDIUM — `expected.hpp` polyfill has no tests

The 170-line `expected` polyfill has zero test coverage.

### 7.5 🟢 LOW — No fuzz test coverage for `csv`, `markdown`, `xml` parsers

Fuzz tests exist for `encode`, `url`, and `utf8`, but not for the parser modules (`csv`, `markdown`, `xml`) which process untrusted input and are most vulnerable to edge cases.

---

## 8. Additional Issues

### 8.1 🔴 HIGH — SSE2 `validate_utf8` claims "SSE2 only" but uses SSSE3 intrinsics

**File:** `src/detail/simd_sse2.cpp:1-6`

The comment says "Target SSE2 only. Stay within SSE2 instruction set — no SSSE3, no popcnt." But the pragma says `#pragma GCC target("sse2,ssse3")` and the file includes `<tmmintrin.h>` (SSSE3). While all x86-64 CPUs since ~2011 support SSSE3, the comment is misleading. The filename `simd_sse2.cpp` suggests SSE2-only.

### 8.2 🟡 MEDIUM — `simd_dispatch.cpp` forward-declares functions that may not exist

**File:** `src/detail/simd_dispatch.cpp:12-42`

The forward declarations for `sse2::`, `avx2::`, `neon::` namespaces are unconditional within platform `#if` guards. However, the actual object files (`simd_sse2.o`, `simd_avx2.o`, etc.) may or may not be linked depending on CMake configuration. If an object file is missing but the dispatch code references it, this will cause link errors rather than runtime fallback.

### 8.3 🟡 MEDIUM — `replace_all` in `str.cpp` has dead code on first call

**File:** `src/str/str.cpp:27-29`

```cpp
std::string result(input);
const char* end = find_byte(result.data(), result.data() + result.size(), needle);
// Use SIMD find_byte to skip to each occurrence
for (size_t i = 0; i < result.size(); ) {
```

The first `find_byte` call on line 28 is unused — its result is immediately discarded. The loop below calls `find_byte` again from scratch. This is a leftover from a refactor.

### 8.4 🟢 LOW — `common_suffix_length` in `diff.cpp` is pure scalar

**File:** `src/diff/diff.cpp:148-155`

While `common_prefix_length` uses SSE2 for acceleration, `common_suffix_length` is a simple byte-by-byte loop. For long strings with matching suffixes, this could be SIMD-accelerated similarly.

---

## Summary Table

| # | Severity | Category | Description | Location |
|---|----------|----------|-------------|----------|
| 1.1 | CRITICAL | Duplication | Dual dispatch architecture | scalar.cpp vs simd_dispatch.cpp |
| 5.1 | CRITICAL | Incomplete | NEON find_byte() truncated | simd_neon.cpp:108-121 |
| 5.2 | CRITICAL | Bug | SSE2 count_code_points uses __builtin_popcount without popcnt target | simd_sse2.cpp:383 |
| 1.2 | HIGH | Duplication | Highway backend duplicates all SIMD functionality | simd_hwy.cpp |
| 2.1 | HIGH | Dead code | 80+ lines dead SIMD code in SSE2 UTF-8 validator | simd_sse2.cpp:179-220 |
| 4.1 | HIGH | Naming | Inconsistent case-conversion function names | Multiple files |
| 5.3 | HIGH | Incomplete | AVX-512 missing validate_utf8/count_code_points | simd_avx512.cpp |
| 5.4 | HIGH | Incomplete | NEON missing validate_utf8/count_code_points | simd_neon.cpp |
| 5.5 | HIGH | Bug | Utf8Validator accepts invalid UTF-8 (overlong/surrogate) | scalar.cpp:~220 |
| 6.1 | HIGH | Docs | No @param/@return Doxygen on any function | All headers |
| 7.2 | HIGH | Testing | Insufficient UTF-8 edge case tests | test_utf8.cpp |
| 8.1 | HIGH | Naming | File says "SSE2 only" but uses SSSE3 | simd_sse2.cpp:1-6 |
| 1.3 | MEDIUM | Duplication | Duplicated hex_decode_table | encode.cpp, url.cpp |
| 1.4 | MEDIUM | Duplication | Duplicated ctz32/popcount helpers | simd_sse2.cpp, simd_avx2.cpp |
| 1.5 | MEDIUM | Duplication | Copy-pasted UTF-8 scalar loop | All x86 backends |
| 2.2 | MEDIUM | Dead code | SBO_SIZE declared but unused | pattern.hpp:48 |
| 3.1 | MEDIUM | Redundant | expected.hpp polyfill never used | expected.hpp |
| 3.2 | MEDIUM | Redundant | SIMDTEXT_MODULE/EXPORT macros unused | export.hpp |
| 4.2 | MEDIUM | Naming | utf8_length vs count_code_points | utf8.hpp |
| 5.6 | MEDIUM | Incomplete | json_unescape_inplace doesn't handle \uXXXX | json.cpp:202 |
| 6.2 | MEDIUM | Docs | Missing @file/@brief on several headers | Multiple |
| 6.3 | MEDIUM | Docs | No architecture/design documentation | — |
| 7.3 | MEDIUM | Testing | No direct tests for detail/ backends | — |
| 7.4 | MEDIUM | Testing | expected.hpp polyfill has no tests | — |
| 8.2 | MEDIUM | Bug risk | Unconditional forward declarations in dispatch | simd_dispatch.cpp |
| 8.3 | MEDIUM | Dead code | Unused find_byte call in replace_all | str.cpp:28 |
| 1.6 | LOW | Duplication | Duplicated is_whitespace with different semantics | str.cpp, scalar.cpp |
| 2.3 | LOW | Dead code | Unused first_cont_byte in SSE2 UTF-8 | simd_sse2.cpp:162 |
| 2.4 | LOW | Dead code | hex_val() in public API but is internal | encode.hpp:48 |
| 4.3 | LOW | Naming | Inconsistent parameter naming (input vs data) | Multiple |
| 4.4 | LOW | Naming | trim_ascii vs trim whitespace disagreement | ascii.hpp, str.hpp |
| 6.4 | LOW | Docs | BytePattern::parse() undocumented | pattern.hpp:31 |
| 7.5 | LOW | Testing | No fuzz tests for csv/markdown/xml parsers | fuzz/ |
| 8.4 | LOW | Perf | common_suffix_length not SIMD-accelerated | diff.cpp:148 |

---

## Recommended Actions (Priority Order)

1. **Fix NEON `find_byte` truncation** — this is a build-breaking bug on ARM
2. **Fix SSE2 `__builtin_popcount`** — replace with the existing `popcount16` SWAR helper or add `popcnt` to the target pragma
3. **Unify dispatch architecture** — pick one dispatch path (`simd_dispatch.cpp` is more complete) and make `scalar.cpp` call through it
4. **Remove dead SSE2 UTF-8 SIMD code** — the 80+ lines of void-casted variables
5. **Fix `Utf8Validator::validate()` to reject overlong/surrogate** — it currently accepts invalid UTF-8
6. **Add AVX-512 and NEON `validate_utf8`/`count_code_points`** implementations
7. **Extract shared helpers** — `hex_decode_table` to a detail header, `ctz32`/`popcount` to `detail/bitops.hpp`, UTF-8 scalar validator to `detail/utf8_scalar.hpp`
8. **Remove `expected.hpp`** — unused polyfill, or move to `detail/` if it will be used later
9. **Add UTF-8 edge case tests** — overlong, surrogate, >U+10FFFF, streaming chunk boundaries
10. **Add `@param`/`@return` Doxygen** to public API functions

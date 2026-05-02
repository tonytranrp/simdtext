# Hot-Path Analysis Report — simdtext

**Date:** 2026-05-02  
**Scope:** All `.cpp` files in `src/` + SIMD detail implementations  
**Goal:** Identify optimization opportunities across all hot paths

---

## Summary of Findings

| Category | Count | Severity |
|---|---|---|
| Memory allocation in hot paths | 6 | 🔴 High |
| Scalar loops that could use SIMD | 5 | 🔴 High |
| Branch misprediction opportunities | 4 | 🟡 Medium |
| Redundant computations | 3 | 🟡 Medium |
| Cache-unfriendly access patterns | 2 | 🟡 Medium |
| SIMD intrinsics opportunities | 4 | 🟡 Medium |
| Missing prefetch / non-temporal stores | 3 | 🟠 Low-Med |
| Dead / wasted code in SIMD paths | 2 | 🟡 Medium |

---

## 1. File-by-File Analysis

### 1.1 `src/scalar/scalar.cpp` — Dispatch layer + string utilities

**Hot paths:**
- `count_byte()`, `is_ascii()`, `lowercase_ascii_inplace()`, `uppercase_ascii_inplace()`, `find_byte()` — all dispatch to SIMD. The dispatch itself calls `detail::detect_cpu()` on every invocation.

**🔴 Redundant computation:** `detect_cpu()` is called on **every** function call. CPU features don't change at runtime. This should be cached (e.g., `static const auto& f = detail::detect_cpu();` or a global).

**🟡 `trim_ascii()`** — Uses scalar `while` loops to skip whitespace. For long strings with whitespace, could use SIMD `find_byte` to skip to the first non-whitespace byte rather than character-by-character.

**🟡 `contains()`** — Delegates to `std::string_view::find()` which is typically a scalar `memchr`/`memmem`. For single-char needles, should use the SIMD `find_byte`.

**🟡 `LineView::Iterator::advance()` / `SplitView::Iterator::advance()`** — Use `remaining_.find('\n')` which is scalar. Could use SIMD `find_byte` for the delimiter search, potentially significant for line-counting on large files.

**🟡 `Utf8Validator::validate()`** — Scalar byte-by-byte with per-byte branching. For streaming validation this is unavoidable, but the per-byte `if/else if` chain has branch misprediction potential. Could use a lookup-table approach.

---

### 1.2 `src/encode/encode.cpp` — Hex & Base64 encode/decode

**Hot paths:**
- `hex_encode_to()` — 4-byte unrolled loop. Already reasonable.
- `hex_decode_to()` — Per-byte scalar loop with table lookup.
- `base64_encode_to()` — 3-byte chunk loop. Scalar.
- `base64_decode_to()` — 4-byte chunk loop. Scalar.

**🔴 Scalar loops that could use SIMD:**
- **`base64_encode_to()`** — The inner loop processes 3 bytes → 4 chars. This is a classic SIMD vectorization target. With SSSE3 `_mm_shuffle_epi8`, you can process 12 input bytes → 16 output chars per iteration (4x unroll of the 3→4 mapping).
- **`base64_decode_to()`** — Same: 4 chars → 3 bytes. Can use `_mm_shuffle_epi8` + `_mm_maddubs_epi16` for parallel lookup and merge.
- **`hex_encode_to()`** — Can use SIMD nibble extraction + pshufb lookup to process 16 bytes → 32 chars at once.
- **`hex_decode_to()`** — Can use SIMD for the table lookup and merge.

**🔴 Memory allocation in hot paths:**
- `hex_encode()` — allocates `std::string(input.size() * 2, '\0')` 
- `hex_decode()` — allocates `std::vector<std::byte>(input.size() / 2)`
- `base64_encode()` — allocates `std::string(required, '\0')`
- `base64_decode()` — allocates `std::vector<std::byte>(max_bytes - padding)`

These are the convenience wrappers. The `_to` variants are zero-allocation and should be preferred. Consider documenting this or providing `std::string` overloads that reuse buffers.

**🟡 Branch misprediction in `base64_decode_to()`:** The per-chunk `if (a == 64 || b == 64)` error check and `if (src[i+2] != '=')` / `if (src[i+3] != '=')` padding checks create data-dependent branches. For trusted input, a branchless path that always writes 3 bytes and then adjusts the write count would avoid mispredictions.

**🟡 Redundant computation in `hex_decode_to()`:** `hi > 15` check is redundant — the hex_decode_table maps valid hex chars to 0-15 and everything else to -1, so `hi < 0` already covers invalid. The `hi > 15` check is dead code (the table never returns values > 15 for hex chars).

---

### 1.3 `src/url/url.cpp` — URL encode/decode

**Hot paths:**
- `url_encode_to()` — Per-byte scalar loop with branch on `url_safe_table[uc]`.
- `url_decode_to()` — Per-byte scalar loop with branch on `%`.
- `url_encode()` / `url_decode()` — Allocating variants.

**🔴 Memory allocation in hot paths:**
- `url_encode()` — allocates `std::string` with `reserve(input.size() * 3)`, then appends char-by-char with `result +=`. Each `+=` may trigger small-string optimization checks. Should use a pre-sized buffer + direct write like `hex_encode_to` does.
- `url_decode()` — same pattern with char-by-char `result +=`.
- `parse_query()` — allocates `std::unordered_map<std::string, std::string>` + per-key `std::string` allocations + `url_decode()` allocation per value. Very allocation-heavy.

**🔴 Scalar loop could use SIMD — `url_encode_to()`:**
The branch `if (url_safe_table[uc])` is per-byte. For URL-safe input (common case), this is a predictable branch. But the encoding still can't be easily SIMD'd because the output is variable-length (1 or 3 bytes per input byte). However, a two-pass approach would work:
1. SIMD pass: classify all bytes as safe/unsafe (using a lookup + movemask)
2. Calculate total output length
3. SIMD or scalar write pass

This eliminates branch misprediction and enables prefetch.

**🟡 Branch misprediction — `url_decode_to()`:** The `if (src[i] == '%')` branch is per-byte and data-dependent. For URL-encoded input with many `%XX` sequences, the branch is somewhat predictable, but for mostly-ASCII URLs, it's unpredictable. A SIMD approach could scan for `%` first using `find_byte`, then process runs of literal bytes with `memcpy`.

**🟡 Duplicated lookup table:** `hex_decode_table` is duplicated between `encode.cpp` and `url.cpp` "for TU locality." This is wasteful — the table is 256 bytes and could be shared via a header `inline constexpr` or a shared detail header. The duplication is a maintenance risk.

---

### 1.4 `src/str/str.cpp` — String utilities

**Hot paths:**
- `trim_left()`, `trim_right()` — Scalar whitespace scanning.
- `replace_all(char, char)` — Uses SIMD `find_byte` to skip to matches. Good.
- `replace_all(string_view, string_view)` — Mixed SIMD + scalar.
- `fields()`, `split_vec()`, `split_into()` — Parsing loops.

**🔴 Memory allocation in hot paths:**
- `replace_all(char, char)` — Copies entire input string upfront (`std::string result(input)`), then does SIMD find + replace. The copy is necessary for in-place, but the initial `find_byte` call before the loop is dead code (result isn't used).
- `replace_all(string_view, string_view)` — Allocates `std::string` with `reserve(input.size())`. May reallocate if replacement is longer than needle.
- `fields()` — Returns `std::vector<std::string_view>`, heap-allocating.
- `split_vec()` — Same.

**🟡 Dead code in `replace_all(char, char)`:**
```cpp
const char* end = find_byte(result.data(), result.data() + result.size(), needle);
```
This line computes `end` but never uses it. The loop re-calls `find_byte` starting from `i=0`. This is a wasted SIMD scan.

**🟡 `trim_left()` could use SIMD:** For long leading whitespace runs, `find_byte`-style SIMD scanning would be faster. Same for `trim_right()` — could scan backwards with SIMD.

**🟡 `fields()` — scalar whitespace skipping:** Uses `is_whitespace()` char-by-char. Could use SIMD to skip whitespace runs.

---

### 1.5 `src/hash/hash.cpp` — CRC32, CRC32C, xxHash64, wyhash

**Hot paths:**
- `crc32()` — Scalar table-lookup loop.
- `crc32c()` — Hardware-accelerated via `crc32q` inline asm.
- `xxhash64()` — Scalar with `__builtin_memcpy` unaligned reads.
- `wyhash()` — Scalar with `__uint128_t` multiply.

**🔴 Scalar loop that could use SIMD — `crc32()`:**
The standard CRC32 polynomial (0xEDB88320) has no hardware instruction. However, the **slice-by-4** or **slice-by-8** algorithm processes 4-8 bytes per iteration using parallel table lookups, which is ~4-8x faster than byte-by-byte. This is a well-known optimization.

**🟡 `crc32c()` — Missing 64-bit stride after the qword loop:** The function switches from `crc32q` (8-byte) to `crc32l` (4-byte) then `crc32b` (1-byte). This is correct, but the initial `uint64_t crc` → `uint32_t crc32_val` transition is clunky. The `crc32q` should accumulate into a 64-bit register natively — the cast to `uint32_t` discards the upper 32 bits which are always zero in CRC32C, so it works, but the code is confusing.

**🟡 `xxhash64()` — No SIMD opportunity** (xxHash64 is inherently scalar-friendly, and the algorithm is already well-optimized). The `round` lambda should be marked `__attribute__((always_inline))` or be a template to guarantee inlining.

**🟡 `wyhash()` — `__uint128_t` multiply:** The `wymix` uses 128-bit multiply which compiles to `mulq` on x86-64. This is optimal. No SIMD opportunity here.

**🟡 Cache-friendly access:** All hash functions process input sequentially — cache-friendly by nature. No issues.

---

### 1.6 `src/json/json.cpp` — JSON tokenizer

**Hot paths:**
- `JsonTokenizer::next()` — Per-token dispatch with `switch`.
- `skip_whitespace()` — Scalar whitespace skip.
- `json_unescape_inplace()` — Scalar byte-by-byte unescape.

**🟡 Branch misprediction — `next()`:** The `switch(c)` on every token start is data-dependent. For structured JSON (repeating patterns of `"key": value,`), this is somewhat predictable, but for heterogeneous JSON it's not. Limited optimization potential — this is fundamentally a state machine.

**🟡 `skip_whitespace()` could use SIMD:** For large JSON files with lots of whitespace/indentation, a SIMD scan (similar to `find_byte` but for multiple whitespace chars) would skip much faster. Could use a 4-char lookup: `(' ' << 24) | ('\t' << 16) | ('\n' << 8) | '\r'` and compare with SIMD.

**🟡 `json_unescape_inplace()` — branch-heavy:** The `switch(data[i])` on every escaped character is predictable for common cases (`\"`, `\\`) but the overall `if (data[i] == '\\')` check on every byte is a branch. For strings with few escapes, this is predictable. For heavily-escaped strings, a SIMD scan for `\\` first would help.

**🟡 Redundant bounds checks in `next()`:** The string scanning in the `'"'` case checks `pos_ < input_.size()` on every character. Could scan for `'"'` and `'\\'` using SIMD, then verify the escape.

**No allocation in hot paths** — `JsonTokenizer` operates on `string_view`. Good.

---

### 1.7 `src/csv/csv.cpp` — CSV parser

**Hot paths:**
- `CsvParser::next_row()` — Per-character state machine.

**🔴 Memory allocation in hot paths:**
- `fields_.clear()` on every `next_row()` — `std::vector<std::string_view>` clearing is O(1) but re-allocation happens when fields grow. For CSV with varying field counts per row, this causes heap churn.
- `parse_csv_row()` — Creates a new `CsvParser` on every call. The parser allocates its `fields_` vector each time.

**🟡 Branch misprediction — `next_row()`:** The per-character `if (c == opts_.quote)` / `else if (c == opts_.delimiter)` / `else if (c == '\n')` chain is data-dependent. For CSV with consistent structure, this is predictable, but for mixed quoted/unquoted fields, it's not.

**🟡 SIMD opportunity — field scanning:** For unquoted fields, scanning to the next delimiter/newline is essentially `find_byte` — already SIMD-accelerated in this library. The CSV parser could use `find_byte` to skip to the next delimiter in unquoted fields instead of character-by-character scanning.

**🟡 Cache-friendly:** Sequential scan. No issues.

---

### 1.8 `src/diff/diff.cpp` — Line diff

**Hot paths:**
- `line_diff()` — LCS DP computation + backtracking.
- `common_prefix_length()` — SSE2 + scalar comparison.
- `common_suffix_length()` — Scalar backward comparison.

**🔴 Memory allocation in hot paths:**
- `split_lines()` — Returns `std::vector<std::string_view>`, allocating for every line.
- `line_diff()` — Allocates `dp` (m+1 × n+1) `size_t` matrix. For 1000×1000 lines, that's 8MB. Uses `std::vector<std::vector<size_t>>` — each inner vector is a separate heap allocation! This is extremely cache-unfriendly.
- Also allocates `prev` and `curr` vectors (unused when dp is allocated). Dead code.

**🔴 Cache-unfriendly access pattern — `line_diff()` DP:**
- `dp[i][j]` is a 2D vector of vectors → each row is a separate heap allocation → pointer chasing across rows.
- The inner loop accesses `dp[i-1][j]` and `dp[i][j-1]` — strided access across separate allocations.
- Should be a single flat `std::vector<size_t>(m * n)` with `dp[i * n + j]` indexing.

**🔴 Scalar loop that could use SIMD — `common_suffix_length()`:** Currently byte-by-byte backward comparison. Should use SSE2/AVX2 like `common_prefix_length()` does — load 16 bytes ending at `a[ai-16]` and `b[bi-16]`, compare, find last matching position.

**🟡 `split_lines()` could use SIMD:** Scanning for `\n` is `find_byte`. Could split using SIMD line-finding.

**🟡 `line_diff()` — O(m×n) LCS:** For large files, this is quadratic. The Myers diff algorithm is O(ND) where D is the edit distance, which is much faster for similar files. This is an algorithmic optimization, not a micro-optimization.

---

### 1.9 `src/log/log.cpp` — Log parsing

**Hot paths:**
- `parse_log_line()` — Per-line parsing with multiple scans.
- `count_log_levels()` — Iterates all lines, parsing each.
- `filter_log_lines()` — Same, returns matching lines.

**🔴 Memory allocation in hot paths:**
- `filter_log_lines()` — Returns `std::vector<std::string_view>` with `push_back` per matching line. Should use `reserve` or return a view.
- `count_log_levels()` — Allocates `LogCounts` (small, stack-friendly). Fine.

**🟡 `parse_log_line()` — 14 `find()` calls in worst case:** The non-bracket timestamp detection scans for each of 14 level keywords (`"TRACE"`, `"DEBUG"`, etc.) using `line.find(lv, pos)`. This is O(14 * line_length) in the worst case. Should use a single scan looking for the first alphabetic word boundary, then compare against the level keywords.

**🟡 Branch misprediction — `parse_log_line()`:** The `if (line[pos] == '[')` branch and the keyword search create data-dependent branching on the line format. For consistent log formats, this is predictable.

**🟡 `count_log_levels()` — per-line `parse_log_line()` + `parse_log_level()`:** This parses the full log line just to extract the level. Could have a fast-path that just finds the level keyword without full parsing.

---

### 1.10 `src/pattern/pattern.cpp` — Binary pattern matching

**Hot paths:**
- `find_pattern_scalar()` — Two-byte fast reject + compact verification.
- `find_pattern_sse2()` — SIMD first-byte scan + scalar verification.
- `find_pattern_avx2()` — Same with AVX2.
- `find_all_patterns()` — Repeated calls to `find_pattern`.

**🟡 SIMD verification is scalar:** After the SIMD first-byte scan finds candidates, verification is still a scalar loop over `fixed[]` entries. For patterns with many non-wildcard bytes, this could use SIMD: load the candidate region, XOR with pattern bytes, AND with mask, check for zero.

**🟡 `FixedEntry fixed[256]` on stack:** 256 * (size_t + uint8_t) = ~2KB on stack per call. With padding, this could be ~3KB. Fine for most use cases but worth noting for deep recursion.

**🟡 `find_all_patterns()` — allocation:** Returns `std::vector<const uint8_t*>` with `push_back` per match. No `reserve`. For files with many pattern matches, this causes repeated reallocation.

**🟡 SSE2 tail after AVX2:** The AVX2 `find_pattern_avx2()` falls back to a scalar loop (not SSE2) for the tail after the AVX2 loop. The SSE2 version exists — could call it for the tail.

**🟡 Redundant `FixedEntry` computation:** All three `find_pattern_*` functions independently compute the same `fixed[]`, `first_fixed`, `first_byte` from the pattern. This should be computed once and stored in `BytePattern`.

---

## 2. SIMD Detail Analysis

### 2.1 `src/detail/simd_sse2.cpp`

**Architecture:** SSE2 + SSSE3 (via `#pragma GCC target`). 4x unrolled 64-byte loops. SWAR popcount for `count_byte`.

**🔴 Dead SIMD code in `validate_utf8()`:** The function declares numerous SIMD variables (`class_lookup`, `vcont`, `vlead2`, etc.) and performs nibble extraction, range comparisons — **then casts them all to `(void)` and falls back to scalar.** The entire SIMD classification block (lines ~180-220) is dead code that compiles to nothing but wastes compile time and is confusing. Either implement the SIMD UTF-8 validation or remove the dead code.

**🟡 `count_code_points()` uses `__builtin_popcount`** while `count_byte()` uses SWAR `popcount16()`. Inconsistent. On x86-64 with `-mpopcnt` (guaranteed since 2008), `__builtin_popcount` compiles to a single `popcnt` instruction. The SWAR version generates ~5 ALU instructions. The SWAR approach was chosen for SSE2 builds without popcnt, but the file already enables SSSE3 via pragma. Should use `__builtin_popcount` consistently.

**🟡 Missing `_mm_prefetch` in `find_byte()`:** The `count_byte` loop processes 64 bytes at a time but doesn't prefetch. For large buffers that exceed L3, adding `_mm_prefetch(data + i + 256, _MM_HINT_T0)` would help. The scalar `find_pattern` in `pattern.cpp` has prefetch but `find_byte` doesn't — inconsistent.

**🟡 Non-temporal stores in `lowercase_ascii` / `uppercase_ascii`:** Already implemented with a 2MB threshold. Good. However, the branch `if (use_nontemporal)` is evaluated per-iteration — it's loop-invariant and should be hoisted outside the loop (two separate loops). The branch predictor will likely predict it correctly, but code clarity and a tiny perf win would come from splitting.

**🟡 `validate_utf8()` — scalar inner loop within SIMD outer loop:** Even in the "SIMD" path, every non-ASCII chunk falls back to a scalar byte-by-byte state machine. For ASCII-heavy UTF-8 (common case), the SIMD fast-path skip works. For CJK text, every chunk hits the scalar path. A proper SIMD UTF-8 validator (like the one in simdjson) would process all bytes in SIMD, only falling back for boundary cases.

---

### 2.2 `src/detail/simd_avx2.cpp`

**Architecture:** AVX2 with explicit `no-avx512*` pragma to avoid frequency downclocking. 4x unrolled 128-byte loops. Hardware popcnt.

**🟡 Same `validate_utf8()` issue as SSE2:** Falls back to scalar for any non-ASCII chunk. The AVX2 version is slightly better — it checks the whole 32-byte chunk for ASCII-ness first, then processes non-ASCII chunks scalar. But for non-ASCII text, it's entirely scalar.

**🟡 Missing `_mm_prefetch` in `find_byte()` and `count_byte()`:** Same as SSE2 — no software prefetching for streaming operations on large buffers.

**🟡 `count_code_points()` — redundant `v80` constant:** Declares `const __m256i v80 = _mm256_set1_epi8(...)` then uses `_mm256_set1_epi8(static_cast<char>(0xC0))` inline. The `0xC0` constant should also be hoisted to a named constant.

**🟡 Non-temporal stores in `lowercase_ascii` / `uppercase_ascii`:** Same 2MB threshold with per-iteration branch. Should be hoisted.

**🟡 SSE2 tail handling:** The AVX2 functions properly fall back to SSE2 for 16-31 byte tails. Good practice.

**Missing intrinsic opportunities:**
- `_mm256_blendv_epi8` could replace the `xor + and` pattern in case conversion. Currently: `_mm256_xor_si256(chunk, _mm256_and_si256(is_upper, vbit5))`. Since `is_upper` is a mask of all-zeros or all-ones per lane, the xor+and is equivalent to a blend, and compilers typically optimize this. However, `_mm256_blendv_epi8` is a single instruction vs 2, and would be more explicit.
- For `is_ascii()`, could use `_mm256_testz_si256` instead of `_mm256_movemask_epi8` — it's a single instruction that sets ZF, avoiding the 2-cycle `vpmovmskb` latency.

---

### 2.3 `src/detail/simd_avx512.cpp`

**Architecture:** AVX-512BW with mask registers. 4x unrolled 256-byte loops. Uses `_mm512_cmpeq_epi8_mask` (mask-based, no movemask needed).

**✅ Well-implemented:** Uses mask registers natively, proper tail handling with `_mm512_maskz_loadu_epi8`, `_mm512_mask_blend_epi8` for case conversion. This is the cleanest of the three SIMD implementations.

**🟡 Missing non-temporal stores:** Unlike SSE2/AVX2, the AVX-512 `lowercase_ascii` / `uppercase_ascii` don't use `_mm512_stream_si512` for large buffers. Should add the same 2MB threshold pattern.

**🟡 Missing `_mm_prefetch`:** Same as SSE2/AVX2.

**🟡 `is_ascii()` could use `_mm512_test`-style:** Instead of `_mm512_movepi8_mask(ored) != 0`, could use a shorter check. But the mask-based approach is already efficient on AVX-512.

---

## 3. Cross-Cutting Concerns

### 3.1 CPU Feature Dispatch Overhead

Every call to `count_byte()`, `is_ascii()`, `lowercase_ascii_inplace()`, `uppercase_ascii_inplace()`, `find_byte()` in `scalar.cpp` calls `detail::detect_cpu()`. If this function uses `__builtin_cpu_supports()` or CPUID, it may not be trivially cheap. Even if it reads a cached global, the function call + branch is unnecessary per-invocation.

**Recommendation:** Use function pointers set once at startup, or `static const auto& f = detail::detect_cpu();` within each dispatch function (first-call initialization via `static`).

### 3.2 `find_byte` Underutilization

The library has a SIMD-accelerated `find_byte()` but many internal loops don't use it:
- `LineView::Iterator::advance()` uses `string_view::find('\n')`
- `CsvParser::next_row()` scans for delimiter char-by-char
- `split_lines()` in diff.cpp scans for `'\n'` char-by-char
- `trim_left()` / `trim_right()` scan char-by-char

These should all use `find_byte()` for the scanning step.

### 3.3 Allocation Strategy

Multiple functions allocate `std::vector` or `std::string` on every call:
- `replace_all()`, `fields()`, `split_vec()`, `split_lines()`, `line_diff()`, `filter_log_lines()`, `find_all_patterns()`, `parse_query()`

For high-throughput use, these should offer `_into` variants (like `hex_encode_to` / `split_into`) that write into caller-provided buffers. The existing `split_into()` is a good pattern that should be extended.

### 3.4 UTF-8 Validation SIMD

Both SSE2 and AVX2 `validate_utf8()` implementations fall back to scalar for non-ASCII chunks. The simdjson library demonstrates that full SIMD UTF-8 validation is possible using a state machine approach with `_mm_shuffle_epi8`-based lookup tables. This would be the single biggest SIMD improvement for the library.

### 3.5 Non-Temporal Store Pattern

SSE2 and AVX2 case-conversion functions use a per-iteration `if (use_nontemporal)` branch. This should be two separate loops:

```cpp
if (use_nontemporal) {
    for (; i + N <= size; i += N) { /* stream store */ }
} else {
    for (; i + N <= size; i += N) { /* normal store */ }
}
```

The branch predictor will likely handle it, but it's cleaner and avoids any potential branch-misprediction at loop entry.

---

## 4. Priority Ranking

| # | Optimization | Impact | Effort |
|---|---|---|---|
| 1 | Full SIMD UTF-8 validation (simdjson-style) | 🔴 Very High | High |
| 2 | SIMD base64 encode/decode | 🔴 High | Medium |
| 3 | Cache-friendly flat DP array in `line_diff()` | 🔴 High | Low |
| 4 | Cache `detect_cpu()` result / use function pointers | 🟡 Medium | Low |
| 5 | Use `find_byte()` in `LineView`, CSV, split_lines, trim | 🟡 Medium | Low |
| 6 | SIMD hex encode (pshufb-based) | 🟡 Medium | Medium |
| 7 | Two-pass SIMD URL encode (classify → write) | 🟡 Medium | Medium |
| 8 | Slice-by-8 CRC32 | 🟡 Medium | Low |
| 9 | SIMD `common_suffix_length()` | 🟡 Medium | Low |
| 10 | Remove dead SIMD code in SSE2 `validate_utf8()` | 🟠 Low | Trivial |
| 11 | SIMD verification in `find_pattern` | 🟠 Low | Medium |
| 12 | Prefetch in `find_byte` / `count_byte` | 🟠 Low | Trivial |
| 13 | Split non-temporal store loops | 🟠 Low | Trivial |
| 14 | AVX-512 non-temporal stores | 🟠 Low | Trivial |
| 15 | `_mm256_testz_si256` for `is_ascii` AVX2 | 🟠 Low | Trivial |

---

*End of report. No code was modified during this analysis.*

# API Audit Report — simdtext v0.2.0

**Date:** 2026-05-02  
**Scope:** All public headers in `include/simdtext/`, C API, comparison with simdutf/ada/fast_float

---

## 1. C++ API Issues

### 🔴 High Severity

| # | Module | Issue |
|---|--------|-------|
| H1 | `ascii.hpp` | `lowercase_ascii_inplace` / `uppercase_ascii_inplace` — no validation that `input` is ASCII first. Mutating non-ASCII bytes is semantically wrong but silently succeeds. Should at least document precondition or offer a checked variant. |
| H2 | `encode.hpp` | `hex_decode` / `base64_decode` (vector-returning overloads) have **no way to report errors**. Invalid input silently returns a partial/empty vector. No `DecodeResult` is returned unlike the `_to` variants. |
| H3 | `utf8.hpp` | `count_code_points` / `utf8_length` — documented as "valid UTF-8" but **no check is performed**. UB on invalid UTF-8 (could over/under-count). simdutf always validates first or offers separate validated/unvalidated paths. |
| H4 | `file.hpp` | `FileScanner(const char* path)` constructor — no way to report open failure except via `is_open()` afterward. But the constructor already mapped the file; if mapping fails, `is_open()` returns false but the object is in a zombie state. Should use a factory method or `expected`. |
| H5 | `csv.hpp` | `CsvParser::field()` — no bounds check on `index`. Out-of-bounds access is UB. Should return `std::optional<string_view>` or assert. |
| H6 | `json.hpp` | `JsonTokenizer` — `next()` returns a reference to internal token, but no `has_error()` is checked by `next()` itself. Errors are "sticky" — once you hit an error, subsequent `next()` calls are undefined. |
| H7 | `expected.hpp` | `expected` polyfill has UB: `operator*` / `operator->` return references into `storage_` without ever calling placement new for the value type when `has_val_` is true in the void specialization. More critically, `operator*` on the non-void specialization dereferences `storage_` as `T*` but the alignment calculation `sizeof(T) > sizeof(E) ? sizeof(T) : sizeof(E)` is wrong — alignment should use `alignof`, not `sizeof`. The `alignas` lines are correct but redundant/confusing. |
| H8 | `expected.hpp` | `expected::value()` throws `std::runtime_error` — should throw a typed exception (e.g., `bad_expected_access<E>`) matching C++23 semantics. Also, `value()` is not `noexcept(false)` explicitly. |

### 🟡 Medium Severity

| # | Module | Issue |
|---|--------|-------|
| M1 | `scan.hpp` | `count_byte` takes `std::span<const char>` but `count_newlines` also takes `std::span<const char>`. Inconsistent: `url.hpp` functions use `std::string_view`. Should provide both overloads consistently or standardize on one. |
| M2 | `scan.hpp` | `find_byte` takes raw pointers (`const char* begin, const char* end`) — only API using raw pointers. Should provide `std::span` / `string_view` overload. |
| M3 | `ascii.hpp` | Missing `noexcept` on `is_ascii`, `lowercase_ascii_inplace`, `uppercase_ascii_inplace`. These are pure SIMD operations that shouldn't throw. `trim_ascii` also missing `noexcept`. |
| M4 | `scan.hpp` | Missing `noexcept` on `count_byte`, `count_newlines`, `contains`. None of these allocate. |
| M5 | `encode.hpp` | `hex_val` is marked public and `SIMDTEXT_API` but documented as "internal helper". Should be in `detail` namespace. |
| M6 | `encode.hpp` | `hex_encode` / `base64_encode` return `std::string` which allocates. No `noexcept` — but they can throw `std::bad_alloc`. This is correct but worth documenting the throwing contract. |
| M7 | `file.hpp` | `MappedFile::Impl` is a pimpl, but `FileScanner` stores `MappedFile` by value and has no pimpl. This leaks `MappedFile`'s move semantics into `FileScanner`'s header. |
| M8 | `lines.hpp` | `LineView`/`SplitView` — `begin()`/`end()` not `noexcept`. Iterator comparison operators not `noexcept`. |
| M9 | `pattern.hpp` | `BytePattern` uses `std::vector` internally (heap allocation). Comment says "Small-buffer storage; heap-allocates only for patterns > 64 bytes" but there's no SBO implementation — just `std::vector`. Misleading documentation. |
| M10 | `hash.hpp` | `fnv1a` is `constexpr` but not `SIMDTEXT_API` — this is correct for inline constexpr, but `crc32`, `crc32c`, `xxhash64`, `wyhash` take `std::string_view` only. No `std::span<const std::byte>` overloads for binary data. |
| M11 | `str.hpp` | `fields` and `split_vec` return `std::vector` but are not `noexcept`. `split_into` is `noexcept`. Inconsistent exception contract. |
| M12 | `str.hpp` | `replace_all` with single-char replacement could operate in-place but doesn't offer an in-place variant. |
| M13 | `diff.hpp` | `line_diff` returns `std::vector<DiffOp>` — allocates O(n). No streaming/callback API for large diffs. |
| M14 | `log.hpp` | `filter_log_lines` returns `std::vector<string_view>` — no streaming alternative. `parse_log_line` doesn't validate timestamp format. |
| M15 | `xml.hpp` | `xml_escape_inplace` returns `size_t` but has no error reporting for capacity overflow. Caller can't distinguish "escaped fine" from "truncated because capacity was too small". |

### 🟢 Low Severity

| # | Module | Issue |
|---|--------|-------|
| L1 | `ascii.hpp` | No `std::string_view` overload for `is_ascii` (only `std::span<const char>`). Users with `string_view` must construct a span. |
| L2 | `utf8.hpp` | `Utf8Validator` — `state_` uses `uint8_t` but only values 0–3 are valid. Could use an enum for clarity. |
| L3 | `utf8.hpp` | `count_code_points` and `utf8_length` are identical — one should be deprecated or documented as an alias. |
| L4 | `url.hpp` | `parse_query` returns `std::unordered_map` — no control over allocation, no callback/lazy variant. |
| L5 | `csv.hpp` | `CsvParser` stores `std::vector<string_view>` for fields — allocates per row. A fixed-size or SBO alternative would be better for performance-critical use. |
| L6 | `json.hpp` | `json_unescape_inplace` — no way to report invalid escape sequences. Returns length but doesn't indicate errors. |
| L7 | `parallel.hpp` | `parallel_for_each_chunk` callback has no way to signal early termination or errors. |
| L8 | `types.hpp` | `ErrorCode` is minimal — no specific error codes for different failure modes (e.g., invalid hex char vs. odd-length hex string). |
| L9 | `export.hpp` | `SIMDTEXT_UNREACHABLE` uses `__has_builtin` without `__has_include` guard. Non-standard compilers will get `do {} while(0)` which silently continues instead of trapping. |

---

## 2. Naming Inconsistencies

| Issue | Examples |
|-------|----------|
| Inconsistent verb form | `lowercase_ascii_inplace` (adjective+verb) vs `trim_ascii` (verb only) vs `trim_left` (verb+direction). Should be consistent: `to_lowercase_ascii` / `trim_left` or `lowercase` / `ltrim`. |
| `_inplace` suffix inconsistency | `lowercase_ascii_inplace`, `uppercase_ascii_inplace`, `json_unescape_inplace`, `xml_escape_inplace` — but `trim_ascii` returns a view (not in-place). The `_inplace` suffix is good but `trim` should have an `_inplace` variant too. |
| `count_code_points` vs `utf8_length` | Two names for the same operation. Pick one. |
| `fields` vs `split_vec` | `fields` is a noun, `split_vec` is verb+noun. Inconsistent. |
| `looks_like_json` / `looks_like_xml` | Colloquial naming — other APIs use `is_valid_json_prefix` or similar. Not necessarily wrong, but inconsistent with the rest of the API tone. |
| `contains` (scan.hpp) vs `contains_char` (str.hpp) | `scan::contains` searches for substring, `str::contains_char` searches for char. The naming makes the distinction clear but the module split is confusing — both are "search" operations. |

---

## 3. Missing `noexcept` Annotations

Functions that are pure SIMD / non-allocating but lack `noexcept`:

- `is_ascii` (ascii.hpp)
- `lowercase_ascii_inplace` (ascii.hpp)
- `uppercase_ascii_inplace` (ascii.hpp)
- `trim_ascii` (ascii.hpp)
- `count_byte` (scan.hpp)
- `count_newlines` (scan.hpp)
- `contains` (scan.hpp)
- `find_byte` (scan.hpp)
- `valid_utf8` (utf8.hpp) — depends on implementation but simdutf's equivalent is noexcept
- `hex_encode_to` (encode.hpp)
- `base64_encode_to` (encode.hpp)
- All `LineView`/`SplitView` iterators (lines.hpp)

---

## 4. Missing `SIMDTEXT_NODISCARD`

These functions return values that should not be discarded:

- `lowercase_ascii_inplace` / `uppercase_ascii_inplace` — void return, OK (in-place mutation). ✅
- `split_into` (str.hpp) — returns `size_t`, missing `SIMDTEXT_NODISCARD`
- `json_unescape_inplace` (json.hpp) — returns `size_t`, missing `SIMDTEXT_NODISCARD`
- `xml_escape_inplace` (xml.hpp) — returns `size_t`, missing `SIMDTEXT_NODISCARD`
- `each_line` / `each_line_containing` (file.hpp) — void, OK ✅

---

## 5. C API Issues

### Missing Bindings for New Modules

The C API (`include/simdtext/c/simdtext.h`) only covers: **scan, ascii, utf8, encode, url, file, pattern, parallel**.

**Completely missing from C API:**

| Module | Key Functions Missing |
|--------|----------------------|
| `hash` | `crc32`, `crc32c`, `xxhash64`, `wyhash`, `fnv1a` |
| `str` | `trim_left`, `trim_right`, `trim`, `replace_all`, `fields`, `split_vec`, `starts_with`, `ends_with`, `contains_char` |
| `json` | `JsonTokenizer`, `looks_like_json`, `is_json_number`, `json_unescape_inplace` |
| `csv` | `CsvParser`, `parse_csv_row` |
| `diff` | `line_diff`, `count_diff_lines`, `text_equal`, `common_prefix_length`, `common_suffix_length` |
| `log` | `parse_log_level`, `parse_log_line`, `count_log_levels`, `filter_log_lines` |
| `xml` | `XmlTokenizer`, `looks_like_xml`, `xml_escape_inplace` |
| `lines` | `LineView`, `SplitView`, `lines`, `split` |

### Thread Safety

- The C API functions are stateless (pure input→output) → **thread-safe** ✅
- `simdtext_file_t` is an opaque handle — **not thread-safe** for concurrent access to the same handle (documented nowhere)
- `simdtext_file_open` uses `new` — if OOM, returns nullptr. This is fine but not documented.

### Error Reporting

- C API uses **no error codes**. Functions return 0/nullptr on error with no way to distinguish error types.
- `simdtext_valid_utf8` returns `int` (0/1) — but returns 1 (true) for null pointer AND empty input. Empty input IS valid UTF-8, but null pointer is an error. Can't distinguish.
- `simdtext_is_ascii` returns 1 for null/empty — same problem.
- Functions returning `char*` (encode/decode) return nullptr on error — caller can't tell if it was null input, allocation failure, or decode error.
- No `simdtext_last_error()` or error code mechanism.

### Other C API Issues

- `simdtext_file_close` manually calls destructor then `std::free` — but `simdtext_file` was allocated with `new`. Should use `delete` instead of manual destructor + `free`. This is UB if `simdtext_file` has a non-trivial destructor or members with non-trivial destructors (it does — `std::string` in `MappedFile::Impl`).
- No `simdtext_base64_decode` / `simdtext_hex_decode` in C API (only encode).
- No `simdtext_url_decode_to` / `simdtext_url_encode_to` (buffer-writing variants).

---

## 6. Missing Features vs. Competitors

### vs. simdutf

| Feature | simdutf | simdtext |
|---------|---------|----------|
| UTF-8 ↔ UTF-16 conversion | ✅ `convert_utf8_to_utf16`, `convert_utf16_to_utf8` | ❌ Missing entirely |
| UTF-8 ↔ UTF-32 conversion | ✅ | ❌ Missing entirely |
| UTF-16 validation | ✅ | ❌ |
| Latin1 detection & conversion | ✅ | ❌ (only ASCII detection) |
| ASCII fallback detection | ✅ `detect_encoding` | ❌ |
| Validation with error position | ✅ `validate_utf8_with_errors` | ❌ (only bool result) |
| Counting UTF-16 code units | ✅ | ❌ |
| BOM detection/handling | ✅ | ❌ |
| `convert_valid_*` (assumes valid) | ✅ | ❌ |
| Implementation detection (which ISA) | ✅ `get_active_implementation` | ❌ (internal only in `detail`) |
| Batch conversion sizes | ✅ | ❌ |

### vs. ada (URL parsing)

| Feature | ada | simdtext |
|---------|-----|----------|
| Full URL parsing (scheme, host, port, path) | ✅ | ❌ (only encode/decode + query parse) |
| URL normalization | ✅ | ❌ |
| IDNA domain processing | ✅ | ❌ |
| URL resolution (relative → absolute) | ✅ | ❌ |

### vs. fast_float

| Feature | fast_float | simdtext |
|---------|-----------|----------|
| `from_chars` for float/double | ✅ | ❌ |
| `from_chars_advanced` with options | ✅ | ❌ |
| JSON number parsing | ✅ | ❌ (simdtext has `is_json_number` bool check only) |

---

## 7. Recommended Additions

### Priority 1 — Critical for Competitive Parity

1. **UTF-8 ↔ UTF-16/UTF-32 conversion** — this is simdutf's core feature and the #1 reason people reach for it. Without this, simdtext can't replace simdutf.
2. **Validation with error location** — `valid_utf8` returning `bool` is insufficient. Add `validate_utf8_with_errors()` returning `{bool valid, size_t error_offset}`.
3. **Fix C API UB** — `simdtext_file_close` must use `delete`, not manual destructor + `free`.
4. **Fix vector-returning decode functions** — `hex_decode` / `base64_decode` should return `DecodeResult` or use `expected<std::vector<std::byte>, ErrorCode>`.

### Priority 2 — API Quality

5. **Add `noexcept`** to all non-allocating pure functions (see Section 3).
6. **Standardize input types** — provide both `std::string_view` and `std::span<const char>` overloads, or pick one and be consistent.
7. **Add C API error reporting** — at minimum, a `simdtext_last_error()` function or error code return values.
8. **Add C bindings** for hash, str, json, csv, diff, log, xml, lines modules.
9. **Deprecate `utf8_length`** or document it as an alias for `count_code_points`.
10. **Move `hex_val`** to `detail` namespace.

### Priority 3 — Feature Expansion

11. **Latin1 detection** — `is_latin1(std::string_view)` checks all bytes < 0xC0.
12. **Encoding detection** — `detect_encoding()` returning an enum (ASCII, Latin1, UTF-8, Binary).
13. **Streaming/chunked APIs** for hash, encode, JSON, XML tokenizers.
14. **In-place variant** for `trim`, `replace_all`.
15. **`simdtext_get_active_implementation()`** — public API to query which SIMD path is active.
16. **BOM detection** — `has_bom()`, `strip_bom()`.

---

*End of audit.*

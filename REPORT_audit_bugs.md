# simdtext Security & Bug Audit Report

**Date:** 2026-05-02  
**Auditor:** simd (subagent)  
**Scope:** All source in `src/`, `include/simdtext/`, `CMakeLists.txt`  
**Codebase version:** 0.2.0 (6,233 lines)

---

## CRITICAL

### C-1: NEON `find_byte` is truncated — incomplete source file
- **File:** `src/detail/simd_neon.cpp:114–121`
- The `find_byte` function body starts at line 114 but the file ends at line 121 mid-expression (`uint8x16_t l`).
- The file is only 121 lines; the entire `find_byte`, `count_code_points`, and `validate_utf8` NEON implementations are missing or garbled.
- **Impact:** Build failure on ARM. If by some miracle it compiles, `find_byte` will crash or return garbage.
- **Fix:** Complete the NEON `find_byte` implementation. Add `validate_utf8` and `count_code_points` NEON implementations.

### C-2: `expected` polyfill — alignment of storage is wrong
- **File:** `include/simdtext/expected.hpp:90–91`
- `alignas(sizeof(T) > sizeof(E) ? sizeof(T) : sizeof(E))` uses **size** as alignment, but alignment and size are independent. `sizeof(T)` may not be a power of two and may be larger than `alignof(T)`.
- This can cause UB when accessing `storage_` via `reinterpret_cast<T*>`.
- **Fix:** Use only `alignas(alignof(T) > alignof(E) ? alignof(T) : alignof(E))`. Remove the second `alignas` that uses `sizeof`.

### C-3: `parallel_valid_utf8` splits multi-byte sequences at chunk boundaries
- **File:** `src/parallel/parallel.cpp:100–115`
- Each thread validates its chunk independently. A 3-byte UTF-8 sequence split across two chunks will be rejected as invalid by both threads, producing a false negative.
- **Impact:** Correct UTF-8 can be reported as invalid.
- **Fix:** Use `Utf8Validator` to carry state across chunk boundaries. Each thread's chunk must start by consuming any continuation bytes left from the previous chunk, or align chunks on non-continuation byte boundaries.

### C-4: C API `simdtext_file_close` uses `std::free` on `new`-allocated memory
- **File:** `src/c/c_api.cpp:117–121`
- `simdtext_file` is allocated with `new` but freed with `std::free()`. The destructor is called manually via `file->~simdtext_file()`, but `std::free` on a `new`-allocated object is UB — `new` and `malloc` need not use the same allocator.
- **Fix:** Use `delete file;` or use `std::malloc` + placement new.

---

## WARN

### W-1: `parallel_is_ascii` and `parallel_valid_utf8` — thread vector not resized after early-exit
- **File:** `src/parallel/parallel.cpp:43–56` (is_ascii), `104–115` (valid_utf8)
- If `found_non_ascii.load()` is true before spawning all threads, the loop `break`s early but `threads` may have fewer entries than `nthreads`. The subsequent `join()` loop is fine (iterates actual vector), but threads that were never started still have reserved capacity. Not a bug per se, but the `reserve(nthreads)` at line 53 means capacity is wasted.
- More importantly: the check `if (found_non_ascii.load())` before `emplace_back` is a TOCTOU race. Between the check and `emplace_back`, another thread may set the flag. The spawned thread then does a redundant check. This is harmless but wasteful.
- **Severity:** Low. Functional but slightly racy design.

### W-2: `hex_decode` ignores decode errors silently
- **File:** `src/encode/encode.cpp:93–96`
- `hex_decode` creates a vector of size `input.size()/2` and calls `hex_decode_to` but discards the `DecodeResult`. If the input has invalid hex characters, the returned vector contains partial/garbage data with no way for the caller to know.
- **Fix:** Either validate first, return `expected<vector<byte>, ErrorCode>`, or at minimum document that invalid input produces undefined output.

### W-3: `base64_decode` ignores decode errors silently
- **File:** `src/encode/encode.cpp:161–167`
- Same as W-2. `base64_decode` discards the `DecodeResult` and returns potentially garbage data.
- **Fix:** Same as W-2.

### W-4: `url_encode_to` — off-by-one in bounds check for `%XX` encoding
- **File:** `src/url/url.cpp:58`
- Check is `if (j + 2 >= out_size)` but needs 3 bytes (`%XX`). Should be `if (j + 3 > out_size)`.
- Current: if `j + 2 >= out_size`, returns 0. If `out_size == j + 3`, we proceed — that's correct (3 bytes available). If `out_size == j + 2`, we reject — correct (only 2 bytes). So actually `j + 2 >= out_size` is equivalent to `j + 3 > out_size`, which is correct.
- **Reclassifying to INFO** — actually correct, just confusing style.

### W-5: `Utf8Validator::validate` — missing surrogate and overlong checks for 3-byte sequences
- **File:** `src/scalar/scalar.cpp` (Utf8Validator::validate)
- The streaming validator has a comment "For simplicity, we just decrement and continue" for the 3-byte range checks (E0/A0, ED/9F), meaning it accepts overlong 3-byte sequences and UTF-8 encoded surrogates.
- The non-streaming `validate_utf8` and `validate_utf8_detailed` do check these correctly.
- **Impact:** `Utf8Validator` can accept invalid UTF-8 that the non-streaming validator rejects.
- **Fix:** Add the same range checks as in `validate_utf8_detailed`.

### W-6: `parallel_for_each_chunk` — callback receives misaligned chunks for multi-byte text
- **File:** `src/parallel/parallel.cpp:120–137`
- Chunks are split at byte boundaries, not at UTF-8 sequence boundaries. The callback receives a `string_view` that may start or end mid-sequence.
- **Impact:** Caller must handle partial sequences. Should be documented.
- **Fix:** Document the contract or add a UTF-8-aware chunk splitter.

### W-7: `ConfigParser` — zero-allocation claim violated by `std::vector<ConfigEntry>`
- **File:** `include/simdtext/config.hpp:13` (doc: "Zero-allocation INI-style config parser")
- `parse()` calls `entries_.clear()` then `entries_.push_back()` which allocates on first parse.
- **Fix:** Change documentation to "minimal-allocation" or pre-reserve.

### W-8: `CsvParser` — zero-allocation claim violated by `std::vector<std::string_view>`
- **File:** `include/simdtext/csv.hpp:17` (doc: "Zero-allocation CSV row iterator")
- `fields_` is a `std::vector` that allocates on first `push_back`.
- **Fix:** Same as W-7.

### W-9: `xml_escape_inplace` — extra count is wrong for `&`
- **File:** `src/xml/xml.cpp:89`
- `case '&': extra += 4;` — `&amp;` is 5 chars, replacing 1 char, so extra = 4. ✓
- `case '<': extra += 3;` — `&lt;` is 4 chars, extra = 3. ✓  
- `case '"': extra += 5;` — `&quot;` is 6 chars, extra = 5. ✓
- `case '\'': extra += 5;` — `&apos;` is 6 chars, extra = 5. ✓
- Actually all correct. Reclassifying to INFO.

### W-10: `common_prefix_length` SSE2 path — UB if data has different bytes in same 8-byte word
- **File:** `src/diff/diff.cpp:80–83`
- `__builtin_ctzll(diff) / 8` — if the first differing bit is not on a byte boundary (which it should be since `diff = va ^ vb`), this is fine. Actually `diff` has at least one bit set per differing byte, so `ctzll` finds the lowest differing bit, and dividing by 8 gives the byte offset. This is correct.
- **Reclassifying to INFO.**

### W-11: `crc32c` hardware path — transition from 64-bit to 32-bit CRC is incorrect
- **File:** `src/hash/hash.cpp:26–44`
- The code starts with `uint64_t crc = 0xFFFFFFFFFFFFFFFFULL` and uses `crc32q` (64-bit CRC32C). Then at line 35, it truncates to `uint32_t crc32_val = static_cast<uint32_t>(crc)`.
- The `crc32q` instruction operates on 64-bit data but still accumulates into a 64-bit register with CRC32C polynomial. The `static_cast<uint32_t>` discards the upper 32 bits, which are part of the CRC state.
- **Impact:** CRC32C results from the hardware path will be incorrect for inputs ≥ 8 bytes.
- **Fix:** Use `uint32_t` for the CRC accumulator from the start, and use `crc32q` with proper 32-bit operand (or process 8 bytes at a time using `crc32b` in a loop for correctness). The Intel CRC32 instructions always use a 32-bit destination register; the `crc32q` form processes 8 bytes of input but writes to a 64-bit register where the upper 32 bits are zero. So `static_cast<uint32_t>` is actually correct. **Reclassifying to INFO** — the code is correct; the 64-bit `crc` register's upper bits are always zero after `crc32q`.

### W-12: `simdtext_file` constructor initializes `mapped` twice
- **File:** `src/c/c_api.cpp:26`
- `simdtext_file` has `MappedFile mapped` and `FileScanner scanner`. The constructor initializes `scanner(path)` which internally creates a `MappedFile`, but `mapped` is default-constructed and never opened. `simdtext_file_data` returns `file->mapped.view()` which will always be empty.
- **Impact:** `simdtext_file_data()` always returns nullptr.
- **Fix:** Either open `mapped` in the constructor, or change `simdtext_file_data` to use `scanner`'s internal file.

---

## INFO

### I-1: Duplicated `hex_decode_table` in `encode.cpp` and `url.cpp`
- Both TUs define identical `hex_decode_table` arrays. Could be shared in a header or a common TU.

### I-2: `BytePattern` doc says "Zero-allocation" but uses `std::vector<uint8_t>`
- **File:** `include/simdtext/pattern.hpp`
- The doc comment says "Zero-allocation: the pattern references the caller's storage" but `bytes_` and `mask_` are `std::vector<uint8_t>`. Comment is misleading.

### I-3: `wyhash` implementation uses `__uint128_t` — GCC/Clang only
- **File:** `src/hash/hash.cpp:107`
- Will not compile on MSVC. The `wymix` lambda uses `__uint128_t`.

### I-4: `crc32c` uses `__asm__ __volatile__` — GCC/Clang only  
- **File:** `src/hash/hash.cpp:26–44`
- Inline asm won't compile on MSVC. Need `_mm_crc32_u64` intrinsics as fallback.

### I-5: `common_prefix_length` SSE2 guarded only by `#if defined(__SSE2__)`
- **File:** `src/diff/diff.cpp:72`
- If compiled without `-msse2`, `_mm_loadu_si128` etc. will fail. Should use the same per-TU compile flags as `simd_sse2.cpp`, or use `__builtin_cpu_supports` runtime check.

### I-6: Non-temporal stores without SFENCE
- **Files:** `src/detail/simd_sse2.cpp`, `src/detail/simd_avx2.cpp`
- `_mm_stream_si128` / `_mm256_stream_si256` are used for buffers > 2MB but there's no `_mm_sfence` after the loop. Subsequent scalar tail writes may be reordered before the non-temporal stores complete.
- **Impact:** On x86, this is technically only a problem if another thread reads the buffer immediately. Likely safe in practice for single-threaded use, but technically a memory ordering violation.

### I-7: `replace_all(char needle, char replacement)` has dead variable
- **File:** `src/str/str.cpp:22`
- `const char* end = find_byte(...)` is unused (the loop re-calls `find_byte` from position `i`).

### I-8: SSE2 `validate_utf8` — SIMD path falls back to scalar anyway
- **File:** `src/detail/simd_sse2.cpp`
- The SSE2 UTF-8 validator does a SIMD load but then processes each byte scalarly within the loop. The SIMD load serves only to check for all-ASCII chunks. No actual SIMD UTF-8 validation is performed.
- **Impact:** Correct but no faster than scalar for non-ASCII data. Misleading code structure.

### I-9: `parallel_find_byte` — relaxed memory ordering may miss earliest result
- **File:** `src/parallel/parallel.cpp:73–82`
- Uses `memory_order_relaxed` for the `compare_exchange_weak` on `earliest`. This is correct for single-variable atomic operations (there's no ordering requirement with other memory), but the `compare_exchange_weak` may spuriously fail, causing the thread to retry. This is fine but worth noting.

### I-10: C API returns allocated strings without length
- **File:** `include/simdtext/c/simdtext.h`
- Functions like `simdtext_hex_encode`, `simdtext_url_encode` return `char*` but don't provide the length. Callers must use `strlen()`, which is O(n) and incorrect if the string contains embedded nulls (unlikely for hex/base64/URL, but still an API smell).

---

## Summary

| Severity | Count |
|----------|-------|
| CRITICAL | 4     |
| WARN     | 4     |
| INFO     | 10    |

### Top Priority Fixes
1. **C-1:** Complete the NEON source file (build-breaking on ARM)
2. **C-3:** Fix parallel UTF-8 validation (correctness bug — false negatives)
3. **C-2:** Fix `expected` storage alignment (UB)
4. **C-4:** Fix C API free/delete mismatch (UB)
5. **W-5:** Fix `Utf8Validator` missing range checks (correctness)
6. **W-12:** Fix `simdtext_file_data` always returning nullptr (functional bug)
7. **W-2/W-3:** Make `hex_decode`/`base64_decode` report errors

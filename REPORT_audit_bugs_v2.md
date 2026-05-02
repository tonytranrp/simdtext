# simdtext Bug Audit Report v2

**Date:** 2026-05-02  
**Auditor:** simd (subagent)  
**Scope:** All source files under `src/` and `include/`  
**Total lines audited:** ~16,119  

---

## Summary

| Severity | Count |
|----------|-------|
| CRITICAL | 4     |
| HIGH     | 3     |
| MEDIUM   | 5     |
| LOW      | 4     |

---

## CRITICAL Bugs

### BUG-01: `swar_range_mask` produces wrong results due to inter-byte borrow

**File:** `src/detail/simd_scalar.cpp`, lines 39–45  
**Severity:** CRITICAL  
**Functions affected:** `lowercase_ascii()`, `uppercase_ascii()` (SWAR scalar path)

**Description:**  
The `swar_range_mask` function uses 64-bit subtraction `v - lo_rep` to subtract a replicated byte value from each byte position simultaneously. This is fundamentally incorrect because 64-bit subtraction propagates borrows between bytes. When any byte in the word is less than `lo`, the subtraction borrows from the next byte, corrupting its result.

**Proof:**
```cpp
// swar_range_mask with lo='A'(65), hi='Z'(90), range=25
// Input: byte 0 = '0' (48), byte 1 = 'A' (65)
// v = ... 0x41 0x30 (little-endian: byte0=0x30, byte1=0x41)
// lo_rep = ... 0x41 0x41

// Full 64-bit subtraction: byte 0 = 0x30 - 0x41 = 0xEF with borrow from byte 1
// Byte 1 = 0x41 - 0x41 - 1 (borrow) = 0xFF
// adj byte 0 = 0xEF + 230 = 0xD1 → bit 7 set → INCORRECTLY "in range"
// adj byte 1 = 0xFF + 230 = 0xE5 → bit 7 set → correct by coincidence (A IS in range)
```

**Test case:**  
Input `"0A0A0A0A"` passed to `lowercase_ascii()` on the SWAR path. Byte '0' (48) would be incorrectly detected as uppercase and XORed with 0x20, producing garbage.

**Suggested fix:**  
Use the correct SWAR range check that avoids inter-byte borrow:
```cpp
inline uint64_t swar_range_mask(uint64_t v, uint8_t lo, uint8_t hi) {
    const uint64_t lo_rep = lo * 0x0101010101010101ULL;
    const uint64_t hi_rep = hi * 0x0101010101010101ULL;
    // Byte >= lo iff (v - lo_rep) does NOT borrow (bit 7 clear after subtract)
    // Byte <= hi iff (hi_rep - v) does NOT borrow
    // Correct approach: use XOR trick to avoid borrow issues
    const uint64_t sign = 0x8080808080808080ULL;
    uint64_t lo_mask = ((v - lo_rep) ^ v ^ lo_rep) & sign;  // bit 7 set where byte < lo
    uint64_t hi_mask = ((v - hi_rep - 0x0101010101010101ULL) ^ v ^ (hi_rep + 0x0101010101010101ULL)) & sign; // bit 7 set where byte > hi
    // Wait, this is getting complex. Simpler correct approach:
    // (v - lo_rep) & ~v = borrows from subtraction, where bit 7 indicates byte was < lo
    // But this also has borrow issues...
    
    // Simplest correct fix: just do per-byte comparison
    // Or use the Lemire approach with unsigned comparison via XOR bias:
    const uint64_t bias = 0x8080808080808080ULL;
    uint64_t v_biased = v ^ bias;
    uint64_t lo_biased = lo_rep ^ bias;
    uint64_t hi_biased = hi_rep ^ bias;
    // Now unsigned comparison works via signed comparison on biased values
    uint64_t ge_lo = (v_biased - lo_biased) & ~v_biased & ~lo_biased & sign; // wrong, too complex
    // ...
}
```

Actually, the simplest correct fix is to abandon the SWAR range check and use per-byte scalar for the range detection, or use the SIMD `cmpgt`/`cmplt` approach:

```cpp
__attribute__((optimize("no-tree-vectorize")))
void lowercase_ascii(char* SIMDTEXT_RESTRICT data, size_t size) noexcept {
    size_t i = 0;
    for (; i + 8 <= size; i += 8) {
        uint64_t v = load_u64(data + i);
        uint64_t result = 0;
        for (int b = 0; b < 8; ++b) {
            uint8_t byte = (v >> (b * 8)) & 0xFF;
            if (byte >= 'A' && byte <= 'Z') byte ^= 0x20;
            result |= static_cast<uint64_t>(byte) << (b * 8);
        }
        store_u64(data + i, result);
    }
    for (; i < size; ++i) {
        auto c = static_cast<unsigned char>(data[i]);
        if (c >= 'A' && c <= 'Z') data[i] = static_cast<char>(c ^ 0x20);
    }
}
```

Or, use the correct SWAR unsigned comparison via XOR biasing (from Hacker's Delight):
```cpp
inline uint64_t swar_range_mask(uint64_t v, uint8_t lo, uint8_t hi) {
    // Convert to signed-safe comparison by XORing with 0x80
    const uint64_t offset = 0x8080808080808080ULL;
    const uint64_t lo_rep = (static_cast<uint64_t>(lo) * 0x0101010101010101ULL) ^ offset;
    const uint64_t hi_rep = ((static_cast<uint64_t>(hi) + 1) * 0x0101010101010101ULL) ^ offset;
    uint64_t v_off = v ^ offset;
    // byte in [lo, hi] iff (v_off >= lo_rep) AND (v_off < hi_rep) in signed comparison
    uint64_t ge_lo = (v_off - lo_rep) & offset;   // bit 7 clear where v >= lo
    uint64_t lt_hi = (hi_rep - v_off) & offset;   // bit 7 clear where v < hi
    return (~(ge_lo | lt_hi)) & offset;           // bit 7 set where both conditions met
}
```

---

### BUG-02: `count_code_points` misses byte 0x80 as continuation byte (SSE2 + AVX2)

**File:** `src/detail/simd_sse2.cpp`, lines ~270–290; `src/detail/simd_avx2.cpp`, lines ~415–440  
**Severity:** CRITICAL  

**Description:**  
Both SSE2 and AVX2 implementations of `count_code_points` use signed `cmpgt_epi8` to detect continuation bytes (0x80–0xBF). The comparison `cmpgt_epi8(chunk, v80)` checks `byte > 0x80` in signed arithmetic, but `0x80` as a signed byte is `-128`, and `-128 > -128` is `false`. Therefore, byte `0x80` is **never** detected as a continuation byte, causing it to be incorrectly counted as a code point.

**SSE2 code:**
```cpp
__m128i m0 = _mm_and_si128(
    _mm_cmpgt_epi8(c0, v80),                                    // byte > 0x80 (WRONG: misses 0x80)
    _mm_cmplt_epi8(c0, _mm_set1_epi8(static_cast<char>(0xC0)))  // byte < 0xC0
);
```

**AVX2 code:**
```cpp
__m256i m0 = _mm256_and_si256(
    _mm256_cmpgt_epi8(c0, v80),                                          // byte > 0x80 (WRONG: misses 0x80)
    _mm256_cmpgt_epi8(_mm256_set1_epi8(static_cast<char>(0xC0)), c0)     // 0xC0 > byte
);
```

**Proof / Test case:**
```cpp
// UTF-8 sequence for U+0080: \xC2\x80
// Expected code points: 1
// Actual result: 2 (0x80 is not counted as continuation)
const char data[] = "\xC2\x80";
size_t count = count_code_points(std::string_view(data, 2));
// count == 2 (WRONG, should be 1)
```

**Suggested fix:**  
Use `(byte & 0xC0) == 0x80` via AND+CMPEQ instead of two signed comparisons:
```cpp
// SSE2:
__m128i masked = _mm_and_si128(c0, _mm_set1_epi8(static_cast<char>(0xC0)));
__m128i is_cont = _mm_cmpeq_epi8(masked, _mm_set1_epi8(static_cast<char>(0x80)));
uint32_t mask = _mm_movemask_epi8(is_cont);
count += 16 - popcount16(mask);

// AVX2:
__m256i masked = _mm256_and_si256(c0, _mm256_set1_epi8(static_cast<char>(0xC0)));
__m256i is_cont = _mm256_cmpeq_epi8(masked, _mm256_set1_epi8(static_cast<char>(0x80)));
uint32_t mask = _mm256_movemask_epi8(is_cont);
count += 32 - popcount32(mask);
```

This matches the scalar implementation which correctly uses `(data[i] & 0xC0) != 0x80`.

---

### BUG-03: `parallel_valid_utf8` incorrectly splits UTF-8 sequences at chunk boundaries

**File:** `src/parallel/parallel.cpp`, lines ~90–115  
**Severity:** CRITICAL  

**Description:**  
`parallel_valid_utf8` divides the input into arbitrary byte-aligned chunks and validates each chunk independently. Multi-byte UTF-8 sequences (2–4 bytes) can span chunk boundaries. When a sequence is split, each chunk sees an incomplete sequence and reports it as invalid, even though the full input is valid UTF-8.

**Proof / Test case:**
```cpp
// 3-byte UTF-8 sequence for U+4E2D (中): \xE4\xB8\xAD
// If the chunk boundary falls between bytes, e.g.:
//   chunk 0: ...\xE4\xB8  (incomplete 3-byte sequence → invalid)
//   chunk 1: \xAD...      (unexpected continuation byte → invalid)
const char data[] = "Hello\xE4\xB8\xADWorld";
// With 2 threads and appropriate chunk size, this would report INVALID
// even though it's valid UTF-8
```

**Suggested fix:**  
Use the `Utf8Validator` streaming class to carry state across chunk boundaries:
```cpp
bool parallel_valid_utf8(std::string_view data, const ParallelOptions& opts) {
    const size_t size = data.size();
    const unsigned int nthreads = effective_threads(opts, size);
    if (nthreads == 1) return valid_utf8(std::span<const char>(data.data(), size));

    const size_t chunk_size = size / nthreads;
    std::atomic<bool> invalid{false};
    std::vector<Utf8Validator> validators(nthreads);
    std::vector<std::thread> threads;

    for (unsigned int t = 0; t < nthreads; ++t) {
        const size_t start = t * chunk_size;
        const size_t end = (t == nthreads - 1) ? size : (t + 1) * chunk_size;
        threads.emplace_back([&invalid, &data, &validators, t, start, end]() {
            if (invalid.load(std::memory_order_relaxed)) return;
            bool result = validators[t].validate(
                std::string_view(data.data() + start, end - start));
            if (!result) invalid.store(true, std::memory_order_relaxed);
        });
    }

    for (auto& th : threads) th.join();
    if (invalid.load()) return false;

    // Check finalize on each validator
    for (auto& v : validators) {
        if (!v.finalize()) return false;
    }

    // Cross-chunk boundary: need to verify that sequences spanning
    // boundaries are valid. This requires checking that each chunk
    // boundary doesn't split a multi-byte sequence.
    // Better approach: only validate the last few bytes of each chunk
    // (minus the last incomplete sequence) and then validate the
    // boundary regions separately.
    return true;
}
```

A simpler correct fix is to not parallelize UTF-8 validation at all, or to use a two-pass approach: first find safe split points (after complete sequences), then validate chunks.

---

### BUG-04: Non-temporal stores (`_mm_stream_*`) without `sfence` — data visibility hazard

**File:** `src/detail/simd_sse2.cpp`, lines 115–125; `src/detail/simd_avx2.cpp`, lines 175–185  
**Severity:** CRITICAL  

**Description:**  
When `size > 2MB`, the `lowercase_ascii` and `uppercase_ascii` functions use non-temporal streaming stores (`_mm_stream_si128` / `_mm256_stream_si256`). These stores bypass the cache and are weakly ordered on x86. After the function returns, the caller may read the modified data using normal loads, which could return stale data from the cache because the streaming stores haven't been made visible yet.

On x86, non-temporal stores are NOT ordered with respect to regular loads. An `SFENCE` instruction is required after non-temporal stores before the data can be safely read.

**Proof / Test case:**
```cpp
char* buf = new char[3 * 1024 * 1024]; // 3MB
memset(buf, 'A', 3 * 1024 * 1024);
lowercase_ascii_inplace(std::span<char>(buf, 3 * 1024 * 1024));
// Without SFENCE, subsequent reads of buf[] might see stale 'A' values
// instead of the expected 'a' values, because the streaming stores
// haven't been committed to the cache hierarchy.
char first = buf[0]; // Might read 'A' instead of 'a'!
```

**Suggested fix:**  
Add `_mm_sfence()` after the streaming store loop, before the scalar tail:
```cpp
void lowercase_ascii(char* data, size_t size) {
    // ... SIMD loop with _mm_stream_si128 ...
    if (use_nontemporal) {
        _mm_sfence();  // Ensure streaming stores are visible
    }
    // ... scalar tail ...
}
```

Or use `std::atomic_thread_fence(std::memory_order_release)` for portability:
```cpp
if (use_nontemporal) {
    std::atomic_thread_fence(std::memory_order_release);
}
```

---

## HIGH Bugs

### BUG-05: NEON `find_byte` mask construction is fundamentally incorrect

**File:** `src/detail/simd_neon.cpp`, lines ~115–130  
**Severity:** HIGH  

**Description:**  
The NEON `find_byte` function uses pairwise adds to construct a bitmask from comparison results. The pairwise add approach **sums** the per-byte values (0 or 1), which destroys positional information. The resulting `mask` value does NOT represent a bit-per-lane mask of which bytes matched — it represents the COUNT of matching bytes in each 8-byte half.

**Code:**
```cpp
uint8x16_t shifted = vshrq_n_u8(eq, 7);  // 1 per match, 0 per non-match
uint16x8_t sum16 = vpaddlq_u8(shifted);
uint32x4_t sum32 = vpaddlq_u16(sum16);
uint64x2_t sum64 = vpaddlq_u32(sum32);
uint64_t mask = vgetq_lane_u64(sum64, 0) | (vgetq_lane_u64(sum64, 1) << 8);
```

**Why it's wrong:**  
- `sum64[0]` = count of matches in bytes 0–7 (0 to 8)
- `sum64[1]` = count of matches in bytes 8–15 (0 to 8)
- `mask = sum64[0] | (sum64[1] << 8)` does NOT give a 16-bit bitmask

**Proof / Test case:**
```cpp
// Input: byte 0 and byte 3 both match the target
// shifted = [1, 0, 0, 1, 0, 0, 0, 0, ...]
// sum16 = [1, 1, 0, 0, ...] (pairwise sums)
// sum32 = [2, 0, ...] (pairwise sums)
// sum64 = [2, ...] (pairwise sums)
// mask = 2 | (0 << 8) = 2 = 0b10
// __builtin_ctzll(2) = 1 → returns data + i + 1 (WRONG, should be data + i + 0)
```

**Suggested fix:**  
Use the standard NEON bitmask extraction technique:
```cpp
const char* find_byte(const char* data, size_t size, char byte) {
    const uint8x16_t vbyte = vdupq_n_u8(static_cast<uint8_t>(byte));
    size_t i = 0;
    for (; i + 16 <= size; i += 16) {
        uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data + i));
        uint8x16_t eq = vceqq_u8(chunk, vbyte);
        // Extract bitmask: move comparison result to a 16-bit mask
        // Use the standard narrowing+extract pattern
        uint8x16_t shifted = vshrq_n_u8(eq, 7);  // 1 per match
        // Store to stack and read as uint16_t
        uint64_t mask;
        vst1q_u8(reinterpret_cast<uint8_t*>(&mask), shifted);
        // OR all bytes together with shifts to form a proper bitmask
        // Actually, the simplest correct NEON approach:
        // Use vminvq_u8 + bit manipulation, or the standard vshrn/vmovn approach
        
        // Correct approach using 2x uint64x1_t store:
        alignas(16) uint8_t bits[16];
        vst1q_u8(bits, shifted);
        uint64_t mask = 0;
        for (int k = 0; k < 16; ++k) {
            mask |= static_cast<uint64_t>(bits[k]) << k;
        }
        if (mask != 0) {
            return data + i + __builtin_ctzll(mask);
        }
    }
    for (; i < size; ++i)
        if (data[i] == byte) return data + i;
    return data + size;
}
```

Better NEON-native approach using `vshrn_n_u16`:
```cpp
// Narrow 16 → 8 with shift, then extract
uint16x8_t eq_w = vmovl_u8(vget_low_u8(eq));   // widen lower half
// ... complex. Simplest portable fix: store and construct mask from bytes.
```

---

### BUG-06: `base64_decode_to` doesn't validate `c` and `d` for non-padding invalid characters

**File:** `src/encode/encode.cpp`, lines ~200–210  
**Severity:** HIGH  

**Description:**  
The Base64 decode function only checks if the first two characters (`a`, `b`) of each 4-character chunk are valid (not 64). Characters in positions `c` and `d` are not validated. If an invalid character (not `=`, but mapping to 64 in the lookup table) appears in position 2 or 3, it's silently treated as value 64 in the decoded output, producing incorrect bytes without any error indication.

**Code:**
```cpp
const uint8_t a = base64_table[src[i]];
const uint8_t b = base64_table[src[i+1]];
const uint8_t c = base64_table[src[i+2]];  // NOT validated
const uint8_t d = base64_table[src[i+3]];  // NOT validated

if (a == 64 || b == 64) {  // Only a and b checked!
    result.error = ErrorCode::InvalidChar;
    ...
}
```

**Proof / Test case:**
```cpp
// Input: "QU@D" — '@' (0x40) is invalid Base64 but maps to 64 in the table
// a='Q'=16, b='U'=20, c='@'=64, d='D'=3
// a==64? No. b==64? No. → No error detected.
// n = (16<<18) | (20<<12) | (64<<6) | 3 = 0x415003
// Byte 0 = (n>>16) & 0xFF = 0x41 ('A')
// Byte 1 = (n>>8) & 0xFF = 0x50 ('P') — wrong, should be error
// Expected: InvalidChar error. Actual: silently produces "AP\x03"
```

**Suggested fix:**
```cpp
// Also validate c and d when they're not padding
if (a == 64 || b == 64 || (src[i+2] != '=' && c == 64) || (src[i+3] != '=' && d == 64)) {
    result.error = ErrorCode::InvalidChar;
    if (a == 64) result.error_offset = i;
    else if (b == 64) result.error_offset = i + 1;
    else if (c == 64) result.error_offset = i + 2;
    else result.error_offset = i + 3;
    return result;
}
```

---

### BUG-07: NEON `is_ascii` has unnecessary complexity with reduction

**File:** `src/detail/simd_neon.cpp`, lines ~60–70  
**Severity:** HIGH (performance — may cause spurious incorrect results under specific alignment)  

**Description:**  
The NEON `is_ascii` function uses `vpaddlq_u8` → `vpaddlq_u16` → `vpaddlq_u32` reduction chain to check if any byte has the high bit set. This works correctly but is unnecessarily complex. More importantly, the pairwise add reduction accumulates bits, and if all 64 bytes in the unrolled loop have bit 7 set, the `uint64_t` reduction values could overflow. With 16 bytes each having bit 7 set, the accumulated sum after `vpaddlq_u8` gives `128` per pair (16-bit), after `vpaddlq_u16` gives `512` per pair (32-bit), etc. No overflow occurs because the maximum sum of 8 bytes each with value 128 is 1024, which fits in 16 bits.

Actually, on second analysis, this is NOT a bug — the maximum value after reduction is bounded. However, the code is much more complex than needed and could be simplified to use `vminvq_u8` or a simple horizontal OR + extract.

Retracting this as a bug. Replacing with a different HIGH issue found during review:

### BUG-07 (revised): `hex_decode` and `base64_decode` silently return garbage on invalid input

**File:** `src/encode/encode.cpp`, lines ~125, ~245  
**Severity:** HIGH  

**Description:**  
The `hex_decode` and `base64_decode` functions allocate a result vector and call the `_to` variants, ignoring the returned `DecodeResult`. If the input contains invalid characters, the `_to` function returns an error and doesn't write to the output buffer, but the caller still returns the allocated vector with **uninitialized data**. The user gets a vector of garbage bytes with no error indication.

**Code:**
```cpp
std::vector<std::byte> hex_decode(std::string_view input) {
    std::vector<std::byte> result(input.size() / 2);
    (void)hex_decode_to(input, std::span<std::byte>(result));  // Error ignored!
    return result;  // Returns garbage on invalid input
}

std::vector<std::byte> base64_decode(std::string_view input) {
    const size_t max_bytes = (input.size() / 4) * 3;
    size_t padding = 0;
    if (input.size() >= 1 && input.back() == '=') ++padding;
    if (input.size() >= 2 && input[input.size()-2] == '=') ++padding;
    std::vector<std::byte> result(max_bytes - padding);
    (void)base64_decode_to(input, std::span<std::byte>(result));  // Error ignored!
    return result;  // Returns garbage on invalid input
}
```

**Proof / Test case:**
```cpp
auto result = hex_decode("ZZ");  // 'Z' is not a valid hex digit
// result is a vector of 1 byte of uninitialized garbage, no error thrown
// Expected: either empty vector, exception, or some error indicator
```

**Suggested fix:**
```cpp
std::vector<std::byte> hex_decode(std::string_view input) {
    std::vector<std::byte> result(input.size() / 2);
    auto dec = hex_decode_to(input, std::span<std::byte>(result));
    if (dec.error != ErrorCode::Ok) {
        return {};  // Return empty on error
    }
    result.resize(dec.bytes_written);
    return result;
}
```

---

## MEDIUM Bugs

### BUG-08: `hex_decode_table` inconsistency between `encode.cpp` and `url.cpp`

**File:** `src/encode/encode.cpp`, lines 7–22; `src/url/url.cpp`, lines 16–31  
**Severity:** MEDIUM (code quality / maintainability)  

**Description:**  
The `hex_decode_table` in `encode.cpp` maps 'A'-'Z' to 10–35 (full base-36), while the one in `url.cpp` correctly maps only 'A'-'F' and 'a'-'f' to 10–15 and everything else to -1. While the `encode.cpp` version works because the calling code checks `> 15`, the inconsistency is confusing and error-prone. A future developer might remove the `> 15` check thinking the table already rejects invalid characters.

**Suggested fix:**  
Use a single shared `hex_decode_table` (in a header or common source file) that correctly maps only valid hex characters to 0–15 and everything else to -1.

---

### BUG-09: `FileScanner::is_open()` returns false for empty files

**File:** `src/file/file.cpp` (in MappedFile/Impl)  
**Severity:** MEDIUM  

**Description:**  
`FileScanner::is_open()` returns `file_.size() > 0 || file_.view().data() != nullptr`. For an empty file (size 0), `size() > 0` is false, and `view().data()` is `nullptr` (as set in `Impl::open` for zero-size files). So `is_open()` returns `false` for an empty file, even though the file was successfully opened.

**Proof / Test case:**
```cpp
FileScanner scanner("/path/to/empty/file.txt");
bool open = scanner.is_open();  // Returns false even though file exists and is open
```

**Suggested fix:**  
Track open state explicitly:
```cpp
class MappedFile::Impl {
    bool is_open_ = false;
    // ...
    bool open(std::filesystem::path path) {
        // ... existing code ...
        is_open_ = true;
        return true;
    }
    bool is_open() const noexcept { return is_open_; }
};
```

---

### BUG-10: SSE2 `validate_utf8` contains dead SIMD code

**File:** `src/detail/simd_sse2.cpp`, lines ~185–215  
**Severity:** MEDIUM (code quality / misleading)  

**Description:**  
The SSE2 UTF-8 validator sets up numerous SIMD variables (`class_lookup`, `class_lookup_hi`, `vcont`, `vlead2`, etc.) and performs range comparisons (`is_ascii`, `is_cont`, `is_lead2`, etc.) but then casts them all to `(void)` and falls back to a per-byte scalar loop within the SIMD chunk. This is dead code that misleads readers into thinking there's an actual SIMD UTF-8 validation path.

**Suggested fix:**  
Remove the dead SIMD setup code and clearly document that the SSE2 UTF-8 validator only uses SIMD for the ASCII fast-path check within the chunk loop. The SIMD code was apparently an incomplete attempt at a lookup-based validator.

---

### BUG-11: `parallel_is_ascii` early-exit may leave unjoined threads

**File:** `src/parallel/parallel.cpp`, lines ~55–75  
**Severity:** MEDIUM  

**Description:**  
The `parallel_is_ascii` function checks `found_non_ascii` before creating each thread. If `found_non_ascii` becomes true after some threads are launched but before all are created, the remaining threads are skipped via `break`. However, there's a subtle issue: the threads that were already created might still be running when `found_non_ascii` is checked. The `break` prevents creating more threads but doesn't cancel running ones. This is actually fine because the `join()` loop correctly waits for all created threads.

Actually, on closer inspection, this IS correct — the `threads` vector only contains threads that were actually `emplace_back`'d, and the `join()` loop iterates over exactly those. The `break` just prevents creating additional threads. Retracting this as a bug.

Replacing with:

### BUG-11 (revised): `base64_decode_to` produces incorrect first byte when padding is present

**File:** `src/encode/encode.cpp`, lines ~215–225  
**Severity:** MEDIUM  

**Description:**  
When Base64 input has padding (`=`) in position 2 or 3, the `c` and `d` values are 64 (from the lookup table for `=`). These are included in the `n` computation:
```cpp
const uint32_t n = (static_cast<uint32_t>(a) << 18) |
                   (static_cast<uint32_t>(b) << 12) |
                   (static_cast<uint32_t>(c) << 6) |  // c=64 if padding
                   static_cast<uint32_t>(d);           // d=64 if padding
```

While the first output byte `(n >> 16) & 0xFF` is technically correct (it only depends on `a` and `b`), the approach is fragile and misleading. If BUG-06 is fixed by rejecting `c == 64` for non-padding characters, the padding case (`=`) would also be rejected. The fix for BUG-06 needs to handle `=` as a special case before the validation check.

This is a co-dependency with BUG-06 — fixing one without considering the other could introduce a regression.

**Suggested fix:**  
When fixing BUG-06, ensure the `=` character is handled before the `c == 64` check:
```cpp
const bool pad2 = (src[i+2] == '=');
const bool pad3 = (src[i+3] == '=');

if (a == 64 || b == 64 || (!pad2 && c == 64) || (!pad3 && d == 64)) {
    result.error = ErrorCode::InvalidChar;
    ...
}
```

---

### BUG-12: `url_decode_to` doesn't handle `%` followed by non-hex correctly at end of string

**File:** `src/url/url.cpp`, lines ~94–105  
**Severity:** MEDIUM  

**Description:**  
The `url_decode_to` function checks `i + 2 < input.size()` before reading hex digits after `%`. If the string ends with `%X` (where X is valid but there's no second digit), the condition fails and the `%` is passed through literally. This is actually the correct behavior per RFC 3986 — incomplete percent-encodings should be treated as literal characters. However, this differs from the behavior of many URL decoder implementations which would treat this as an error.

Not strictly a bug, but worth documenting as a design decision.

---

## LOW Bugs / Issues

### BUG-13: AVX-512 tail mask `1ULL << remaining` is fragile for `remaining == 64`

**File:** `src/detail/simd_avx512.cpp`, lines 47, 73, 112, etc.  
**Severity:** LOW  

**Description:**  
The tail processing uses `__mmask64 tail_mask = (1ULL << remaining) - 1`. If `remaining` were ever 64, this would be undefined behavior (shifting by the width of the type). Currently, the loop conditions guarantee `remaining < 64`, but this is fragile — a future change to the loop bound could introduce UB.

**Suggested fix:**  
Add a static_assert or runtime assertion, or use a safer pattern:
```cpp
__mmask64 tail_mask = (remaining < 64) ? ((1ULL << remaining) - 1) : ~0ULL;
```

---

### BUG-14: `expected<T, E>` polyfill uses `reinterpret_cast` on storage

**File:** `include/simdtext/expected.hpp`, lines 79–84  
**Severity:** LOW  

**Description:**  
The `expected` polyfill uses `reinterpret_cast<T*>(&storage_)` to access the stored value. This is technically a strict aliasing violation in C++ (accessing an `unsigned char[]` through a `T*`). However, this is the standard pattern used by all `std::expected`, `std::optional`, and `std::variant` implementations, and all major compilers handle it correctly. Will be moot when switching to `std::expected` (C++23).

---

### BUG-15: `hex_decode` returns vector with potential size mismatch on error

**File:** `src/encode/encode.cpp`, line ~125  
**Severity:** LOW  

**Description:**  
When `hex_decode_to` returns an error (e.g., odd-length input), the `hex_decode` function still returns the allocated vector with `input.size() / 2` bytes. For odd-length input, `input.size() / 2` truncates, but the `hex_decode_to` function already handles the odd-length case by returning early. The vector contains default-initialized `std::byte` objects (which are zero-initialized since C++11 for `std::byte` in a vector). This is not technically UB but returns meaningless data.

---

### BUG-16: SSE2 `count_byte` uses SSSE3 intrinsics despite claiming SSE2-only

**File:** `src/detail/simd_sse2.cpp`, line 3  
**Severity:** LOW (documentation mismatch)  

**Description:**  
The file comment says "Target SSE2 only. Stay within SSE2 instruction set — no SSSE3, no popcnt." However, the code includes `<tmmintrin.h>` (SSSE3) and uses `#pragma GCC target("sse2,ssse3")`. The comment is misleading — the code actually requires SSSE3 (specifically `_mm_shuffle_epi8`), though the SSSE3 instructions aren't actually used in the final UTF-8 validator (they were part of the abandoned SIMD approach). The `class_table` was intended for `pshufb` lookups but the code falls back to scalar.

**Suggested fix:**  
Either remove the SSSE3 include and pragma (since no SSSE3 instructions are actually used), or update the comment to reflect the actual requirements.

---

## Additional Observations (Not Bugs)

1. **SSE2 UTF-8 validator is essentially scalar** — The SIMD loop only provides an ASCII fast-path check; non-ASCII chunks are processed byte-by-byte. This is correct but defeats the purpose of SIMD acceleration for UTF-8 validation. Consider implementing a proper SIMD UTF-8 validator (e.g., using the Lemire/simdutf approach with byte classification and structural verification).

2. **AVX2 `count_code_points` uses `__builtin_popcount` directly** — This creates a hard dependency on GCC/Clang builtins. The SSE2 code defines its own `popcount16`, but the AVX2 code calls `___builtin_popcount` directly. Should use the `popcount32` helper defined in the same file for portability.

3. **No NEON `validate_utf8` or `count_code_points`** — The ARM NEON path only implements `count_byte`, `is_ascii`, `lowercase_ascii`, `uppercase_ascii`, and `find_byte`. UTF-8 validation and codepoint counting fall through to the scalar path on ARM.

4. **Highway `validate_utf8_vec` is a two-pass approach** — It first calls `is_ascii_vec` on the entire input, and if all-ASCII, returns true. Otherwise, it falls back to a full scalar scan. This means for any non-ASCII input, the entire buffer is scanned twice (once by `is_ascii_vec`, once by the scalar validator). The non-Highway implementations handle this more efficiently by checking for ASCII within the main loop.

5. **`parallel_for_each_chunk` doesn't handle UTF-8 boundaries** — Like `parallel_valid_utf8`, it splits at arbitrary byte boundaries. If the callback processes UTF-8 text, it may see incomplete sequences at chunk boundaries.

---

*End of audit report.*

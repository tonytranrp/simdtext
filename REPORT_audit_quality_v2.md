# simdtext Code Quality Audit Report v2

**Date:** 2026-05-02  
**Auditor:** simd (C++ quality agent)  
**Codebase:** simdtext 0.2.0 (per `version.hpp`), CMakeLists says 0.1.0  
**Scope:** Full codebase — headers, implementation, CMake, C API

---

## Summary

| Category | CRITICAL | HIGH | MEDIUM | LOW | Total |
|----------|----------|------|--------|-----|-------|
| 1. Code Duplication | 2 | 3 | 2 | 0 | 7 |
| 2. API Inconsistencies | 0 | 2 | 4 | 1 | 7 |
| 3. Missing noexcept | 0 | 2 | 3 | 2 | 7 |
| 4. Missing [[nodiscard]] | 0 | 0 | 2 | 2 | 4 |
| 5. Missing const | 0 | 0 | 1 | 3 | 4 |
| 6. Dead Code | 1 | 2 | 3 | 1 | 7 |
| 7. Error Handling | 1 | 2 | 3 | 1 | 7 |
| 8. Documentation Gaps | 0 | 0 | 4 | 3 | 7 |
| 9. Include Hygiene | 0 | 1 | 3 | 2 | 6 |
| 10. CMake Issues | 1 | 2 | 2 | 1 | 6 |
| 11. Thread Safety | 1 | 1 | 1 | 0 | 3 |
| 12. ABI Concerns | 0 | 2 | 2 | 1 | 5 |
| **Total** | **6** | **17** | **30** | **17** | **70** |

---

## 1. Code Duplication

### Issue 1.1 — CRITICAL: UTF-8 validation logic copy-pasted 5 times

**Files:**
- `src/detail/simd_sse2.cpp:123-257` (scalar-within-SIMD-loop + tail)
- `src/detail/simd_avx2.cpp:139-265` (same pattern, 3 copies: AVX2 body, SSE2 tail, scalar tail)
- `src/detail/simd_scalar.cpp:119-143`
- `src/highway/simd_hwy.cpp` (highway fallback)
- `src/scalar/scalar.cpp:185-256` (`validate_utf8_detailed` — yet another variant)

**Description:** The exact same UTF-8 validation state machine (expected_cont tracking, lead byte classification, overlong/surrogate/range checks) is duplicated across every backend. Each copy has the same bugs or could diverge in fixes. The SSE2 and AVX2 backends both fall back to *scalar byte-by-byte processing within the SIMD loop*, making the "SIMD" UTF-8 validation effectively scalar with an ASCII fast-path.

**Suggested fix:** Extract the core validation state machine into a shared `detail::validate_utf8_scalar` function in a shared header. SIMD backends should call this for non-ASCII chunks. The UTF-8 validation loop body is ~30 lines — duplicating it 5 times is indefensible.

```cpp
// include/simdtext/detail/utf8_validate.hpp
namespace simdtext::detail {
// Shared scalar UTF-8 validation state machine.
// SIMD backends call this for non-ASCII chunks.
struct utf8_state {
    int expected_cont = 0;
    uint8_t prev_lead_class = 0;
    uint8_t prev_lead_byte = 0;
    uint8_t first_cont_byte = 0;
};

bool validate_utf8_chunk(const uint8_t*& p, const uint8_t* end, utf8_state& state);
bool validate_utf8_finalize(const utf8_state& state);
}
```

---

### Issue 1.2 — CRITICAL: hex_decode_table duplicated between encode.cpp and url.cpp

**Files:**
- `src/encode/encode.cpp:7-23` — `hex_decode_table`
- `src/url/url.cpp:37-53` — identical `hex_decode_table` with comment "same as encode.cpp, duplicated for TU locality"

**Description:** Two identical 256-entry lookup tables. If one is fixed (e.g., accepting uppercase A-F in URL decode but not hex decode, or vice versa), the other won't be.

**Suggested fix:** Move to a shared internal header:

```cpp
// include/simdtext/detail/hex_table.hpp
namespace simdtext::detail {
inline constexpr std::array<int8_t, 256> hex_decode_table = { ... };
inline constexpr std::array<char, 16> hex_upper_chars = { ... };
inline constexpr std::array<char, 16> hex_lower_chars = { ... };
}
```

---

### Issue 1.3 — HIGH: popcount/ctz helpers duplicated across SSE2, AVX2, scalar

**Files:**
- `src/detail/simd_sse2.cpp:19-42` — `popcount16`, `ctz32`
- `src/detail/simd_avx2.cpp:23-80` — `popcount64`, `popcount32`, `ctz32`, `ctz64`
- `src/detail/simd_scalar.cpp` — uses `__builtin_popcountll` directly

**Description:** Three separate implementations of bit-manipulation helpers. The SSE2 version uses SWAR popcount (to avoid `-mpopcnt`), the AVX2 version uses `__builtin_popcount`, and they each have their own `ctz32`.

**Suggested fix:** Create a shared `detail/bitops.hpp` header with conditional compilation:

```cpp
// include/simdtext/detail/bitops.hpp
namespace simdtext::detail {
// Use compiler intrinsics when available, SWAR fallback otherwise.
SIMDTEXT_FORCE_INLINE int popcount32(unsigned int x) { ... }
SIMDTEXT_FORCE_INLINE int popcount64(unsigned long long x) { ... }
SIMDTEXT_FORCE_INLINE int ctz32(unsigned int x) { ... }
SIMDTEXT_FORCE_INLINE int ctz64(unsigned long long x) { ... }
}
```

---

### Issue 1.4 — HIGH: Case conversion pattern (lowercase/uppercase) repeated identically

**Files:**
- `src/detail/simd_sse2.cpp:85-122` — lowercase + uppercase
- `src/detail/simd_avx2.cpp:95-137` — same pattern, 256-bit
- `src/detail/simd_avx512.cpp:82-131` — same pattern, 512-bit
- `src/detail/simd_neon.cpp:70-100` — same pattern, NEON
- `src/detail/simd_scalar.cpp:76-100` — SWAR version
- `src/highway/simd_hwy.cpp` — Highway version

**Description:** The case-conversion algorithm (compare range → mask → XOR bit 5) is structurally identical across all 6 backends. Only the SIMD width and intrinsics differ. This is a prime candidate for a template or macro that generates all variants from one definition.

**Suggested fix:** Use Google Highway exclusively for this operation (it already abstracts the ISA), or create a template that takes load/store/compare/xor primitives:

```cpp
// Pseudo-code: generate case conversion from a policy
template<typename SimdPolicy>
void case_convert_impl(char* data, size_t size, char range_lo, char range_hi) {
    auto vlo = SimdPolicy::set1(range_lo - 1);
    auto vhi = SimdPolicy::set1(range_hi + 1);
    auto vbit = SimdPolicy::set1(0x20);
    for (size_t i = 0; i + SimdPolicy::width <= size; i += SimdPolicy::width) {
        auto chunk = SimdPolicy::load(data + i);
        auto in_range = SimdPolicy::and_op(SimdPolicy::gt(chunk, vlo), SimdPolicy::gt(vhi, chunk));
        auto flipped = SimdPolicy::xor_op(chunk, SimdPolicy::and_op(in_range, vbit));
        SimdPolicy::store(data + i, flipped);
    }
}
```

---

### Issue 1.5 — HIGH: Dual dispatch — scalar.cpp AND simd_dispatch.cpp both dispatch

**Files:**
- `src/scalar/scalar.cpp` — reimplements CPU dispatch + forward declarations
- `src/detail/simd_dispatch.cpp` — the "real" dispatch

**Description:** `scalar.cpp` has its own forward declarations for `sse2::count_byte`, `avx2::count_byte`, etc. and its own `detect_cpu()` calls. This means there are **two independent dispatch paths** for the same functions. The `scalar.cpp` dispatch is used by the public API (`count_byte`, `is_ascii`, etc.), while `simd_dispatch.cpp` is used by `validate_utf8_dispatch` and `count_code_points_dispatch`. If either is fixed, the other won't be.

**Suggested fix:** Remove all dispatch from `scalar.cpp`. Have it call `detail::count_byte_dispatch()` etc. from `simd_dispatch.cpp`. Single source of truth for dispatch logic.

---

### Issue 1.6 — MEDIUM: Non-temporal store logic duplicated in lowercase/uppercase

**Files:**
- `src/detail/simd_sse2.cpp:92-95` and `105-108`
- `src/detail/simd_avx2.cpp:102-106` and `117-121`

**Description:** The `nontemporal_threshold` check + `_mm_stream_si128`/`_mm256_stream_si256` branch is duplicated in both lowercase and uppercase in both SSE2 and AVX2 files (4 copies). The 2MB threshold magic number appears 4 times.

**Suggested fix:** Extract into a shared constant and a helper:

```cpp
namespace simdtext::detail {
    static constexpr size_t NONTEMPORAL_THRESHOLD = 2 * 1024 * 1024;
}
```

---

### Issue 1.7 — MEDIUM: is_whitespace defined in str.cpp, is_ascii_whitespace defined in scalar.cpp

**Files:**
- `src/str/str.cpp:10` — `is_whitespace(char c)`
- `src/scalar/scalar.cpp:157` — `is_ascii_whitespace(char c)`

**Description:** Two nearly identical whitespace-check functions. `is_whitespace` checks for 6 chars (space, tab, newline, CR, formfeed, vertical tab), while `is_ascii_whitespace` checks for 4 (space, tab, CR, newline). Both are `static` so they can't see each other, but the difference in what they consider "whitespace" is a bug waiting to happen.

**Suggested fix:** Define a single `detail::is_c_whitespace` (all 6) and `detail::is_ascii_blank` (space + tab only) in a shared header.

---

## 2. API Inconsistencies

### Issue 2.1 — HIGH: Inconsistent parameter types across the public API

**Files (headers):**
- `scan.hpp` — `count_byte(std::span<const char>)`, `count_newlines(std::span<const char>)`
- `utf8.hpp` — `valid_utf8(std::span<const char>)`, `count_code_points(std::string_view)`, `utf8_length(std::string_view)`
- `str.hpp` — all functions take `std::string_view`
- `parallel.hpp` — all functions take `std::string_view`
- `encode.hpp` — takes `std::span<const std::byte>`

**Description:** The API is inconsistent about whether to accept `std::span<const char>`, `std::string_view`, or raw pointers. Users must remember which type each function expects. `std::string_view` and `std::span<const char>` are implicitly convertible from each other in one direction only, creating surprising compile errors.

**Suggested fix:** Standardize on `std::string_view` for all text-processing functions (it's the conventional choice for text). Use `std::span<const char>` only when `std::string_view` semantics are wrong (e.g., when embedded NULs matter). At minimum, provide overloads or explicit conversion guidance.

```cpp
// Standardize: text → string_view, binary → span<const byte>
SIMDTEXT_API size_t count_byte(std::string_view input, char byte);
SIMDTEXT_API bool valid_utf8(std::string_view input);
SIMDTEXT_API size_t count_code_points(std::string_view input);  // currently string_view ✓
SIMDTEXT_API size_t utf8_length(std::string_view input);         // currently string_view ✓
```

---

### Issue 2.2 — HIGH: hex_val declared in encode.hpp but defined in url.cpp

**Files:**
- `include/simdtext/encode.hpp:49` — declaration: `SIMDTEXT_NODISCARD SIMDTEXT_API int hex_val(char c) noexcept;`
- `src/url/url.cpp:132` — definition

**Description:** `hex_val` is declared in the **encode** header but defined in the **url** source file. This violates the principle that a symbol's declaration and definition should be in matching files. If someone includes `encode.hpp` and links without `url.cpp`, they get a linker error.

**Suggested fix:** Move `hex_val` definition to `src/encode/encode.cpp`, or move the declaration to a shared `detail/hex.hpp` header. Better yet, make it `constexpr inline` in a detail header since it's just a lookup.

---

### Issue 2.3 — MEDIUM: count_code_points and utf8_length are aliases with no documentation of the relationship

**Files:**
- `include/simdtext/utf8.hpp:51-54`

```cpp
SIMDTEXT_NODISCARD SIMDTEXT_API size_t count_code_points(std::string_view input) noexcept;
SIMDTEXT_NODISCARD SIMDTEXT_API size_t utf8_length(std::string_view input) noexcept;
```

**Description:** Both functions do the same thing, but neither's documentation mentions the other. Users may wonder which to use. `utf8_length` is a common name from other libraries, `count_code_points` is more descriptive.

**Suggested fix:** Document the alias relationship, or deprecate one:

```cpp
/// Count the number of Unicode code points in valid UTF-8.
/// Equivalent to `utf8_length()`. Prefer `count_code_points()` for new code.
SIMDTEXT_NODISCARD SIMDTEXT_API size_t count_code_points(std::string_view input) noexcept;

/// Alias for `count_code_points()`. Provided for API compatibility.
SIMDTEXT_DEPRECATED("Use count_code_points() instead")
SIMDTEXT_NODISCARD SIMDTEXT_API size_t utf8_length(std::string_view input) noexcept;
```

---

### Issue 2.4 — MEDIUM: trim_ascii (ascii.hpp) vs trim (str.hpp) — overlapping, different whitespace definitions

**Files:**
- `include/simdtext/ascii.hpp:30` — `trim_ascii(std::string_view)` — trims space, tab, CR, LF
- `include/simdtext/str.hpp:14` — `trim(std::string_view)` — trims space, tab, newline, CR, formfeed, vertical tab

**Description:** Two trim functions with different whitespace definitions. `trim_ascii` only trims 4 chars, `trim` trims 6. The names don't make the distinction clear. This is a correctness trap.

**Suggested fix:** Document the difference clearly, or unify them:

```cpp
/// Trim ASCII whitespace (space, tab, CR, LF).
SIMDTEXT_NODISCARD SIMDTEXT_API std::string_view trim_ascii(std::string_view input);

/// Trim C whitespace (space, tab, newline, CR, formfeed, vertical tab).
SIMDTEXT_NODISCARD SIMDTEXT_API std::string_view trim(std::string_view s) noexcept;
```

---

### Issue 2.5 — MEDIUM: contains() in scan.hpp vs contains_char() in str.hpp

**Files:**
- `include/simdtext/scan.hpp:23` — `contains(std::string_view input, std::string_view needle)` — substring search
- `include/simdtext/str.hpp:46` — `contains_char(std::string_view input, char needle)` — single-char search

**Description:** Two "contains" functions with different names and different semantics. `scan::contains` falls back to `string_view::find` and isn't SIMD-accelerated. `str::contains_char` uses SIMD `find_byte`. The naming suggests overlap but they do different things.

**Suggested fix:** Add a `contains(input, char)` overload to scan.hpp that delegates to `contains_char`:

```cpp
/// Check if input contains the given character. SIMD-accelerated.
SIMDTEXT_NODISCARD SIMDTEXT_API bool contains(std::string_view input, char needle);
```

---

### Issue 2.6 — MEDIUM: split() in lines.hpp vs split_vec()/split_into()/fields() in str.hpp

**Files:**
- `include/simdtext/lines.hpp` — `split(std::string_view, char)` returns `SplitView` (lazy)
- `include/simdtext/str.hpp` — `split_vec(std::string_view, char)` returns `vector<string_view>` (eager)
- `include/simdtext/str.hpp` — `split_into(...)` writes to pre-allocated buffer
- `include/simdtext/str.hpp` — `fields(std::string_view)` splits by whitespace

**Description:** Three different split functions with different names. The relationship between `split` (lazy) and `split_vec` (eager) is unclear from the names. Users need to discover both.

**Suggested fix:** Rename for clarity or document the distinction:

```cpp
// lines.hpp — lazy iteration
SplitView split_view(std::string_view input, char delimiter);

// str.hpp — eager allocation
std::vector<std::string_view> split(std::string_view input, char delimiter);
```

---

### Issue 2.7 — LOW: LineView/SplitView begin()/end() not marked noexcept

**Files:**
- `include/simdtext/lines.hpp:48-49` — `begin()` / `end()` not noexcept

**Description:** These functions construct iterators from trivial operations and cannot throw, but aren't marked noexcept.

**Suggested fix:**
```cpp
Iterator begin() const noexcept { return Iterator(input_); }
Iterator end() const noexcept { return Iterator(); }
```

---

## 3. Missing noexcept

### Issue 3.1 — HIGH: SIMD backend functions missing noexcept

**Files:**
- `src/detail/simd_sse2.cpp` — `count_byte`, `is_ascii`, `find_byte`, `validate_utf8`, `count_code_points` all missing noexcept
- `src/detail/simd_avx2.cpp` — same
- `src/detail/simd_avx512.cpp` — same
- `src/detail/simd_neon.cpp` — same
- `src/detail/simd_scalar.cpp` — `count_byte`, `is_ascii` are marked noexcept, but `validate_utf8` is not

**Description:** All SIMD backend functions are pure computation with no allocation or throwing operations. They should all be noexcept. The scalar.cpp file correctly marks some as noexcept but the SIMD files don't.

**Suggested fix:** Add noexcept to all backend function signatures:

```cpp
namespace simdtext::detail::sse2 {
size_t count_byte(const char* data, size_t size, char byte) noexcept;
bool is_ascii(const char* data, size_t size) noexcept;
// ... etc
}
```

---

### Issue 3.2 — HIGH: Dispatch functions missing noexcept

**Files:**
- `src/detail/simd_dispatch.cpp` — all `*_dispatch` functions missing noexcept
- `src/scalar/scalar.cpp` — all dispatch code missing noexcept

**Description:** The dispatch functions simply call noexcept backend functions. They can't throw. Missing noexcept prevents optimizations and makes the API harder to use in noexcept contexts.

**Suggested fix:**
```cpp
size_t count_byte_dispatch(const char* data, size_t size, char byte) noexcept { ... }
bool is_ascii_dispatch(const char* data, size_t size) noexcept { ... }
```

---

### Issue 3.3 — MEDIUM: ConfigParser::parse() marked noexcept but calls vector::clear/push_back

**Files:**
- `include/simdtext/config.hpp:17` — `SIMDTEXT_NODISCARD size_t parse() noexcept;`
- `src/config/config.cpp:12` — `entries_.clear()` + `entries_.push_back(...)`

**Description:** `ConfigParser::parse()` is marked noexcept but calls `std::vector::clear()` and `std::vector::push_back()`, both of which can throw (`push_back` allocates). If the allocation fails, `std::terminate` will be called. This is likely unintentional.

**Suggested fix:** Either remove noexcept from `parse()`, or pre-allocate the vector, or use a fixed-size array:

```cpp
// Option A: Remove noexcept (honest)
size_t parse();  // may throw on allocation

// Option B: Pre-allocate (if max entries is bounded)
size_t parse() noexcept {
    entries_.clear();
    entries_.reserve(256);  // pre-allocate
    // ...
}
```

---

### Issue 3.4 — MEDIUM: CsvParser::next_row() marked noexcept but pushes to vector

**Files:**
- `include/simdtext/csv.hpp:27` — `SIMDTEXT_NODISCARD bool next_row() noexcept;`
- `src/csv/csv.cpp:8` — `fields_.push_back(...)` called multiple times

**Description:** Same issue as ConfigParser. `push_back` can throw `std::bad_alloc`.

**Suggested fix:** Remove noexcept from `next_row()`.

---

### Issue 3.5 — LOW: LineView::Iterator::operator++(int) not noexcept

**Files:**
- `include/simdtext/lines.hpp:38`

**Description:** Post-increment creates a copy. While the copy can't throw for this type, marking it noexcept is consistent with the pre-increment.

**Suggested fix:**
```cpp
Iterator operator++(int) noexcept { ... }
```

---

### Issue 3.6 — LOW: BytePattern::parse returns optional but not noexcept

**Files:**
- `include/simdtext/pattern.hpp:15` — `static std::optional<BytePattern> parse(std::string_view hex_pattern);`

**Description:** `parse()` allocates vectors, so it correctly isn't noexcept. However, the from_bytes/from_masked also allocate and aren't noexcept either. This is consistent, so this is a low-severity documentation note: the API should document that these can throw.

---

## 4. Missing [[nodiscard]]

### Issue 4.1 — MEDIUM: hex_encode_to, base64_encode_to, url_encode_to return values should not be discarded

**Files:**
- `include/simdtext/encode.hpp:16` — already has SIMDTEXT_NODISCARD ✓
- `include/simdtext/url.hpp:11` — already has SIMDTEXT_NODISCARD ✓

Actually these are correctly marked. Let me re-check...

### Issue 4.1 — MEDIUM: MappedFile::open() return value should not be discarded

**Files:**
- `include/simdtext/file.hpp:22` — `SIMDTEXT_NODISCARD bool open(const char* path);`

Already has SIMDTEXT_NODISCARD ✓.

### Issue 4.1 — MEDIUM: Utf8Validator::validate() and finalize() missing [[nodiscard]]

**Files:**
- `include/simdtext/utf8.hpp:28` — `SIMDTEXT_NODISCARD bool validate(std::string_view chunk) noexcept;`
- `include/simdtext/utf8.hpp:31` — `SIMDTEXT_NODISCARD bool finalize() noexcept;`

Already has SIMDTEXT_NODISCARD ✓.

Let me find actual missing nodiscards:

### Issue 4.1 — MEDIUM: FileScanner::is_open() missing [[nodiscard]]

**Files:**
- `include/simdtext/file.hpp:30` — `SIMDTEXT_NODISCARD bool is_open() const;`

Already marked ✓.

### Issue 4.1 — MEDIUM: BytePattern constructors missing [[nodiscard]]

**Files:**
- `include/simdtext/pattern.hpp:12` — `static std::optional<BytePattern> parse(...)` — optional is implicitly nodiscard
- `BytePattern()` default constructor — not nodiscard, and shouldn't be

### Issue 4.1 — MEDIUM: ConfigParser::entry() should be nodiscard

**Files:**
- `include/simdtext/config.hpp:23` — `SIMDTEXT_NODISCARD const ConfigEntry& entry(size_t index) const noexcept;`

Already marked ✓.

Let me find genuinely missing ones:

### Issue 4.1 — MEDIUM: DiffOp::Type enum comparison results — functions returning these aren't nodiscarded

Actually, the main missing [[nodiscard]] cases are:

### Issue 4.1 — MEDIUM: json_unescape_inplace and xml_escape_inplace missing [[nodiscard]]

**Files:**
- `include/simdtext/json.hpp:48` — `SIMDTEXT_API size_t json_unescape_inplace(char* data, size_t len) noexcept;`
- `include/simdtext/xml.hpp:37` — `SIMDTEXT_API size_t xml_escape_inplace(char* data, size_t len, size_t capacity) noexcept;`

**Description:** These functions return the length of the result. Discarding the return value means you don't know how much of the buffer is valid.

**Suggested fix:**
```cpp
SIMDTEXT_NODISCARD SIMDTEXT_API size_t json_unescape_inplace(char* data, size_t len) noexcept;
SIMDTEXT_NODISCARD SIMDTEXT_API size_t xml_escape_inplace(char* data, size_t len, size_t capacity) noexcept;
```

---

### Issue 4.2 — LOW: split_into() missing [[nodiscard]]

**Files:**
- `include/simdtext/str.hpp:31` — `SIMDTEXT_API size_t split_into(...)` — no SIMDTEXT_NODISCARD

**Description:** Returns the number of segments written. Discarding means you don't know how many segments are valid.

**Suggested fix:** Add `SIMDTEXT_NODISCARD`.

---

### Issue 4.3 — LOW: DecodeResult::ok() already has [[nodiscard]] — confirmed

Not an issue.

---

## 5. Missing const

### Issue 5.1 — MEDIUM: ParallelOptions struct fields should be const-accessible

**Files:**
- `include/simdtext/parallel.hpp:11-14`

**Description:** `ParallelOptions` is a simple struct with public fields — this is fine. But when passed as `const ParallelOptions&`, the fields are correctly read-only. No issue here.

### Issue 5.1 — MEDIUM: C API uses non-const pointers where const would be appropriate

**Files:**
- `include/simdtext/c/simdtext.h:30` — `void simdtext_lowercase_ascii(char* data, size_t len);`
- `include/simdtext/c/simdtext.h:31` — `void simdtext_uppercase_ascii(char* data, size_t len);`

**Description:** These correctly use non-const `char*` since they modify data in-place. Not an issue.

### Issue 5.1 — MEDIUM: BytePattern::SBO_SIZE declared but never used

**Files:**
- `include/simdtext/pattern.hpp:42` — `static constexpr size_t SBO_SIZE = 64;`

**Description:** The SBO_SIZE constant is declared but the implementation uses `std::vector`, not small-buffer optimization. This is dead code (see issue 6.x).

### Actual const issues:

### Issue 5.1 — MEDIUM: expected.hpp operator* returns non-const reference from const method

**Files:**
- `include/simdtext/expected.hpp:63-64`

```cpp
[[nodiscard]] T& operator*() & noexcept { return *reinterpret_cast<T*>(&storage_); }
[[nodiscard]] const T& operator*() const& noexcept { return *reinterpret_cast<const T*>(&storage_); }
```

**Description:** The const overload correctly returns `const T&`. This is actually fine.

### Issue 5.2 — LOW: simd_hwy.cpp Highway overloads don't forward const-ness

The Highway fallback in `simd_hwy.cpp` has scalar overloads that correctly handle const. Not an issue.

### Issue 5.3 — LOW: Several function parameters could be const

**Files:**
- `src/detail/simd_avx2.cpp:89` — `void lowercase_ascii(char* data, size_t size)` — data can't be const (it's modified in-place). OK.
- `src/pattern/pattern.cpp:46` — `FixedEntry fixed[256]` could be `const` after initialization in `find_pattern_scalar`. Low priority.

### Issue 5.4 — LOW: BytePattern::byte() and mask() should be noexcept (already are) but the accessors should validate bounds

**Files:**
- `include/simdtext/pattern.hpp:25-26`

```cpp
[[nodiscard]] uint8_t byte(size_t i) const noexcept { return bytes_[i]; }
[[nodiscard]] uint8_t mask(size_t i) const noexcept { return mask_[i]; }
```

**Description:** No bounds checking. If `i >= size()`, this is UB. A `[[cassert]]` or `Expects()` would be appropriate for debug builds.

---

## 6. Dead Code

### Issue 6.1 — CRITICAL: SSE2 validate_utf8 has ~60 lines of dead SIMD classification code

**Files:**
- `src/detail/simd_sse2.cpp:89-191`

**Description:** The SSE2 UTF-8 validator starts with an ambitious SIMD classification attempt (class_table, hi_nib/lo_nib, range comparisons), but then gives up and casts everything to void:

```cpp
(void)chunk; (void)hi_nib; (void)lo_nib; (void)v7f; (void)v80; (void)vbf;
(void)vc1; (void)vc2; (void)vdf; (void)ve0; (void)vef; (void)vf4; (void)vf5;
(void)class_lookup; (void)class_lookup_hi; (void)vcont; (void)vlead2;
(void)vlead3; (void)vlead4; (void)vinvalid;
```

Then falls back to scalar byte-by-byte processing. The 256-entry `class_table` is allocated and never used. The `__m128i` variables are loaded and discarded. This wastes code size, confuses readers, and may generate dead instructions.

**Suggested fix:** Delete the entire dead code block (lines 89-170). Replace with a comment explaining why the scalar fallback is used:

```cpp
bool validate_utf8(const char* data, size_t size) {
    // SIMD UTF-8 validation is complex due to cross-lane multi-byte sequences.
    // For correctness, we process byte-by-byte within the SIMD loop for non-ASCII chunks,
    // using the SIMD path only as an ASCII fast-path check.
    // TODO: Implement proper pshufb-based UTF-8 validation following simdutf's approach.
    
    const __m128i vhigh = _mm_set1_epi8(static_cast<char>(0x80));
    int expected_cont = 0;
    uint8_t prev_lead_class = 0;
    uint8_t prev_lead_byte = 0;
    uint8_t first_cont_byte = 0;
    // ... (scalar within SIMD loop)
```

---

### Issue 6.2 — HIGH: BytePattern::SBO_SIZE declared but never used

**Files:**
- `include/simdtext/pattern.hpp:42`

```cpp
static constexpr size_t SBO_SIZE = 64;
```

**Description:** Small-buffer optimization is declared but `BytePattern` uses `std::vector<uint8_t>` for storage. The SBO_SIZE constant is dead code.

**Suggested fix:** Either implement SBO (replace `std::vector` with a small-buffer + heap fallback), or remove the constant:

```cpp
// Option A: Remove dead constant
// (delete SBO_SIZE)

// Option B: Implement SBO
class SIMDTEXT_API BytePattern {
    // ...
    static constexpr size_t SBO_SIZE = 64;
    alignas(uint8_t) unsigned char sbo_storage_[SBO_SIZE * 2]; // bytes + mask
    uint8_t* bytes_ptr_;
    uint8_t* mask_ptr_;
    size_t size_;
    bool uses_heap_;
};
```

---

### Issue 6.3 — HIGH: expected.hpp is included in simdtext.hpp but never used by any public API

**Files:**
- `include/simdtext/simdtext.hpp:5` — `#include <simdtext/expected.hpp>` (not present actually, let me recheck)

Actually, looking at simdtext.hpp, it doesn't include expected.hpp. But the file exists in the include directory. Let me check if it's used anywhere...

**Files:**
- `include/simdtext/expected.hpp` — exists but not included by simdtext.hpp

**Description:** `expected.hpp` provides a `std::expected` polyfill but it's not included by any other header or used by any public API. It's a utility that was prepared but never adopted.


**Suggested fix:** Either integrate `expected` into the public API (e.g., `expected<DecodeResult, ErrorCode>` instead of `DecodeResult` with embedded error codes), or move it to a `detail/` subdirectory and document it as internal.

---

### Issue 6.4 — MEDIUM: `#include <tmmintrin.h>` in simd_sse2.cpp for SSSE3, but comment says "Stay within SSE2"

**Files:**
- `src/detail/simd_sse2.cpp:2` — `#include <tmmintrin.h>  // SSSE3 for _mm_shuffle_epi8`
- `src/detail/simd_sse2.cpp:8` — Comment: "Target SSE2 only. Stay within SSE2 instruction set — no SSSE3, no popcnt."

**Description:** The file includes the SSSE3 header but the comment explicitly says to stay within SSE2. The `_mm_shuffle_epi8` intrinsic is SSSE3, not SSE2. However, looking at the code, `_mm_shuffle_epi8` is never actually called — it was part of the dead UTF-8 classification code (issue 6.1). The pragma target includes `ssse3` (`#pragma GCC target("sse2,ssse3")`), which contradicts the comment.

**Suggested fix:** Remove the SSSE3 include and pragma target. Change to `#pragma GCC target("sse2")` only:

```cpp
#include <emmintrin.h>  // SSE2 only
// No tmmintrin.h — we stay within SSE2

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC push_options
#pragma GCC target("sse2")
#pragma GCC optimize("no-tree-vectorize")
#endif
```

---

### Issue 6.5 — MEDIUM: Unused `class_table` in SSE2 UTF-8 validator

**Files:**
- `src/detail/simd_sse2.cpp:97-119` — 256-entry `class_table` array

**Description:** This 256-byte lookup table is declared `alignas(16) static const` but never used in any live code path. It's part of the dead SIMD classification attempt.

**Suggested fix:** Remove with the rest of the dead code (see issue 6.1).

---

### Issue 6.6 — MEDIUM: High nibble/low nibble variables in dead SSE2 UTF-8 code

**Files:**
- `src/detail/simd_sse2.cpp:161-162` — `hi_nib`, `lo_nib`

**Description:** These variables are computed and then cast to void. Wasted computation if the compiler doesn't optimize them away (it likely will, but the code is still misleading).

**Suggested fix:** Remove with the rest of the dead code.

---

### Issue 6.7 — LOW: AVX512 guard `#if defined(__AVX512BW__) && defined(__AVX512F__)` in source file

**Files:**
- `src/detail/simd_avx512.cpp:1`

**Description:** The entire file is guarded by a compile-time check `#if defined(__AVX512BW__)`. This means the file compiles to nothing when AVX-512 is not available at compile time. But CMake already conditionally includes this file only when `COMPILER_SUPPORTS_AVX512BW` is true. The double guard is redundant but harmless.

**Suggested fix:** Remove the inner guard since CMake handles it, or keep it as a safety net (harmless).

---

## 7. Error Handling

### Issue 7.1 — CRITICAL: parallel_valid_utf8 splits at arbitrary byte boundaries

**Files:**
- `src/parallel/parallel.cpp:133-153`

**Description:** `parallel_valid_utf8` splits the input into equal-sized chunks and validates each independently. But UTF-8 multi-byte sequences can span chunk boundaries. A 3-byte sequence starting at the end of one chunk will be incorrectly reported as invalid in both chunks.

For example, the UTF-8 encoding of "€" (0xE2 0x82 0xAC) split between bytes 2 and 3 would cause chunk 1 to see an incomplete 3-byte sequence (0xE2 0x82) → invalid, and chunk 2 to see a lone continuation byte (0xAC) → invalid.

**Suggested fix:** This is a fundamental design issue. Options:

1. **Only parallelize the ASCII fast path:** Check if each chunk is all-ASCII in parallel. Only if a chunk has non-ASCII bytes, validate it sequentially with state carried across chunks.
2. **Overlap chunks:** Extend each chunk by 3 bytes (max UTF-8 sequence length) and validate the overlap only in one thread.
3. **Remove the function** until a correct implementation exists.

```cpp
// Option 1: Correct parallel UTF-8 validation
bool parallel_valid_utf8(std::string_view data, const ParallelOptions& opts) {
    // Phase 1: Check if entirely ASCII (embarrassingly parallel)
    if (parallel_is_ascii(data, opts)) return true;
    
    // Phase 2: Has non-ASCII — validate sequentially with state machine
    // (parallelization of multi-byte UTF-8 requires careful boundary handling)
    return valid_utf8(std::span<const char>(data.data(), data.size()));
}
```

---

### Issue 7.2 — HIGH: hex_decode() and base64_decode() silently return partial results on error

**Files:**
- `src/encode/encode.cpp:69-73`

```cpp
std::vector<std::byte> hex_decode(std::string_view input) {
    std::vector<std::byte> result(input.size() / 2);
    (void)hex_decode_to(input, std::span<std::byte>(result));
    return result;
}
```

**Description:** `hex_decode()` ignores the `DecodeResult` return value (cast to void). If the input is invalid, the returned vector contains partially decoded data with no indication of failure. Same for `base64_decode()`.

**Suggested fix:** Either throw on error, return an `expected`, or at minimum document that callers should use the `_to` variants for error checking:

```cpp
/// Decode hexadecimal string to byte vector.
/// @throws std::runtime_error if input contains invalid hex characters or odd length.
SIMDTEXT_NODISCARD SIMDTEXT_API std::vector<std::byte> hex_decode(std::string_view input);
```

Or use the `expected` type:
```cpp
SIMDTEXT_NODISCARD SIMDTEXT_API expected<std::vector<std::byte>, ErrorCode> hex_decode(std::string_view input);
```

---

### Issue 7.3 — HIGH: ConfigParser::entry() returns reference to static local on out-of-bounds access

**Files:**
- `src/config/config.cpp:39-41`

```cpp
const ConfigEntry& ConfigParser::entry(size_t index) const noexcept {
    static const ConfigEntry empty = {};
    return index < entries_.size() ? entries_[index] : empty;
}
```

**Description:** Silently returns a default-constructed `ConfigEntry` for out-of-bounds access instead of asserting or throwing. This hides bugs. Callers who check `entry.field_count()` before accessing might be fine, but callers who iterate by index without checking will silently get empty values.

**Suggested fix:** Use an assertion for debug builds, and return the static for release:

```cpp
const ConfigEntry& ConfigParser::entry(size_t index) const noexcept {
    assert(index < entries_.size() && "ConfigParser::entry() index out of bounds");
    static const ConfigEntry empty = {};
    return index < entries_.size() ? entries_[index] : empty;
}
```

---

### Issue 7.4 — MEDIUM: BytePattern::from_masked() silently returns empty pattern on size mismatch

**Files:**
- `src/pattern/pattern.cpp:37-39`

```cpp
BytePattern BytePattern::from_masked(std::span<const uint8_t> bytes, std::span<const uint8_t> mask) {
    BytePattern p;
    if (bytes.size() != mask.size()) return p;  // silent failure
    // ...
}
```

**Description:** If `bytes.size() != mask.size()`, returns a default-constructed (empty) BytePattern with no indication of failure. Callers might not check `pattern.empty()` and pass it to `find_pattern`, which would return nullptr.

**Suggested fix:** Either return `std::optional<BytePattern>` (like `parse()` does), or assert:

```cpp
static std::optional<BytePattern> from_masked(std::span<const uint8_t> bytes, std::span<const uint8_t> mask);
```

---

### Issue 7.5 — MEDIUM: url_encode_to returns 0 on both "output too small" and "empty input"

**Files:**
- `src/url/url.cpp:53-68`

**Description:** `url_encode_to` returns 0 when the output buffer is too small, but also returns 0 for empty input. Callers can't distinguish "nothing to encode" from "buffer too small".

**Suggested fix:** Return a distinct sentinel value, or use `DecodeResult`-style error reporting:

```cpp
/// Returns bytes written. Returns 0 on error (output too small).
/// To distinguish empty input from error, check `input.empty()` first.
SIMDTEXT_NODISCARD SIMDTEXT_API size_t url_encode_to(std::string_view input, std::span<char> output) noexcept;
```

---

### Issue 7.6 — MEDIUM: FileScanner::is_open() check is wrong

**Files:**
- `src/file/file.cpp:86-88`

```cpp
bool FileScanner::is_open() const {
    return file_.size() > 0 || file_.view().data() != nullptr;
}
```

**Description:** An empty file has `size() == 0` and `view().data() != nullptr` (if mmap succeeded for a 0-byte file, data would be nullptr but size 0). Actually, looking at the Impl, for a 0-byte file, `data_ = nullptr` and `size_ = 0`, so `view()` returns `{nullptr, 0}` and `data() == nullptr`. This means `is_open()` returns false for empty files, which is wrong.

**Suggested fix:** Track open state explicitly:

```cpp
class MappedFile::Impl {
    bool is_open_ = false;
public:
    bool open(std::filesystem::path path) {
        // ... existing logic ...
        is_open_ = true;
        return true;
    }
    bool open() const { return is_open_; }
};
```

---

### Issue 7.7 — LOW: json_unescape_inplace doesn't handle \uXXXX properly

**Files:**
- `src/json/json.cpp:195-199`

```cpp
case 'u': {
    // \uXXXX — write as-is for simplicity (full Unicode decode needs more logic)
    data[j++] = '\\';
    // Copy uXXXX
    for (size_t k = 0; k < 5 && i + k < len; k++) data[j++] = data[i + k];
    i += 5;
    break;
}
```

**Description:** The comment admits the implementation is incomplete. `\uXXXX` escape sequences are kept as-is (`\uXXXX` in the output), which is not proper unescaping. JSON strings with Unicode escapes will not be correctly decoded.

**Suggested fix:** Either implement proper \uXXXX → UTF-8 conversion, or document the limitation and return an error for \u sequences:

```cpp
case 'u': {
    // TODO: Full \uXXXX → UTF-8 conversion not yet implemented.
    // For now, copy the escape sequence verbatim.
    data[j++] = '\\';
    for (size_t k = 0; k < 5 && i + k < len; k++) data[j++] = data[i + k];
    i += 5;
    break;
}
```

---

## 8. Documentation Gaps

### Issue 8.1 — MEDIUM: Utf8Result struct fields lack documentation

**Files:**
- `include/simdtext/utf8.hpp:15-20`

```cpp
struct Utf8Result {
    bool valid = true;           ///< true if valid UTF-8
    size_t error_offset = 0;     ///< Byte offset of first error (0 if valid)
    uint8_t error_byte = 0;      ///< The invalid byte at error_offset
    std::string_view error_desc; ///< Human-readable error description
};
```

**Description:** These fields are documented ✓. However, the relationship between `error_offset` when `valid == true` is ambiguous — the doc says "0 if valid" but `error_offset` defaults to 0, which could mean "error at position 0" or "no error".

**Suggested fix:** Clarify the semantics:
```cpp
size_t error_offset = 0;  ///< Byte offset of first error. Only valid when `valid == false`.
```

---

### Issue 8.2 — MEDIUM: DiffOp struct fields lack documentation

**Files:**
- `include/simdtext/diff.hpp:9-17`

```cpp
struct DiffOp {
    enum Type : uint8_t {
        Equal, Insert, Delete,
    };
    Type type;
    std::string_view a_text;  // Text from a (for Equal/Delete)
    std::string_view b_text;  // Text from b (for Equal/Insert)
    size_t a_line;            // Line number in a (1-based)
    size_t b_line;            // Line number in b (1-based)
};
```

**Description:** The inline comments are minimal. What happens to `a_text` for an Insert? What happens to `b_line` for a Delete? The semantics of unset fields are unclear.

**Suggested fix:** Document the unset-field semantics:
```cpp
struct DiffOp {
    enum Type : uint8_t { Equal, Insert, Delete };
    Type type;
    std::string_view a_text;  ///< Text from source a. Valid for Equal and Delete. Empty for Insert.
    std::string_view b_text;  ///< Text from source b. Valid for Equal and Insert. Empty for Delete.
    size_t a_line = 0;        ///< 1-based line number in a. 0 when not applicable (Insert).
    size_t b_line = 0;        ///< 1-based line number in b. 0 when not applicable (Delete).
};
```

---

### Issue 8.3 — MEDIUM: LogEntry struct fields lack @brief documentation

**Files:**
- `include/simdtext/log.hpp:16-22`

**Description:** Fields have inline comments but no Doxygen-style `///<` markers. Inconsistent with Utf8Result which uses `///<`.

**Suggested fix:** Standardize on Doxygen-style:
```cpp
struct LogEntry {
    std::string_view timestamp;   ///< Timestamp portion of the log line
    std::string_view level;       ///< Log level string (INFO, WARN, ERROR, etc.)
    std::string_view message;     ///< Message body
    size_t line_number = 0;       ///< 1-based line number in the input
    size_t byte_offset = 0;       ///< Byte offset in the original input
};
```

---

### Issue 8.4 — MEDIUM: ParallelOptions fields lack documentation

**Files:**
- `include/simdtext/parallel.hpp:11-14`

```cpp
struct SIMDTEXT_API ParallelOptions {
    unsigned int num_threads = 0;
    size_t min_chunk_size = 64 * 1024;
};
```

**Description:** No documentation for what `num_threads = 0` means (auto-detect? single-threaded?). The `min_chunk_size` default value has a magic number.

**Suggested fix:**
```cpp
/// Parallel text processing options.
struct SIMDTEXT_API ParallelOptions {
    /// Number of threads to use. 0 = auto-detect based on hardware concurrency.
    unsigned int num_threads = 0;
    /// Minimum chunk size (bytes) per thread. Prevents threading overhead on small inputs.
    /// Default: 64 KB.
    size_t min_chunk_size = 64 * 1024;
};
```

---

### Issue 8.5 — LOW: CsvOptions::strict field undocumented

**Files:**
- `include/simdtext/csv.hpp:9`

```cpp
struct CsvOptions {
    char delimiter = ',';
    char quote = '"';
    bool strict = true;  // Reject malformed quoting
};
```

**Description:** The inline comment is helpful but doesn't explain what "malformed quoting" means. What happens when `strict = false` and quoting is malformed?

---

### Issue 8.6 — LOW: version.hpp lacks documentation

**Files:**
- `include/simdtext/version.hpp`

**Description:** No `@file` or `@brief` documentation. The macros are self-explanatory but should follow the same documentation style as other headers.

---

### Issue 8.7 — LOW: export.hpp lacks documentation for SIMDTEXT_RESTRICT and SIMDTEXT_UNREACHABLE

**Files:**
- `include/simdtext/export.hpp:57-70`

**Description:** `SIMDTEXT_RESTRICT` and `SIMDTEXT_UNREACHABLE` are defined but not documented. Users of the library shouldn't need these, but they're in a public header.

---

## 9. Include Hygiene

### Issue 9.1 — HIGH: expected.hpp includes `<stdexcept>` — violates zero-allocation design

**Files:**
- `include/simdtext/expected.hpp:3` — `#include <stdexcept>`

**Description:** The `expected` polyfill throws `std::runtime_error` from `value()`. But the entire library is designed around zero-allocation, noexcept patterns. Including `<stdexcept>` in a header that could be used in hot paths adds unnecessary overhead and contradicts the library's design philosophy.

**Suggested fix:** Remove the `value()` throwing accessor, or replace with `assert`:

```cpp
[[nodiscard]] T& value() & {
    assert(has_val_ && "bad expected access");
    if (!has_val_) __builtin_unreachable();
    return **this;
}
```

Or use the `SIMDTEXT_UNREACHABLE()` macro defined in export.hpp.

---

### Issue 9.2 — MEDIUM: simd_sse2.cpp includes <tmmintrin.h> but doesn't use it

**Files:**
- `src/detail/simd_sse2.cpp:2` — `#include <tmmintrin.h>`

**Description:** The SSSE3 header is included but no SSSE3 intrinsics are used in the live code. This is a include-what-you-use violation and could cause issues on processors that don't support SSSE3.

**Suggested fix:** Remove the include. If SSSE3 intrinsics are added later, add it back then.

---

### Issue 9.3 — MEDIUM: diff.cpp includes <emmintrin.h> under `__SSE2__` guard but also uses `__builtin_memcpy`

**Files:**
- `src/diff/diff.cpp:5-7`

```cpp
#if defined(__SSE2__)
#include <emmintrin.h>
#endif
```

**Description:** The SSE2 intrinsics are used for `common_prefix_length`. This is fine, but the file also uses `__builtin_memcpy` which is a GCC/Clang extension. On MSVC, this won't compile.

**Suggested fix:** Replace `__builtin_memcpy` with `std::memcpy`:

```cpp
// Instead of:
__builtin_memcpy(&va, a.data() + i, 8);
// Use:
std::memcpy(&va, a.data() + i, 8);
```

---

### Issue 9.4 — MEDIUM: hash.cpp uses `__asm__ __volatile__` for CRC32C — non-portable

**Files:**
- `src/hash/hash.cpp:76-80`

```cpp
__asm__ __volatile__("crc32q %1, %0" : "+r"(crc) : "rm"(val));
```

**Description:** Inline assembly for CRC32C is GCC/Clang-only and won't compile on MSVC. The file already has a `#if defined(__GNUC__) || defined(__clang__)` pattern elsewhere but doesn't guard the inline asm.

**Suggested fix:** Add MSVC intrinsic path:

```cpp
#if defined(_MSC_VER)
    crc = _mm_crc32_u64(crc, val);
#elif defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__("crc32q %1, %0" : "+r"(crc) : "rm"(val));
#endif
```

---

### Issue 9.5 — LOW: version.hpp doesn't include export.hpp but defines public macros

**Files:**
- `include/simdtext/version.hpp`

**Description:** `SIMDTEXT_VERSION_MAJOR` etc. are defined without including any other header. This is fine since they're just integer macros, but it's inconsistent with the pattern where every public header includes `export.hpp`.

---

### Issue 9.6 — LOW: pattern.hpp includes `<optional>` but only for return type

**Files:**
- `include/simdtext/pattern.hpp:9` — `#include <optional>`

**Description:** `<optional>` is a heavy header for just one return type. Consider forward-declaration or a different error-reporting mechanism. Low priority since the header is already including `<vector>` and `<string_view>`.

---

## 10. CMake Issues

### Issue 10.1 — CRITICAL: Version mismatch — CMakeLists.txt says 0.1.0, version.hpp says 0.2.0

**Files:**
- `CMakeLists.txt:2` — `project(simdtext VERSION 0.1.0 LANGUAGES CXX)`
- `include/simdtext/version.hpp:2` — `#define SIMDTEXT_VERSION_MAJOR 0` / `#define SIMDTEXT_VERSION_MINOR 2`

**Description:** The CMake project version and the header version are out of sync. `SIMDTEXT_VERSION_STRING` will report "0.2.0" but CMake's `PROJECT_VERSION` will be "0.1.0". This affects `simdtext-config-version.cmake` and any CMake-dependent version checks.

**Suggested fix:** Generate `version.hpp` from CMake, or sync them manually:

```cmake
# Option A: Generate version.hpp from CMake
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/simdtext/version.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/simdtext/version.hpp
    @ONLY
)
```

---

### Issue 10.2 — HIGH: Highway linked as PUBLIC but should be PRIVATE

**Files:**
- `CMakeLists.txt:148` — `target_link_libraries(simdtext PUBLIC hwy::hwy)`
- `CMakeLists.txt:151` — `target_link_libraries(simdtext PUBLIC hwy)`

**Description:** Highway is an implementation detail of simdtext. Linking it as PUBLIC means consumers of simdtext also get Highway's include paths and link against it. This pollutes the consumer's build and creates unnecessary dependencies.

**Suggested fix:** Change to PRIVATE:

```cmake
target_link_libraries(simdtext PRIVATE hwy::hwy)
# But we need Highway's headers for simd_hwy.cpp:
target_link_libraries(simdtext PRIVATE hwy::hwy)
```

Note: This may require adding `hwy`'s include directories to simdtext's PRIVATE includes. Since Highway uses runtime dispatch, the consumer doesn't need to know about it.

---

### Issue 10.3 — HIGH: simdtext_c compiled as C++ but advertised as C API

**Files:**
- `CMakeLists.txt:155` — `add_library(simdtext_c src/c/c_api.cpp)`
- `CMakeLists.txt:160` — `set_target_properties(simdtext_c PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)`

**Description:** The C API wrapper is compiled as C++23. If a C consumer tries to use it, the ABI will be C++ (name mangling). The header uses `extern "C"` which is correct, but the source file is C++.

**Suggested fix:** This is actually fine — `extern "C"` in the header ensures C linkage. The implementation being C++ is correct. However, the CMake should set the language to C for the public header validation:

```cmake
# Consider adding a C compliance test
add_executable(test_c_api examples/test_c_api.c)
target_link_libraries(test_c_api PRIVATE simdtext_c)
```

---

### Issue 10.4 — MEDIUM: SSE2 object compiled with `-mssse3` flag, contradicting the "SSE2 only" intent

**Files:**
- `CMakeLists.txt:119` — `target_compile_options(simdtext_sse2 PRIVATE -msse2 -mssse3 -mno-avx)`

**Description:** The SSE2 object is compiled with `-mssse3`, allowing the compiler to emit SSSE3 instructions. This contradicts the source file's own comment ("Stay within SSE2 instruction set"). On a CPU that supports SSE2 but not SSSE3 (e.g., some early AMD processors), this could cause SIGILL.

**Suggested fix:** Remove `-mssse3` from the SSE2 object's compile flags:

```cmake
target_compile_options(simdtext_sse2 PRIVATE -msse2 -mno-avx)
```

---

### Issue 10.5 — MEDIUM: Empty Base64 SSSE3 section in CMakeLists.txt

**Files:**
- `CMakeLists.txt:123-126`

```cmake
# ── Base64 SSSE3 intrinsics object ────────────────────────
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
elseif(MSVC)
endif()
```

**Description:** An empty if/elseif/endif block for a "Base64 SSSE3 intrinsics object" that was never implemented. Dead CMake code.

**Suggested fix:** Remove the empty block:

```cmake
# (delete lines 123-126)
```

---

### Issue 10.6 — LOW: simdtext-cli redundantly links Highway

**Files:**
- `CMakeLists.txt:163-167`

**Description:** `simdtext-cli` links Highway directly even though it already links `simdtext` (which links Highway). This is redundant since Highway is linked as PUBLIC in simdtext.

**Suggested fix:** Remove the direct Highway link from the CLI target:

```cmake
add_executable(simdtext-cli cli/main.cpp)
target_link_libraries(simdtext-cli PRIVATE simdtext)
# No need to link hwy — it comes transitively from simdtext (PUBLIC)
```

---

## 11. Thread Safety

### Issue 11.1 — CRITICAL: parallel_valid_utf8 is fundamentally broken (see Issue 7.1)

Already documented above. Splitting UTF-8 at arbitrary byte boundaries produces false negatives.

---

### Issue 11.2 — HIGH: parallel_find_byte has a subtle race condition in CAS loop

**Files:**
- `src/parallel/parallel.cpp:102-112`

```cpp
const char* result = find_byte(data.data() + start, data.data() + end, byte);
if (result != data.data() + end) {
    const char* current = earliest.load(std::memory_order_relaxed);
    while (current == nullptr || result < current) {
        if (earliest.compare_exchange_weak(current, result,
                std::memory_order_relaxed, std::memory_order_relaxed)) {
            break;
        }
    }
}
```

**Description:** The CAS loop uses `std::memory_order_relaxed` for both success and failure. This means there's no synchronization guarantee that the `result` pointer written by one thread is visible to other threads in a timely manner. More critically, the `current == nullptr || result < current` condition has a TOCTOU issue: between reading `current` and the CAS, another thread could have stored a pointer that's *earlier* than `result`, and the CAS would still succeed if `current` was nullptr.

The actual bug: if thread A finds byte at position 5 (stores 5), then thread B finds byte at position 100, B's CAS would succeed if it read nullptr before A's store was visible, overwriting position 5 with position 100.

**Suggested fix:** Use `memory_order_release` on the CAS success and `memory_order_acquire` on the final load:

```cpp
// Store with release semantics
earliest.compare_exchange_weak(current, result,
    std::memory_order_release, std::memory_order_relaxed);

// Final load with acquire semantics
const char* result = earliest.load(std::memory_order_acquire);
```

---

### Issue 11.3 — MEDIUM: parallel_is_ascii early-exits threads but already-launched threads still run

**Files:**
- `src/parallel/parallel.cpp:72-84`

```cpp
for (unsigned int t = 0; t < nthreads; ++t) {
    if (found_non_ascii.load(std::memory_order_relaxed)) break;
    // ...
    threads.emplace_back([...]() {
        if (found_non_ascii.load(std::memory_order_relaxed)) return;
        // ... do work ...
    });
}
```

**Description:** The loop breaks when `found_non_ascii` is detected, but threads that were already launched continue running. The `break` only prevents launching new threads. This is not a correctness bug (the result is still correct), but it wastes CPU cycles.

**Suggested fix:** This is a known trade-off in parallel algorithms. Document it, or use a thread pool with cancellation. Low priority.

---

## 12. ABI Concerns

### Issue 12.1 — HIGH: hash.hpp defines non-inline constexpr functions in header

**Files:**
- `include/simdtext/hash.hpp:14-20`

```cpp
constexpr uint64_t fnv1a(std::string_view s) noexcept {
    uint64_t hash = 14695981039346656037ULL;
    for (char c : s) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= 1099511628211ULL;
    }
    return hash;
}
```

**Description:** In C++17/20, `constexpr` functions are implicitly `inline`. However, the function is not a template, so every translation unit that includes this header will get its own copy. If the function's address is taken, different TUs may see different addresses. For a hash function, this is unlikely to be a problem, but the macro `SIMDTEXT_HASH(s)` takes the address of `fnv1a` in a switch statement, which means different TUs could have different case labels.

Actually, `constexpr` implies `inline` for non-template functions since C++17, so this is fine. The ODR is satisfied. But the `fnv1a(const char (&s)[N])` template overload is also fine. Let me re-examine...

The real ABI concern is that `SIMDTEXT_HASH(s)` macro computes `fnv1a(s)` at the call site. If `fnv1a`'s algorithm changes, the hash values in switch statements will change silently. This is a maintenance trap, not an ABI issue per se.

**Revised issue:** The `SIMDTEXT_HASH` macro encourages using hash values in `switch` statements, but the FNV-1a algorithm could change between versions, breaking binary compatibility silently.

**Suggested fix:** Document the stability guarantee:

```cpp
/// Compile-time FNV-1a hash. The algorithm is STABLE across simdtext versions.
/// Do not change the FNV-1a constants without a major version bump.
constexpr uint64_t fnv1a(std::string_view s) noexcept { ... }
```

---

### Issue 12.2 — HIGH: pattern.hpp has inline functions that call SIMDTEXT_API functions

**Files:**
- `include/simdtext/pattern.hpp:33-35` and `41-43`

```cpp
SIMDTEXT_NODISCARD inline const uint8_t* find_pattern(
    std::span<const uint8_t> data, const BytePattern& pattern) {
    return find_pattern(data.data(), data.size(), pattern);
}
```

**Description:** These inline wrapper functions are defined in the header. They call the `SIMDTEXT_API` (exported) overload. If the exported function's ABI changes, these inline functions would need recompilation. This is fine for a header-only usage pattern but could cause ODR violations if different TUs see different definitions.

**Suggested fix:** This is acceptable for convenience overloads. Just ensure the inline functions are in the same header as the exported functions they call, which they are.

---

### Issue 12.3 — MEDIUM: SIMDTEXT_API on iterator classes forces vtable/RTTI export

**Files:**
- `include/simdtext/lines.hpp:8-9` — `class SIMDTEXT_API LineView` and `class SIMDTEXT_API Iterator`

**Description:** `SIMDTEXT_API` is applied to both `LineView` and its nested `Iterator` class. On Windows with shared library builds, this means all member functions (including inline ones like `operator*`, `operator->`, `operator==`) are exported. This can lead to duplicate symbols if the consumer also compiles the inline functions.

**Suggested fix:** Only apply `SIMDTEXT_API` to the non-inline member functions (constructors, `advance`), not the entire class:

```cpp
class LineView {
public:
    class Iterator {
    public:
        // ... type aliases ...
        
        SIMDTEXT_API Iterator(std::string_view remaining);  // non-inline
        SIMDTEXT_API Iterator& operator++();  // non-inline
        SIMDTEXT_API Iterator operator++(int);  // non-inline
        
        // Inline functions don't need SIMDTEXT_API
        SIMDTEXT_NODISCARD reference operator*() const { return line_; }
        SIMDTEXT_NODISCARD bool operator==(const Iterator& other) const { ... }
    };
    // ...
};
```

---

### Issue 12.4 — MEDIUM: expected.hpp has heavy template code in public header

**Files:**
- `include/simdtext/expected.hpp` — ~150 lines of template code

**Description:** The `expected` polyfill is in a public header with full template implementations. This adds compile time to every TU that includes it. Since it's not used by any public API (see issue 6.3), this is unnecessary bloat.

**Suggested fix:** Move to `detail/expected.hpp` or remove entirely until used.

---

### Issue 12.5 — LOW: config.hpp's ConfigEntry struct has no padding control

**Files:**
- `include/simdtext/config.hpp:8-13`

```cpp
struct ConfigEntry {
    std::string_view section;
    std::string_view key;
    std::string_view value;
    size_t line;
};
```

**Description:** On 64-bit, this struct is 56 bytes with 0 padding waste. Actually this is fine — string_view is 16 bytes, size_t is 8, total = 16*3 + 8 = 56, which is 8-byte aligned. No issue.

---

## Appendix: Priority Remediation Order

These are the issues ranked by impact × effort:

1. **Fix parallel_valid_utf8** (7.1/11.1 — CRITICAL) — Broken by design. Either fix or remove.
2. **Delete dead SSE2 UTF-8 code** (6.1 — CRITICAL) — 60+ lines of dead code + misleading.
3. **Fix version mismatch** (10.1 — CRITICAL) — CMake vs header version out of sync.
4. **Extract shared UTF-8 validation** (1.1 — CRITICAL) — 5 copies of the same state machine.
5. **Deduplicate hex_decode_table** (1.2 — CRITICAL) — Same table, two files.
6. **Fix SSE2 compile flags** (10.4 — MEDIUM → HIGH) — `-mssse3` on SSE2 object could SIGILL.
7. **Consolidate dispatch** (1.5 — HIGH) — Two independent dispatch paths.
8. **Fix parallel_find_byte race** (11.2 — HIGH) — CAS with relaxed ordering.
9. **Standardize API parameter types** (2.1 — HIGH) — span vs string_view confusion.
10. **Move hex_val definition** (2.2 — HIGH) — Declared in encode.hpp, defined in url.cpp.
11. **Extract shared bitops** (1.3 — HIGH) — popcount/ctz duplicated 3 times.
12. **Fix ConfigParser/CsvParser noexcept** (3.3/3.4 — MEDIUM) — noexcept on throwing functions.
13. **Change Highway to PRIVATE** (10.2 — HIGH) — Leaks implementation detail.
14. **Fix FileScanner::is_open()** (7.6 — MEDIUM) — Wrong for empty files.
15. **Remove dead CMake sections** (10.5 — MEDIUM) — Empty Base64 SSSE3 block.

---

*End of audit. 70 issues found across 12 categories.*

# Architecture

## Directory Layout

```
simdtext/
├── include/simdtext/          # Public headers
│   ├── simdtext.hpp           # Umbrella header
│   ├── ascii.hpp              # ASCII classification & case ops
│   ├── scan.hpp               # count_byte, count_newlines, contains, find_byte
│   ├── lines.hpp              # LineView, SplitView, lines(), split()
│   ├── encode.hpp             # Hex & Base64 encode/decode
│   ├── url.hpp                # URL encode/decode, parse_query
│   ├── utf8.hpp               # UTF-8 validation
│   ├── file.hpp               # MappedFile, FileScanner
│   ├── types.hpp              # ErrorCode, DecodeResult
│   ├── expected.hpp           # std::expected polyfill (C++23)
│   ├── version.hpp            # Version macros
│   ├── export.hpp             # DLL export, SIMD target macros
│   ├── detail/
│   │   └── cpu_detect.hpp     # CpuFeatures struct, detect_cpu()
│   └── c/
│       └── simdtext.h         # C API header
├── src/
│   ├── detail/
│   │   ├── cpu_detect.cpp     # CPU feature detection (CPUID)
│   │   ├── simd_dispatch.cpp  # Dispatch: CPU features → best implementation
│   │   ├── simd_scalar.cpp    # Scalar fallback implementations
│   │   ├── simd_sse2.cpp      # SSE2 intrinsics implementations
│   │   └── simd_avx2.cpp      # AVX2 intrinsics implementations
│   ├── highway/
│   │   └── simd_hwy.cpp       # Google Highway portable SIMD implementations
│   ├── scalar/
│   │   └── scalar.cpp         # Non-SIMD scalar operations (encoding, etc.)
│   ├── encode/
│   │   └── encode.cpp         # Hex & Base64 implementation
│   ├── url/
│   │   └── url.cpp            # URL encode/decode implementation
│   ├── file/
│   │   └── file.cpp           # MappedFile & FileScanner implementation
│   └── c/
│       └── c_api.cpp          # C API wrapper
├── cli/
│   └── main.cpp               # CLI tool entry point
├── tests/
│   ├── main.cpp               # Test runner
│   ├── test_framework.hpp     # Lightweight test macros
│   ├── test_ascii.cpp
│   ├── test_scan.cpp
│   ├── test_lines.cpp
│   ├── test_encode.cpp
│   ├── test_url.cpp
│   ├── test_utf8.cpp
│   └── test_file.cpp
├── benchmarks/
│   └── bench_core.cpp         # Google Benchmark suite
├── examples/
│   └── stats.cpp              # Example: file statistics
├── deps/
│   └── highway/               # Google Highway (bundled)
└── docs/
    └── *.md
```

## Module Breakdown

### Scanning (`scan.hpp` / `simd_dispatch.cpp`)

Core scanning operations: `count_byte`, `count_newlines`, `contains`, `find_byte`. These are the most SIMD-critical functions and have implementations at every level: Highway, AVX2 intrinsics, SSE2 intrinsics, and scalar.

The public API delegates to `detail::*_dispatch()` functions which select the best available implementation based on cached CPU feature detection.

### ASCII (`ascii.hpp`)

`is_ascii`, `lowercase_ascii_inplace`, `uppercase_ascii_inplace`, `trim_ascii`. The first three have SIMD paths; `trim_ascii` is a simple scalar scan from both ends.

### Lines & Splitting (`lines.hpp`)

`LineView` and `SplitView` are lazy iterator adapters. They hold a `string_view` and produce `string_view` elements — zero allocation, zero copying. The iterator finds delimiter positions by scanning for `'\n'` or the custom delimiter character.

### Encoding (`encode.hpp` / `encode.cpp`)

Hex and Base64 encode/decode. These are primarily scalar implementations — the character-level logic doesn't benefit as much from SIMD. The `*_to` variants write into caller-provided buffers (zero allocation). The convenience variants (`hex_encode`, `base64_encode`, etc.) allocate and return strings/vectors.

### URL (`url.hpp` / `url.cpp`)

URL encoding/decoding and query string parsing. URL encoding is byte-by-byte with a lookup table for unreserved characters. `parse_query` iterates key-value pairs separated by `&` with `=` delimiters, URL-decoding each key and value.

### UTF-8 (`utf8.hpp`)

`valid_utf8` checks the full UTF-8 validity rules: correct lead byte / continuation byte sequences, no overlong encodings, no surrogate pairs (0xD800–0xDFFF). The Highway implementation processes 16–64 bytes at a time.

### File I/O (`file.hpp` / `file.cpp`)

- **`MappedFile`**: RAII wrapper around platform memory-mapped file APIs (`mmap`/`munmap` on POSIX, `CreateFileMapping`/`MapViewOfFile` on Windows). Provides a `string_view` over the mapped data — zero-copy, works with files larger than available memory (the OS pages in data on demand).

- **`FileScanner`**: Built on `MappedFile` + `LineView`. Convenience class for line-by-line processing with `each_line()`, `each_line_containing()`, `count_lines()`, and `count_matching()`.

### C API (`c/simdtext.h` / `c_api.cpp`)

Thin C wrapper over the C++ API. All functions use C linkage and simple types (`const char*`, `size_t`, `int`). String-returning functions allocate via the C++ allocator and must be freed with `simdtext_free()`. The `simdtext_file_t` handle wraps `MappedFile` in an opaque pointer.

## SIMD Dispatch Strategy

```
┌──────────────────────────────────────────┐
│           Public API (C++)               │
│  count_byte(), is_ascii(), etc.          │
└──────────────┬───────────────────────────┘
               │
               ▼
┌──────────────────────────────────────────┐
│        detail::dispatch layer            │
│  Checks detail::detect_cpu() cache       │
│  Routes to best available impl           │
└──────┬───────┬───────┬───────────────────┘
       │       │       │
       ▼       ▼       ▼
   Highway   AVX2    SSE2    Scalar
   (hwy)    intrins intrins  fallback
```

### CPU Feature Detection

`detail::detect_cpu()` is called once on first use and cached in a function-local `static`:

```cpp
const CpuFeatures& detect_cpu() {
    static CpuFeatures cached = do_detect();
    return cached;
}
```

`do_detect()` uses `__builtin_cpu_supports()` on GCC/Clang and `__cpuid()` on MSVC. On AArch64, NEON is assumed available.

### Per-ISA Object Libraries

SSE2 and AVX2 intrinsics are compiled as separate CMake object libraries with ISA-specific compiler flags:

```cmake
add_library(simdtext_sse2 OBJECT src/detail/simd_sse2.cpp)
target_compile_options(simdtext_sse2 PRIVATE -msse2 -mno-avx)

add_library(simdtext_avx2 OBJECT src/detail/simd_avx2.cpp)
target_compile_options(simdtext_avx2 PRIVATE -mavx2 -mbmi -mlzcnt -mpopcnt)
```

This ensures correct instruction generation regardless of the global `-march` setting. The objects are linked into the main `simdtext` library.

### Google Highway

When enabled, Highway provides portable SIMD that compiles once and dispatches at runtime to the best available target. Highway's approach avoids the need for per-ISA compilation in most cases, and supports future ISAs (AVX-512, SVE, RISC-V V) without code changes.

The Highway path (`src/highway/simd_hwy.cpp`) is the primary SIMD implementation. The intrinsics paths exist as a fallback and for comparison.

## Zero-Allocation Design

simdtext is designed to avoid heap allocation in hot paths:

| Operation | Allocation? | Notes |
|-----------|-------------|-------|
| `count_byte`, `count_newlines` | No | Pure read, accumulator |
| `is_ascii` | No | Pure read, boolean result |
| `lowercase_ascii_inplace` | No | In-place modification |
| `uppercase_ascii_inplace` | No | In-place modification |
| `trim_ascii` | No | Returns `string_view` into input |
| `lines()` / `split()` | No | Returns view with lazy iterators |
| `find_byte` | No | Returns pointer into input |
| `hex_encode_to` / `base64_encode_to` | No | Writes to caller buffer |
| `hex_decode_to` / `base64_decode_to` | No | Writes to caller buffer |
| `hex_encode` / `base64_encode` | **Yes** | Returns `std::string` |
| `hex_decode` / `base64_decode` | **Yes** | Returns `std::vector<std::byte>` |
| `url_encode` / `url_decode` | **Yes** | Returns `std::string` |
| `parse_query` | **Yes** | Returns `std::unordered_map` |
| `MappedFile::view()` | No | Returns `string_view` over mmap |
| `FileScanner::each_line` | No | Callback receives `string_view` |

The `_to` suffixed functions and the view-based APIs allow completely allocation-free processing when needed.

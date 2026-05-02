# simdtext

[![CI](https://github.com/tonytran-ai/simdtext/actions/workflows/ci.yml/badge.svg)](https://github.com/tonytran-ai/simdtext/actions/workflows/ci.yml)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://isocpp.org/std/the-standard)
[![MIT License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![SIMD](https://img.shields.io/badge/SIMD-SSE2%20%7C%20AVX2%20%7C%20NEON-orange.svg)](#simd-acceleration)

High-performance C++ text utilities for large buffers. SIMD-accelerated, zero-allocation, production-ready.

## Table of Contents

- [Why simdtext?](#why-simdtext)
- [Comparison](#comparison)
- [Features](#features)
- [Quick Start](#quick-start)
- [API Summary](#api-summary)
- [Integration](#integration)
- [CLI Reference](#cli-reference)
- [Benchmarks](#benchmarks)
- [SIMD Acceleration](#simd-acceleration)
- [Building](#building)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [Requirements](#requirements)
- [License](#license)

## Why simdtext?

You're processing logs, network data, config files, or game assets — millions of lines, gigabytes of text. You need it fast, and you need it correct. simdtext gives you both.

**vs. rolling your own loops:**
- 10–50× faster on buffer operations via SIMD
- Correct UTF-8 validation, Base64 padding, URL encoding edge cases
- Zero-allocation views mean no GC pressure

**vs. [simdutf](https://github.com/simdutf/simdutf):**
- simdutf focuses on Unicode conversion (UTF-8 ↔ UTF-16/UTF-32)
- simdtext focuses on text *operations*: scanning, splitting, encoding, URL processing
- Use both together — simdutf for conversions, simdtext for everything else

**vs. [ada](https://github.com/ada-url/ada):**
- ada is a full WHATWG URL parser (scheme, host, port, path)
- simdtext provides lightweight URL encoding/decoding and query parsing
- Use ada for URL parsing, simdtext for URL encoding/decoding at scale

**vs. [fast_float](https://github.com/fastfloat/fast_float):**
- fast_float is SIMD-accelerated number parsing
- simdtext doesn't do number parsing — different domain
- Same design philosophy: zero-allocation, SIMD-first, header-friendly

## Comparison

| Feature | simdtext | [simdutf](https://github.com/simdutf/simdutf) | [ada](https://github.com/ada-url/ada) | [fast_float](https://github.com/fastfloat/fast_float) |
|---|---|---|---|---|
| **Primary focus** | Text operations | Unicode conversion | URL parsing | Number parsing |
| Byte scanning | ✅ SIMD | — | — | — |
| Line/split iteration | ✅ Zero-alloc | — | — | — |
| ASCII case ops | ✅ SIMD | — | — | — |
| Hex encode/decode | ✅ | — | — | — |
| Base64 encode/decode | ✅ | — | — | — |
| URL encode/decode | ✅ | — | ✅ (full WHATWG) | — |
| Query string parsing | ✅ | — | ✅ | — |
| UTF-8 validation | ✅ SIMD | ✅ SIMD | — | — |
| UTF-8↔UTF-16/32 | — | ✅ SIMD | — | — |
| Number parsing | — | — | — | ✅ SIMD |
| Memory-mapped files | ✅ | — | — | — |
| C API | ✅ | ✅ | — | ✅ |
| Zero allocation (hot paths) | ✅ | ✅ | ✅ | ✅ |
| Header-only mode | No | Optional | No | Yes |
| SIMD backends | SSSE3, AVX2, AVX-512, NEON, Highway | Custom | — | Custom |
| C++ standard | C++23 | C++11 | C++17 | C++11 |

## Features

- 🚀 **SIMD-accelerated** — `count_byte`, `is_ascii`, `lowercase/uppercase`, `find_byte`, `valid_utf8` use Google Highway (SSE2 → AVX2 → AVX-512, NEON)
- 💾 **Zero allocation** — `LineView`, `SplitView`, `trim_ascii` return `string_view`; `_to` variants write to caller buffers
- 🧵 **Multi-threaded** — `parallel_count_byte`, `parallel_is_ascii`, `parallel_valid_utf8` and more via `parallel.hpp`
- 🔍 **Pattern scanning** — Find byte patterns with wildcard support via `pattern.hpp`
- #️⃣ **Fast hashing** — FNV-1a (constexpr), CRC32/CRC32C (hardware), xxHash64, Wyhash
- ✂️ **String utilities** — `trim`, `replace_all`, `fields`, `split_vec`, `starts_with`, `ends_with`
- 📄 **Memory-mapped files** — process files larger than RAM via `MappedFile` and `FileScanner`
- 🔧 **CLI tool** — `simdtext stats`, `simdtext grep`, `simdtext validate-utf8`, and more
- 🔗 **C API** — FFI-friendly for Rust, Python, Ruby, etc.
- 🌍 **Cross-platform** — Linux, macOS, Windows (x86 + ARM)
- 📦 **Easy integration** — CMake `add_subdirectory`, `find_package`, or vcpkg

## Quick Start

```cpp
#include <simdtext/simdtext.hpp>

int main() {
    // Scan a file
    simdtext::FileScanner scanner("server.log");
    size_t errors = scanner.count_matching("ERROR");

    // Zero-copy line iteration
    std::string data = /* load file */;
    for (std::string_view line : simdtext::lines(data)) {
        if (simdtext::contains(line, "ERROR")) { /* ... */ }
    }

    // ASCII operations (SIMD-accelerated)
    simdtext::lowercase_ascii_inplace(buffer);
    auto trimmed = simdtext::trim_ascii("  hello  ");

    // Encoding
    auto hex = simdtext::hex_encode(bytes);
    auto b64 = simdtext::base64_encode(bytes);

    // URL helpers
    auto decoded = simdtext::url_decode("hello%20world");
    auto params = simdtext::parse_query("name=tony&lang=c%2B%2B");

    // UTF-8 validation
    bool ok = simdtext::valid_utf8(buffer);
}
```

## API Summary

### Scanning (`<simdtext/scan.hpp>`)

| Function | Signature | Description |
|----------|-----------|-------------|
| `count_byte` | `size_t(std::span<const char>, char)` | Count byte occurrences |
| `count_newlines` | `size_t(std::span<const char>)` | Count newline characters |
| `contains` | `bool(std::string_view, std::string_view)` | Check if needle exists in haystack |
| `find_byte` | `const char*(const char*, const char*, char)` | Find first byte in range |

### ASCII (`<simdtext/ascii.hpp>`)

| Function | Signature | Description |
|----------|-----------|-------------|
| `is_ascii` | `bool(std::span<const char>)` | Check if all bytes are ASCII |
| `lowercase_ascii_inplace` | `void(std::span<char>)` | Lowercase ASCII in-place |
| `uppercase_ascii_inplace` | `void(std::span<char>)` | Uppercase ASCII in-place |
| `trim_ascii` | `string_view(string_view)` | Trim ASCII whitespace |

### Lines & Splitting (`<simdtext/lines.hpp>`)

| Function/Class | Signature | Description |
|----------------|-----------|-------------|
| `LineView` | Class | Iterable line view (split by `'\n'`) |
| `lines` | `LineView(string_view)` | Create a LineView |
| `SplitView` | Class | Iterable split view by delimiter |
| `split` | `SplitView(string_view, char)` | Create a SplitView |

### Encoding (`<simdtext/encode.hpp>`)

| Function | Signature | Description |
|----------|-----------|-------------|
| `hex_encode` | `string(span<const byte>)` | Encode to hex string |
| `hex_encode_to` | `size_t(span<const byte>, span<char>)` | Encode to hex buffer |
| `hex_decode` | `vector<byte>(string_view)` | Decode from hex |
| `hex_decode_to` | `DecodeResult(string_view, span<byte>)` | Decode hex to buffer |
| `base64_encode` | `string(span<const byte>)` | Encode to Base64 string |
| `base64_encode_to` | `size_t(span<const byte>, span<char>)` | Encode to Base64 buffer |
| `base64_decode` | `vector<byte>(string_view)` | Decode from Base64 |
| `base64_decode_to` | `DecodeResult(string_view, span<byte>)` | Decode Base64 to buffer |

### URL (`<simdtext/url.hpp>`)

| Function | Signature | Description |
|----------|-----------|-------------|
| `url_encode` | `string(string_view)` | URL-encode a string |
| `url_encode_to` | `size_t(string_view, span<char>)` | URL-encode to buffer |
| `url_decode` | `string(string_view)` | URL-decode a string |
| `url_decode_to` | `size_t(string_view, span<char>)` | URL-decode to buffer |
| `parse_query` | `unordered_map<string,string>(string_view)` | Parse query string |

### UTF-8 (`<simdtext/utf8.hpp>`)

| Function | Signature | Description |
|----------|-----------|-------------|
| `valid_utf8` | `bool(span<const char>)` | Validate UTF-8 encoding |

### Pattern Scanning (`<simdtext/pattern.hpp>`)

| Function | Signature | Description |
|----------|-----------|-------------|
| `find_pattern` | `const char*(string_view, string_view hex)` | Find byte pattern with wildcards (`??` = any) |
| `byte_pattern_parse` | `BytePattern(string_view hex)` | Parse hex pattern into bytes/masks |

### Parallel Processing (`<simdtext/parallel.hpp>`)

| Function | Signature | Description |
|----------|-----------|-------------|
| `parallel_count_byte` | `size_t(string_view, char, ParallelOptions)` | Multi-threaded byte counting |
| `parallel_is_ascii` | `bool(string_view, ParallelOptions)` | Multi-threaded ASCII check |
| `parallel_count_newlines` | `size_t(string_view, ParallelOptions)` | Multi-threaded newline counting |
| `parallel_find_byte` | `const char*(string_view, char, ParallelOptions)` | Multi-threaded byte search |
| `parallel_valid_utf8` | `bool(string_view, ParallelOptions)` | Multi-threaded UTF-8 validation |
| `parallel_for_each_chunk` | `void(string_view, callback, ParallelOptions)` | Parallel chunk processing |

### Hashing (`<simdtext/hash.hpp>`)

| Function | Signature | Description |
|----------|-----------|-------------|
| `fnv1a` | `uint64_t(string_view) constexpr` | Compile-time FNV-1a hash |
| `SIMDTEXT_HASH` | Macro | Switch-case string matching |
| `crc32` | `uint32_t(string_view) noexcept` | CRC32 (hardware-accelerated) |
| `crc32c` | `uint32_t(string_view) noexcept` | CRC32C Castagnoli (hardware-accelerated) |
| `xxhash64` | `uint64_t(string_view) noexcept` | xxHash-64 fast non-crypto hash |
| `wyhash` | `uint64_t(string_view) noexcept` | Wyhash fast non-crypto hash |

### String Utilities (`<simdtext/str.hpp>`)

| Function | Signature | Description |
|----------|-----------|-------------|
| `trim_left` | `string_view(string_view)` | Trim leading whitespace |
| `trim_right` | `string_view(string_view)` | Trim trailing whitespace |
| `trim` | `string_view(string_view)` | Trim both ends |
| `replace_all` | `string(string_view, char, char)` | Replace all occurrences of a char |
| `replace_all` | `string(string_view, string_view, string_view)` | Replace all occurrences of a substring |
| `fields` | `vector<string_view>(string_view)` | Split by whitespace, skip empty |
| `split_vec` | `vector<string_view>(string_view, char)` | Split by delimiter |
| `split_into` | `size_t(string_view, char, string_view*, size_t)` | Split into pre-allocated buffer |
| `starts_with` | `bool(string_view, string_view)` | Check prefix |
| `ends_with` | `bool(string_view, string_view)` | Check suffix |
| `contains_char` | `bool(string_view, char)` | Check if char exists (SIMD) |

### File I/O (`<simdtext/file.hpp>`)

| Class | Key Methods | Description |
|-------|-------------|-------------|
| `MappedFile` | `open()`, `view()`, `size()` | Memory-mapped file (zero-copy) |
| `FileScanner` | `each_line()`, `count_lines()`, `count_matching()`, `each_line_containing()` | Line-by-line scanner |

### Types (`<simdtext/types.hpp>`)

| Type | Description |
|------|-------------|
| `ErrorCode` | Enum: `Ok`, `InvalidChar`, `InvalidLength`, `OutputTooSmall` |
| `DecodeResult` | Struct: `bytes_written`, `error_offset`, `error`, `ok()` |

> See [docs/API.md](docs/API.md) for complete documentation with examples.

## Integration

### CMake — `add_subdirectory`

```cmake
# Clone into your project
add_subdirectory(simdtext)
target_link_libraries(my_app PRIVATE simdtext)
```

### CMake — `find_package`

```cmake
# After installing simdtext
find_package(simdtext CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE simdtext::simdtext)
```

### CMake — FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    simdtext
    GIT_REPOSITORY https://github.com/tonytran-ai/simdtext.git
    GIT_TAG        v0.2.0
)
FetchContent_MakeAvailable(simdtext)
target_link_libraries(my_app PRIVATE simdtext)
```

### CPM

```cmake
CPMAddPackage(
    NAME simdtext
    GITHUB_REPOSITORY tonytran-ai/simdtext
    GIT_TAG v0.2.0
)
target_link_libraries(my_app PRIVATE simdtext)
```

### vcpkg

```bash
vcpkg install simdtext
```

```cmake
find_package(simdtext CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE simdtext::simdtext)
```

## CLI Reference

```bash
simdtext stats <file>              # File statistics with scan speed
simdtext grep <file> <pattern>     # Find lines containing pattern
simdtext count <file> <byte>       # Count occurrences of a byte
simdtext lower <file>              # Lowercase file to stdout
simdtext upper <file>              # Uppercase file to stdout
simdtext hex-encode <file>         # Hex-encode file to stdout
simdtext base64-encode <file>      # Base64-encode file to stdout
simdtext url-decode <string>       # URL-decode a string
simdtext url-encode <string>       # URL-encode a string
simdtext validate-utf8 <file>      # Validate UTF-8 encoding
```

### Examples

```bash
$ simdtext stats server.log
File: server.log
Size: 2.3 GB
Lines: 18,432,109
ASCII: yes
UTF-8 valid: yes
Scan speed: 12.4 GB/s

$ simdtext grep server.log ERROR
2024-01-15 03:21:44 ERROR connection timeout
2024-01-15 03:22:01 ERROR disk full

$ simdtext url-decode "hello%20world%21"
hello world!
```

## Benchmarks

Benchmarks are built with Google Benchmark and measure throughput of core operations.

### Running Benchmarks

```bash
cmake -B build -DSIMDTEXT_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bench_core
```

### Example Results

> Results vary by CPU. Run on your own hardware for accurate numbers.

| Operation | Throughput | Notes |
|-----------|-----------|-------|
| `valid_utf8` | 104.1 GB/s | AVX2 via Highway |
| `is_ascii` | 103.6 GB/s | AVX2 via Highway |
| `lowercase`/`uppercase` | ~28 GB/s | AVX2, fixed cache thrashing |
| `hex_encode` | 15–17 GB/s | SSSE3/AVX2 SIMD |
| `hex_decode` | 13–14 GB/s | SSSE3/AVX2 SIMD |
| `base64_encode` | 10+ GB/s | AVX2 SIMD |
| `count_byte` | 12.3 GB/s | AVX2, 1GB buffer |
| `lines` | ~10 GB/s | SIMD-accelerated |
| `base64_decode` | 3.61 GB/s | AVX2 SIMD |
| `url_decode` | 1.2 GB/s | Highway SIMD |
| `url_encode` | 1.1 GB/s | Highway SIMD |

> See [docs/BENCHMARKS.md](docs/BENCHMARKS.md) for detailed methodology and instructions on adding new benchmarks.

## SIMD Acceleration

simdtext uses a multi-layer SIMD strategy for maximum performance across all x86 and ARM platforms:

```
Google Highway (portable, runtime dispatch)
    ├── AVX-512BW  (64 bytes/cycle)
    ├── AVX2       (32 bytes/cycle)
    ├── SSE2       (16 bytes/cycle)
    └── NEON       (16 bytes/cycle, ARM)
```

Plus SSSE3 intrinsics for hex encode/decode, AVX2 intrinsics for hex and base64 encode/decode, compiled as separate object files with per-ISA flags for guaranteed correct codegen. CPU features are detected once at startup and cached.

If Google Highway is unavailable, operations fall back to the hand-written intrinsics or pure scalar C++.

## v0.2.0 — SIMD Optimization Wave

A major round of SIMD optimizations across nearly every operation:

| Operation | Before | After | Speedup |
|-----------|--------|-------|---------|
| `hex_encode` | 1.16 GB/s | 15–17 GB/s | **14×** |
| `hex_decode` | 2.24 GB/s | 13–14 GB/s | **6×** |
| `base64_encode` | 1.56 GB/s | 10+ GB/s | **7×** |
| `base64_decode` | 2.41 GB/s | 3.61 GB/s | **50%** |
| `url_encode` | 257 MB/s | 1.1 GB/s | **4.2×** |
| `url_decode` | 859 MB/s | 1.2 GB/s | **41%** |
| `lowercase`/`uppercase` | 16.2 GB/s | ~28 GB/s | **1.7×** |
| `lines`/`split` | 5.1 GB/s | ~10 GB/s | **2×** |

**Highlights:**
- **SSSE3/AVX2 SIMD hex encode/decode** — lookup-table-free pshufb-based implementation
- **AVX2 SIMD base64 encode/decode** — full SIMD path with proper padding
- **Highway SIMD url encode/decode** — portable across x86 and ARM
- **Fixed lowercase/uppercase cache thrashing** — eliminated false dependency that limited throughput
- **SIMD-accelerated lines/split** — newline scanning now uses SIMD byte counting
- **NEON optimizations for ARM** — count_code_points and validate_utf8 NEON implementations
- **Zero compiler warnings** across all build configurations

## Building

```bash
# Build with SIMD (default)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Build without SIMD
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSIMDTEXT_USE_HIGHWAY=OFF
cmake --build build

# Run tests
./build/test_simdtext

# Run benchmarks
cmake -B build -DSIMDTEXT_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bench_core
```

See [BUILDING.md](docs/BUILDING.md) for detailed instructions including cross-compilation, vcpkg, and CMake presets.

## Documentation

| Document | Description |
|----------|-------------|
| [API Reference](docs/API.md) | Full API for every function with signatures and examples |
| [Building](docs/BUILDING.md) | Build instructions, CMake options, cross-compilation |
| [Architecture](docs/ARCHITECTURE.md) | Directory structure, SIMD dispatch, zero-allocation design |
| [Benchmarks](docs/BENCHMARKS.md) | How to run and add benchmarks |
| [Contributing](CONTRIBUTING.md) | Code style, commit format, PR process |
| [Changelog](CHANGELOG.md) | Release history |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines on building, code style, PR process, and adding new modules.

## Requirements

- C++23 compiler (GCC 12+, Clang 16+, MSVC 19.34+)
- CMake 3.16+
- Google Highway (bundled in `deps/`)

## License

MIT

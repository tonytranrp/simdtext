# simdtext

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://isocpp.org/std/the-standard)
[![MIT License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![SIMD](https://img.shields.io/badge/SIMD-SSE2%20%7C%20AVX2%20%7C%20NEON-orange.svg)](#simd-acceleration)

High-performance C++ text utilities for large buffers. SIMD-accelerated, zero-allocation, production-ready.

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

## Features

- 🚀 **SIMD-accelerated** — `count_byte`, `is_ascii`, `lowercase/uppercase`, `find_byte`, `valid_utf8` use Google Highway (SSE2 → AVX2 → AVX-512, NEON)
- 💾 **Zero allocation** — `LineView`, `SplitView`, `trim_ascii` return `string_view`; `_to` variants write to caller buffers
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

## API Summary

### Scanning (SIMD-accelerated)

| Function | Description |
|----------|-------------|
| `count_byte(input, byte)` | Count byte occurrences |
| `count_newlines(input)` | Count newline characters |
| `contains(input, needle)` | Check if needle exists |
| `find_byte(begin, end, byte)` | Find first byte occurrence |

### ASCII (SIMD-accelerated)

| Function | Description |
|----------|-------------|
| `is_ascii(input)` | Check if all bytes are ASCII |
| `lowercase_ascii_inplace(input)` | Lowercase ASCII in-place |
| `uppercase_ascii_inplace(input)` | Uppercase ASCII in-place |
| `trim_ascii(input)` | Trim ASCII whitespace |

### Lines & Splitting (zero-allocation)

| Function | Description |
|----------|-------------|
| `lines(input)` | Iterable line view (split by `'\n'`) |
| `split(input, delim)` | Iterable split view |

### Encoding

| Function | Description |
|----------|-------------|
| `hex_encode(bytes)` / `hex_encode_to(bytes, out)` | Encode to hex |
| `hex_decode(str)` / `hex_decode_to(str, out)` | Decode from hex |
| `base64_encode(bytes)` / `base64_encode_to(bytes, out)` | Encode to Base64 |
| `base64_decode(str)` / `base64_decode_to(str, out)` | Decode from Base64 |

### URL

| Function | Description |
|----------|-------------|
| `url_encode(input)` / `url_encode_to(input, out)` | URL-encode |
| `url_decode(input)` / `url_decode_to(input, out)` | URL-decode |
| `parse_query(query)` | Parse query string to map |

### File I/O

| Class | Description |
|-------|-------------|
| `MappedFile` | Memory-mapped file (zero-copy read) |
| `FileScanner` | Line-by-line file scanner |

### UTF-8

| Function | Description |
|----------|-------------|
| `valid_utf8(input)` | Validate UTF-8 encoding |

## SIMD Acceleration

simdtext uses a multi-layer SIMD strategy for maximum performance across all x86 and ARM platforms:

```
Google Highway (portable, runtime dispatch)
    ├── AVX-512BW  (64 bytes/cycle)
    ├── AVX2       (32 bytes/cycle)
    ├── SSE2       (16 bytes/cycle)
    └── NEON       (16 bytes/cycle, ARM)
```

Plus hand-written SSE2/AVX2 intrinsics compiled as separate object files with per-ISA flags for guaranteed correct codegen. CPU features are detected once at startup and cached.

If Google Highway is unavailable, operations fall back to the hand-written intrinsics or pure scalar C++.

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
| [Contributing](docs/CONTRIBUTING.md) | Code style, commit format, PR process |

## Design Principles

1. **Zero allocation in hot paths** — views, spans, in-place operations
2. **SIMD-first** — Google Highway for portable, runtime-dispatched SIMD
3. **Correctness** — UTF-8 validation checks all edge cases; encode/decode handles padding and errors
4. **API surface** — every allocating function has a `_to` variant that writes to a caller buffer
5. **C API** — FFI-friendly for bindings to any language
6. **No exceptions in hot paths** — decode errors use `DecodeResult`, not exceptions

## Requirements

- C++23 compiler (GCC 12+, Clang 16+, MSVC 19.34+)
- CMake 3.16+
- Google Highway (bundled in `deps/`)

## License

MIT

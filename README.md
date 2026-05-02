# simdtext

High-performance C++ text utilities for large buffers.

simdtext provides fast SIMD-accelerated, zero-allocation operations for scanning, splitting, trimming, encoding, decoding, and validating text. Designed for logs, network protocols, data files, config parsers, game engines, and backend services.

## Features

- **SIMD-accelerated** — count_byte, is_ascii, lowercase/uppercase, find_byte use Google Highway
- **Zero allocation** — line/split views return `string_view`, not `string`
- **Memory-mapped files** — process huge files without loading into memory
- **CLI tool** — `simdtext stats`, `simdtext grep`, and more
- **C++20** — modern standard with `std::span`
- **Cross-platform** — Linux, macOS, Windows

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
}
```

## CLI

```bash
# File statistics with scan speed
simdtext stats server.log

# Grep lines containing pattern
simdtext grep server.log ERROR

# Count occurrences of a byte
simdtext count file.txt $'\n'

# Lowercase/uppercase file contents
simdtext lower file.txt
simdtext upper file.txt

# Encoding
simdtext hex-encode file.bin
simdtext base64-encode file.bin

# URL helpers
simdtext url-decode "hello%20world"
simdtext url-encode "hello world"

# UTF-8 validation
simdtext validate-utf8 file.txt
```

## API

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

### Lines & Splitting

| Function | Description |
|----------|-------------|
| `lines(input)` | Iterable line view (zero-copy) |
| `split(input, delim)` | Iterable split view (zero-copy) |

### Encoding

| Function | Description |
|----------|-------------|
| `hex_encode(bytes)` | Encode to hex string |
| `hex_decode(string)` | Decode hex to bytes |
| `base64_encode(bytes)` | Encode to Base64 string |
| `base64_decode(string)` | Decode Base64 to bytes |

### URL

| Function | Description |
|----------|-------------|
| `url_encode(input)` | URL-encode a string |
| `url_decode(input)` | URL-decode a string |
| `parse_query(query)` | Parse query string to map |

### File I/O

| Function | Description |
|----------|-------------|
| `MappedFile(path)` | Memory-mapped file (zero-copy) |
| `FileScanner(path)` | Line-by-line file scanner |
| `FileScanner::each_line(cb)` | Iterate lines |
| `FileScanner::each_line_containing(needle, cb)` | Iterate matching lines |
| `FileScanner::count_lines()` | Count lines |
| `FileScanner::count_matching(needle)` | Count matching lines |

### UTF-8

| Function | Description |
|----------|-------------|
| `valid_utf8(input)` | Validate UTF-8 encoding |

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

# Run CLI
./build/simdtext stats server.log
```

## Benchmarks

```bash
cmake -B build -DSIMDTEXT_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bench_core
```

## Design Principles

- **Zero allocation** — line/split views return `string_view`, not `string`
- **In-place operations** — lowercase/uppercase modify the buffer directly
- **SIMD-accelerated** — Google Highway for portable SIMD (SSE2→AVX2→AVX-512, NEON)
- **Memory-mapped files** — process files larger than RAM
- **Scalar fallback** — works without Highway on any platform

## License

MIT

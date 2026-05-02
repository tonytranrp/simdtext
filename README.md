# simdtext

High-performance C++ text utilities for large buffers.

simdtext provides fast zero-allocation operations for scanning, splitting, trimming, encoding, decoding, and validating text. Designed for logs, network protocols, data files, config parsers, game engines, and backend services.

## Quick Start

```cpp
#include <simdtext/simdtext.hpp>

int main() {
    // Scan a file
    simdtext::FileScanner scanner("server.log");

    size_t errors = scanner.count_matching("ERROR");

    scanner.each_line_containing("ERROR", [](std::string_view line) {
        std::cout << line << "\n";
    });

    // String operations
    std::string header = "CONTENT-TYPE: APPLICATION/JSON";
    simdtext::lowercase_ascii_inplace(header);
    // → "content-type: application/json"

    // Zero-copy line iteration
    std::string data = /* load file */;
    for (std::string_view line : simdtext::lines(data)) {
        if (simdtext::contains(line, "ERROR")) {
            // handle error
        }
    }

    // Encoding
    auto hex = simdtext::hex_encode(bytes);
    auto b64 = simdtext::base64_encode(bytes);

    // URL helpers
    auto decoded = simdtext::url_decode("hello%20world");
    auto params = simdtext::parse_query("name=tony&lang=c%2B%2B");
    // params["name"] == "tony", params["lang"] == "c++"
}
```

## API

### Scanning

| Function | Description |
|----------|-------------|
| `count_byte(input, byte)` | Count byte occurrences |
| `count_newlines(input)` | Count newline characters |
| `contains(input, needle)` | Check if needle exists |
| `find_byte(begin, end, byte)` | Find first byte occurrence |

### ASCII

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
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run tests
./build/test_simdtext

# Run example
./build/example_stats server.log
```

## Design Principles

- **Zero allocation** — line/split views return `string_view`, not `string`
- **In-place operations** — lowercase/uppercase modify the buffer directly
- **Memory-mapped files** — process huge files without loading into memory
- **C++20** — modern standard with `std::span`
- **SIMD-ready** — scalar implementations now, SIMD paths coming via Google Highway

## Status

v0.1 — Scalar implementations with full API. SIMD acceleration coming soon.

## License

MIT

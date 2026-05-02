# API Reference

Complete reference for simdtext v0.1.0. All symbols live in the `simdtext` namespace.
Include the umbrella header for everything, or individual headers for finer control.

```cpp
#include <simdtext/simdtext.hpp>          // everything
#include <simdtext/scan.hpp>              // only scanning functions
```

---

## Scanning — `<simdtext/scan.hpp>`

SIMD-accelerated scanning operations for large buffers.

### `count_byte`

```cpp
[[nodiscard]] size_t count_byte(std::span<const char> input, char byte);
```

Counts occurrences of `byte` in `input`.

| Parameter | Description |
|-----------|-------------|
| `input`   | Buffer to scan |
| `byte`    | Byte value to count |

**Returns:** Number of occurrences.

```cpp
std::string data = "hello\nworld\nfoo\n";
size_t n = simdtext::count_byte(data, '\n');  // n == 3
```

**Performance:** Processes 16–64 bytes per iteration depending on SIMD level (SSE2/AVX2/Highway). Scalar fallback for tail bytes.

---

### `count_newlines`

```cpp
[[nodiscard]] size_t count_newlines(std::span<const char> input);
```

Counts newline characters (`'\n'`) in `input`. Equivalent to `count_byte(input, '\n')` but may use specialized paths.

```cpp
size_t n = simdtext::count_newlines(buffer);
```

---

### `contains`

```cpp
[[nodiscard]] bool contains(std::string_view input, std::string_view needle);
```

Checks whether `input` contains `needle`.

| Parameter | Description |
|-----------|-------------|
| `input`   | Haystack |
| `needle`  | Substring to find |

**Returns:** `true` if `needle` is found, `false` otherwise.

```cpp
bool found = simdtext::contains("hello world", "wor");  // true
```

**Note:** For single-byte searches, `find_byte` is faster.

---

### `find_byte`

```cpp
[[nodiscard]] const char* find_byte(const char* begin, const char* end, char byte);
```

Finds the first occurrence of `byte` in `[begin, end)`.

| Parameter | Description |
|-----------|-------------|
| `begin`   | Start of range (inclusive) |
| `end`     | End of range (exclusive) |
| `byte`    | Byte to find |

**Returns:** Pointer to the first occurrence, or `end` if not found.

```cpp
const char* p = simdtext::find_byte(data.data(), data.data() + data.size(), '\n');
if (p != data.data() + data.size()) {
    // found newline at offset p - data.data()
}
```

---

## ASCII — `<simdtext/ascii.hpp>`

ASCII classification and case conversion. SIMD-accelerated.

### `is_ascii`

```cpp
[[nodiscard]] bool is_ascii(std::span<const char> input);
```

Returns `true` if every byte in `input` is in the range 0x00–0x7F.

```cpp
bool ok = simdtext::is_ascii(buffer);
```

---

### `lowercase_ascii_inplace`

```cpp
void lowercase_ascii_inplace(std::span<char> input);
```

Converts ASCII uppercase letters (A–Z) to lowercase (a–z) in-place. Non-ASCII bytes are left unchanged.

```cpp
std::string s = "Hello World 123";
simdtext::lowercase_ascii_inplace(s);  // "hello world 123"
```

**Performance:** No allocation. Processes 16–64 bytes per SIMD iteration.

---

### `uppercase_ascii_inplace`

```cpp
void uppercase_ascii_inplace(std::span<char> input);
```

Converts ASCII lowercase letters (a–z) to uppercase (A–Z) in-place. Non-ASCII bytes are left unchanged.

```cpp
std::string s = "Hello World 123";
simdtext::uppercase_ascii_inplace(s);  // "HELLO WORLD 123"
```

---

### `trim_ascii`

```cpp
[[nodiscard]] std::string_view trim_ascii(std::string_view input);
```

Trims leading and trailing ASCII whitespace (space `' '`, tab `'\t'`, carriage return `'\r'`, newline `'\n'`).

**Returns:** A `string_view` into the original data — no allocation.

```cpp
auto trimmed = simdtext::trim_ascii("  hello world  ");  // "hello world"
```

---

## Lines & Splitting — `<simdtext/lines.hpp>`

Zero-allocation iteration over lines and delimited segments.

### `LineView`

```cpp
class LineView {
public:
    explicit LineView(std::string_view input);
    Iterator begin() const;
    Iterator end() const;
};
```

Iterable view over lines in a text buffer split by `'\n'`. Each line is a `std::string_view` — no allocation, no copying.

`LineView::Iterator` is a forward iterator yielding `std::string_view`.

### `lines`

```cpp
[[nodiscard]] LineView lines(std::string_view input);
```

Creates a `LineView` from `input`.

```cpp
for (std::string_view line : simdtext::lines("foo\nbar\nbaz")) {
    std::cout << line << "\n";
}
// Output: foo  bar  baz
```

**Note:** Carriage returns (`'\r'`) are not stripped. Use `trim_ascii` on each line if needed.

---

### `SplitView`

```cpp
class SplitView {
public:
    SplitView(std::string_view input, char delim);
    Iterator begin() const;
    Iterator end() const;
};
```

Iterable view over segments split by a single-character delimiter.

### `split`

```cpp
[[nodiscard]] SplitView split(std::string_view input, char delimiter);
```

```cpp
for (std::string_view seg : simdtext::split("a,b,c", ',')) {
    std::cout << seg << "\n";
}
// Output: a  b  c
```

---

## Encoding — `<simdtext/encode.hpp>`

Hex and Base64 encoding/decoding. Requires `<simdtext/types.hpp>` for `DecodeResult`.

### Hex

#### `hex_encode`

```cpp
[[nodiscard]] std::string hex_encode(std::span<const std::byte> input);
```

Encodes bytes to a hexadecimal string (lowercase).

```cpp
auto bytes = std::vector<std::byte>{std::byte{0xDE}, std::byte{0xAD}};
std::string hex = simdtext::hex_encode(bytes);  // "dead"
```

#### `hex_encode_to`

```cpp
[[nodiscard]] size_t hex_encode_to(std::span<const std::byte> input, std::span<char> output);
```

Encodes bytes to hexadecimal into a pre-allocated buffer. Output must be at least `2 * input.size()` bytes.

**Returns:** Bytes written, or `0` on error (output too small).

```cpp
char buf[64];
size_t n = simdtext::hex_encode_to(input, buf);
```

#### `hex_decode`

```cpp
[[nodiscard]] std::vector<std::byte> hex_decode(std::string_view input);
```

Decodes a hex string to bytes. Returns empty vector on invalid input.

#### `hex_decode_to`

```cpp
[[nodiscard]] DecodeResult hex_decode_to(std::string_view input, std::span<std::byte> output);
[[nodiscard]] DecodeResult hex_decode_to(std::string_view input, std::span<char> output);
```

Decodes hex into a pre-allocated buffer. Output must be at least `input.size() / 2` bytes.

**Returns:** `DecodeResult` with `bytes_written` and `error` code.

```cpp
char buf[32];
auto result = simdtext::hex_decode_to("deadbeef", buf);
if (result.ok()) {
    // result.bytes_written bytes available in buf
}
```

### Base64

#### `base64_encode`

```cpp
[[nodiscard]] std::string base64_encode(std::span<const std::byte> input);
```

Encodes bytes to Base64 (standard alphabet with `+` and `/`, `=` padding).

#### `base64_encode_to`

```cpp
[[nodiscard]] size_t base64_encode_to(std::span<const std::byte> input, std::span<char> output);
```

Encodes to a pre-allocated buffer. Output must be at least `4 * ceil(input.size() / 3.0)` bytes.

**Returns:** Bytes written, or `0` on error.

#### `base64_decode`

```cpp
[[nodiscard]] std::vector<std::byte> base64_decode(std::string_view input);
```

Decodes a Base64 string. Returns empty vector on invalid input.

#### `base64_decode_to`

```cpp
[[nodiscard]] DecodeResult base64_decode_to(std::string_view input, std::span<std::byte> output);
[[nodiscard]] DecodeResult base64_decode_to(std::string_view input, std::span<char> output);
```

Decodes Base64 into a pre-allocated buffer.

---

## URL — `<simdtext/url.hpp>`

URL encoding/decoding and query string parsing.

### `url_encode`

```cpp
[[nodiscard]] std::string url_encode(std::string_view input);
```

URL-encodes a string. Unreserved characters (A–Z, a–z, 0–9, `-`, `_`, `.`, `~`) pass through; all others become `%XX`.

```cpp
auto encoded = simdtext::url_encode("hello world");  // "hello%20world"
```

### `url_encode_to`

```cpp
[[nodiscard]] size_t url_encode_to(std::string_view input, std::span<char> output);
```

URL-encodes into a pre-allocated buffer. Worst-case output size is `3 * input.size()`.

**Returns:** Bytes written, or `0` if output is too small.

### `url_decode`

```cpp
[[nodiscard]] std::string url_decode(std::string_view input);
```

Decodes `%XX` sequences. Invalid sequences are passed through unchanged.

```cpp
auto decoded = simdtext::url_decode("hello%20world");  // "hello world"
```

### `url_decode_to`

```cpp
[[nodiscard]] size_t url_decode_to(std::string_view input, std::span<char> output);
```

Decodes into a pre-allocated buffer. Output size is at most `input.size()`.

**Returns:** Bytes written, or `0` on error.

### `parse_query`

```cpp
[[nodiscard]] std::unordered_map<std::string, std::string> parse_query(std::string_view query);
```

Parses a URL query string into key-value pairs. Handles URL-decoding of keys and values.

```cpp
auto params = simdtext::parse_query("name=tony&lang=c%2B%2B");
// params["name"] == "tony"
// params["lang"] == "c++"
```

---

## UTF-8 — `<simdtext/utf8.hpp>`

### `valid_utf8`

```cpp
[[nodiscard]] bool valid_utf8(std::span<const char> input);
```

Validates UTF-8 encoding. Checks for overlong encodings, truncated sequences, invalid continuation bytes, and surrogate pairs.

```cpp
bool ok = simdtext::valid_utf8(buffer);
```

**Performance:** SIMD-accelerated via Google Highway where available.

---

## File I/O — `<simdtext/file.hpp>`

Memory-mapped file access and line-by-line scanning.

### `MappedFile`

```cpp
class MappedFile {
public:
    MappedFile();
    explicit MappedFile(const char* path);
    ~MappedFile();

    MappedFile(MappedFile&&) noexcept;
    MappedFile& operator=(MappedFile&&) noexcept;

    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    [[nodiscard]] bool open(const char* path);
    [[nodiscard]] std::string_view view() const noexcept;
    [[nodiscard]] size_t size() const noexcept;
};
```

Memory-mapped file for zero-copy read access. Uses `mmap` on POSIX, `CreateFileMapping` on Windows.

```cpp
simdtext::MappedFile mf("server.log");
if (mf.size() > 0) {
    std::string_view data = mf.view();
    // process data — no copy, no allocation
}
```

**Thread safety:** Read-only access is safe from multiple threads. The file must not be modified externally while mapped.

### `FileScanner`

```cpp
class FileScanner {
public:
    explicit FileScanner(const char* path);
    explicit FileScanner(const std::string& path);

    [[nodiscard]] bool is_open() const;

    void each_line(std::function<void(std::string_view)> callback) const;
    void each_line_containing(std::string_view needle,
                              std::function<void(std::string_view)> callback) const;
    [[nodiscard]] size_t count_lines() const;
    [[nodiscard]] size_t count_matching(std::string_view needle) const;
};
```

High-level file scanner built on `MappedFile`. Memory-maps the file and iterates lines via `LineView`.

```cpp
simdtext::FileScanner scanner("access.log");
scanner.each_line_containing("ERROR", [](std::string_view line) {
    std::cout << line << "\n";
});
size_t total = scanner.count_lines();
size_t errors = scanner.count_matching("ERROR");
```

---

## Types — `<simdtext/types.hpp>`

### `ErrorCode`

```cpp
enum class ErrorCode {
    Ok = 0,
    InvalidChar,
    InvalidLength,
    OutputTooSmall,
};
```

Error codes returned by decode operations.

### `DecodeResult`

```cpp
struct DecodeResult {
    size_t bytes_written = 0;
    size_t error_offset = 0;
    ErrorCode error = ErrorCode::Ok;

    [[nodiscard]] bool ok() const noexcept;
};
```

Result of a decode operation. Check `ok()` first, then read `bytes_written`. On error, `error_offset` indicates the position of the invalid input.

---

## C API — `<simdtext/c/simdtext.h>`

C-compatible API for use from C, Rust, Python, etc. Link against `simdtext_c`.

### Version

```c
int simdtext_version_major(void);   // 0
int simdtext_version_minor(void);   // 1
int simdtext_version_patch(void);   // 0
```

### Scanning

```c
size_t simdtext_count_byte(const char* data, size_t len, char byte);
size_t simdtext_count_newlines(const char* data, size_t len);
int    simdtext_contains(const char* haystack, size_t haystack_len,
                          const char* needle, size_t needle_len);
```

### ASCII

```c
int  simdtext_is_ascii(const char* data, size_t len);
void simdtext_lowercase_ascii(char* data, size_t len);
void simdtext_uppercase_ascii(char* data, size_t len);
```

### UTF-8

```c
int simdtext_valid_utf8(const char* data, size_t len);
```

### Encoding

```c
// Returns heap-allocated string. Caller must free with simdtext_free().
char* simdtext_hex_encode(const char* data, size_t len);
char* simdtext_base64_encode(const char* data, size_t len);
```

### URL

```c
char* simdtext_url_encode(const char* data, size_t len);
char* simdtext_url_decode(const char* data, size_t len);
```

### File I/O

```c
typedef struct simdtext_file* simdtext_file_t;

simdtext_file_t simdtext_file_open(const char* path);
void            simdtext_file_close(simdtext_file_t file);
const char*     simdtext_file_data(simdtext_file_t file);
size_t          simdtext_file_size(simdtext_file_t file);
size_t          simdtext_file_count_lines(simdtext_file_t file);
```

### Memory

```c
void simdtext_free(void* ptr);
```

Free strings returned by `simdtext_hex_encode`, `simdtext_base64_encode`, `simdtext_url_encode`, `simdtext_url_decode`. On Windows, this ensures the correct heap is used.

---

## Version Macros — `<simdtext/version.hpp>`

```cpp
#define SIMDTEXT_VERSION_MAJOR 0
#define SIMDTEXT_VERSION_MINOR 1
#define SIMDTEXT_VERSION_PATCH 0
#define SIMDTEXT_VERSION "0.1.0"
```

## Export Macros — `<simdtext/export.hpp>`

| Macro | Purpose |
|-------|---------|
| `SIMDTEXT_API` | Shared library export/import |
| `SIMDTEXT_NODISCARD` | `[[nodiscard]]` |
| `SIMDTEXT_FORCE_INLINE` | Force inline on hot paths |
| `SIMDTEXT_TARGET_SSE2` | GCC/Clang `target("sse2")` attribute |
| `SIMDTEXT_TARGET_AVX2` | GCC/Clang `target("avx2,bmi")` attribute |
| `SIMDTEXT_TARGET_NEON` | GCC/Clang `target("+neon")` attribute |
| `SIMDTEXT_COLD` | Cold function hint |
| `SIMDTEXT_DEPRECATED(msg)` | Deprecation marker |

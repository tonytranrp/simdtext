# simdtext Restructuring Plan — C++23 Production Library

## Target Directory Layout

```
simdtext/
├── CMakeLists.txt
├── cmake/
│   ├── simdtext-config.cmake.in
│   ├── simdtext-config-version.cmake.in
│   └── FindHighway.cmake
├── cmake_presets.json
├── vcpkg.json
├── .clang-format
├── .clang-tidy
├── include/
│   └── simdtext/
│       ├── simdtext.hpp          # Umbrella header (includes all below)
│       ├── types.hpp             # ErrorCode, DecodeError, expected aliases
│       ├── ascii.hpp             # is_ascii, lowercase/uppercase, trim
│       ├── scan.hpp              # count_byte, count_newlines, contains, find_byte
│       ├── lines.hpp             # LineView, lines()
│       ├── split.hpp             # SplitView, split()
│       ├── encode.hpp            # hex/base64 encode/decode
│       ├── url.hpp               # url_encode/decode, parse_query
│       ├── utf8.hpp              # valid_utf8
│       ├── file.hpp              # MappedFile, FileScanner, FileScannerBuilder
│       └── version.hpp           # SIMDTEXT_VERSION_* macros (generated)
├── src/
│   ├── scalar/
│   │   └── scalar.cpp
│   ├── highway/
│   │   └── simd_hwy.cpp
│   ├── encode/
│   │   └── encode.cpp
│   ├── url/
│   │   └── url.cpp
│   └── file/
│       └── file.cpp
├── tests/
│   ├── CMakeLists.txt
│   ├── test_ascii.cpp
│   ├── test_scan.cpp
│   ├── test_lines.cpp
│   ├── test_split.cpp
│   ├── test_encode.cpp
│   ├── test_url.cpp
│   ├── test_utf8.cpp
│   └── test_file.cpp
├── cli/
│   └── main.cpp
├── examples/
│   ├── stats.cpp
│   └── quickstart.cpp
├── benchmarks/
│   ├── CMakeLists.txt
│   └── bench_core.cpp
├── docs/
│   ├── api.md
│   └── getting-started.md
└── .github/
    └── workflows/
        └── ci.yml
```

---

## 1. Header Split — File-by-File Changes

### `include/simdtext/types.hpp`
Move from `simdtext.hpp`:
- `enum class ErrorCode` → rename to `DecodeError` (more descriptive)
- `struct DecodeResult` → replace with `std::expected` (see §2)
- Add:
```cpp
#pragma once
#include <expected>
#include <cstddef>
#include <system_error>

namespace simdtext {

/// Error type for decode operations.
enum class DecodeError {
    InvalidChar,      // unexpected character in input
    InvalidLength,    // input length not a multiple of expected size
    OutputTooSmall,   // output buffer too small
};

/// Convenience alias: decode result is either T or DecodeError.
template<typename T>
using DecodeResult = std::expected<T, DecodeError>;

/// Specialization for size-only results.
using DecodeSize = std::expected<size_t, DecodeError>;

} // namespace simdtext
```

### `include/simdtext/ascii.hpp`
Move from `simdtext.hpp`:
- `is_ascii()`
- `lowercase_ascii_inplace()`
- `uppercase_ascii_inplace()`
- `trim_ascii()`

Add `constexpr` where possible (scalar paths can be constexpr; SIMD dispatch cannot).

### `include/simdtext/scan.hpp`
Move from `simdtext.hpp`:
- `count_byte()`
- `count_newlines()`
- `contains()`
- `find_byte()`

### `include/simdtext/lines.hpp`
Move from `simdtext.hpp`:
- `class LineView` + `LineView::Iterator`
- `lines()` factory

Add (C++23):
- `operator|` adapter: `sv | simdtext::views::lines`
- Sentinel-based end (replace pointer comparison with `std::default_sentinel_t`)
- Future: `std::generator<std::string_view> lines(std::string_view)` when compilers support it

### `include/simdtext/split.hpp`
Move from `simdtext.hpp`:
- `class SplitView` + `SplitView::Iterator`
- `split()` factory

Add:
- `operator|` adapter: `sv | simdtext::views::split_by(',')`
- Sentinel-based end
- String-view delimiter overload: `split(sv, std::string_view delim)`

### `include/simdtext/encode.hpp`
Move from `simdtext.hpp`:
- All `hex_encode*` / `hex_decode*` / `base64_encode*` / `base64_decode*`

**API change**: Replace `DecodeResult` (old struct) with `std::expected`:
```cpp
// Before:
DecodeResult hex_decode_to(std::string_view input, std::span<std::byte> output);

// After:
DecodeSize hex_decode_to(std::string_view input, std::span<std::byte> output);
// Returns expected<size_t, DecodeError> — size on success, error on failure

// Before:
std::vector<std::byte> hex_decode(std::string_view input);  // silent failure

// After:
DecodeResult<std::vector<std::byte>> hex_decode(std::string_view input);
// Returns expected<vector<byte>, DecodeError> — explicit error handling
```

### `include/simdtext/url.hpp`
Move from `simdtext.hpp`:
- `url_encode_to()`, `url_encode()`, `url_decode_to()`, `url_decode()`
- `parse_query()`

**API change**:
```cpp
// Before:
std::unordered_map<std::string, std::string> parse_query(std::string_view query);

// After (C++23):
#include <flat_map>
std::flat_map<std::string, std::string> parse_query(std::string_view query);
// flat_map: better cache locality for small query strings, sorted keys
```

### `include/simdtext/utf8.hpp`
Move from `simdtext.hpp`:
- `valid_utf8()`

Add:
- `validate_utf8_with_offset()` → returns `std::expected<void, size_t>` (offset of first invalid byte)

### `include/simdtext/file.hpp`
Move from `simdtext.hpp`:
- `class MappedFile`
- `class FileScanner`

**API changes**:
- `MappedFile::open()` returns `std::expected<void, std::error_code>` instead of `bool`
- Add `FileScanner::Builder`:
```cpp
class FileScanner::Builder {
public:
    Builder&& with_path(const char* path) &&;
    Builder&& with_needle(std::string_view needle) &&;
    Builder&& with_skip_empty(bool skip = true) &&;
    FileScanner build() &&;
private:
    // ...
};
```
- `FileScanner::lines()` returns `LineView` instead of callback-based `each_line()`
- Keep `each_line()` for backward compat, mark `[[deprecated]]`

### `include/simdtext/simdtext.hpp` (umbrella)
```cpp
#pragma once
#include <simdtext/version.hpp>
#include <simdtext/types.hpp>
#include <simdtext/ascii.hpp>
#include <simdtext/scan.hpp>
#include <simdtext/lines.hpp>
#include <simdtext/split.hpp>
#include <simdtext/encode.hpp>
#include <simdtext/url.hpp>
#include <simdtext/utf8.hpp>
#include <simdtext/file.hpp>
```

### `include/simdtext/version.hpp`
Generated by CMake `configure_file`:
```cpp
#pragma once
#define SIMDTEXT_VERSION_MAJOR @PROJECT_VERSION_MAJOR@
#define SIMDTEXT_VERSION_MINOR @PROJECT_VERSION_MINOR@
#define SIMDTEXT_VERSION_PATCH @PROJECT_VERSION_PATCH@
#define SIMDTEXT_VERSION "@PROJECT_VERSION@"
```

---

## 2. C++23 Modernization Details

| Feature | Where | How |
|---|---|---|
| `std::expected<T,E>` | `types.hpp`, `encode.hpp`, `url.hpp`, `file.hpp` | Replace old `DecodeResult` / `bool` returns |
| `std::flat_map` | `url.hpp` `parse_query()` | Replace `unordered_map` |
| Deducing this | `LineView::Iterator`, `SplitView::Iterator` | `template<typename Self> auto&& operator*(this Self&& self)` for const/mixed overloads |
| `std::print` | `cli/main.cpp` | Replace `printf`/`std::cout` |
| `if constexpr` | `encode.cpp`, `scalar.cpp` | Branch on template params at compile time |
| Concepts | `types.hpp`, public API | `template<std::ranges::contiguous_range R> size_t count_byte(R&& input, char byte)` |
| `constexpr` | `ascii.hpp` scalar paths, `hex_val()` | Mark constexpr; SIMD paths dispatch at runtime |
| `std::generator` | `lines.hpp`, `split.hpp` | Conditional: `#if __cpp_generator` provide generator overload |
| Sentinel | `LineView::Iterator`, `SplitView::Iterator` | End sentinel uses `std::default_sentinel_t` instead of empty Iterator |
| Modules prep | All headers | Ensure no macros leak, no `using namespace` in headers, add `export` comment markers |

---

## 3. Build System Changes

### `CMakeLists.txt` (root)
Key changes:
- `cmake_minimum_required(VERSION 3.25)` — needed for presets + C++23
- `set(CMAKE_CXX_STANDARD 23)`
- `configure_file(version.hpp.in include/simdtext/version.hpp)`
- `target_compile_features(simdtext PUBLIC cxx_std_23)`
- Add `option(BUILD_SHARED_LIBS)` support
- Generate CMake package config:
```cmake
include(CMakePackageConfigHelpers)
configure_package_config_file(
    cmake/simdtext-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/simdtext-config.cmake
    INSTALL_DESTINATION lib/cmake/simdtext
)
write_basic_package_version_file(...)
install(TARGETS simdtext EXPORT simdtext-targets ...)
install(EXPORT simdtext-targets NAMESPACE simdtext:: ...)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/simdtext-config.cmake ...)
```
- Tests: `if(SIMDTEXT_BUILD_TESTS) add_subdirectory(tests) endif()`
- Benchmarks: `if(SIMDTEXT_BENCHMARKS) add_subdirectory(benchmarks) endif()`
- Examples: `if(SIMDTEXT_BUILD_EXAMPLES) add_subdirectory(examples) endif()`
- Version header generation via `configure_file`

### `cmake/simdtext-config.cmake.in`
Standard CMake package config for `find_package(simdtext)`.

### `cmake_presets.json`
```json
{
  "version": 6,
  "configurePresets": [
    { "name": "dev", "binaryDir": "build/dev", "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug", "SIMDTEXT_BUILD_TESTS": "ON", "SIMDTEXT_BUILD_EXAMPLES": "ON" } },
    { "name": "release", "binaryDir": "build/release", "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" } },
    { "name": "asan", "binaryDir": "build/asan", "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug", "CMAKE_CXX_FLAGS": "-fsanitize=address,undefined -fno-omit-frame-pointer" } },
    { "name": "coverage", "binaryDir": "build/coverage", "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug", "CMAKE_CXX_FLAGS": "--coverage" } },
    { "name": "no-hwy", "binaryDir": "build/no-hwy", "cacheVariables": { "SIMDTEXT_USE_HIGHWAY": "OFF" } }
  ],
  "buildPresets": [
    { "name": "dev", "configurePreset": "dev" },
    { "name": "release", "configurePreset": "release" }
  ],
  "testPresets": [
    { "name": "dev", "configurePreset": "dev", "output": { "outputOnFailure": true } }
  ]
}
```

### `vcpkg.json`
```json
{
  "name": "simdtext",
  "version": "0.2.0",
  "dependencies": [
    { "name": "highway", "version>=": "1.1" }
  ],
  "overrides": [],
  "builtin-baseline": ""
}
```

---

## 4. Testing

### `tests/CMakeLists.txt`
```cmake
find_package(Catch2 3 REQUIRED)

add_executable(simdtext_tests
    test_ascii.cpp test_scan.cpp test_lines.cpp test_split.cpp
    test_encode.cpp test_url.cpp test_utf8.cpp test_file.cpp
)
target_link_libraries(simdtext_tests PRIVATE simdtext Catch2::Catch2WithMain)
target_compile_features(simdtext_tests PRIVATE cxx_std_23)

include(CTest)
include(Catch)
catch_discover_tests(simdtext_tests)
```

### Test Coverage Plan

| Module | Edge Cases |
|---|---|
| `ascii` | Empty input, all-ASCII, mixed UTF-8, trim with only whitespace, trim empty |
| `scan` | count_byte with 0 occurrences, needle at start/end, contains empty needle, find_byte not found |
| `lines` | Empty string, single line no newline, trailing newline, Windows \r\n, very long line |
| `split` | Empty string, delimiter not found, consecutive delimiters, single-char input |
| `encode` | Empty input, invalid hex chars, odd-length hex, base64 padding, buffer too small |
| `url` | No special chars, all special chars, invalid %xx sequences, empty query, duplicate keys |
| `utf8` | Valid ASCII, valid multibyte, overlong encoding, truncated sequences, surrogate pairs |
| `file` | Nonexistent file, empty file, large file (>4GB if possible), binary file with nulls |

### Property-Based Testing Ideas
- **hex roundtrip**: `hex_decode(hex_encode(bytes)) == bytes` for random byte sequences
- **url roundtrip**: `url_decode(url_encode(s)) == s` for random strings
- **base64 roundtrip**: same as hex
- **split reassembly**: `join(split(s, d), d) == s` for strings not ending in delimiter

---

## 5. Code Quality

### `.clang-format`
```yaml
BasedOnStyle: LLVM
Language: Cpp
Standard: c++23
ColumnLimit: 100
IndentWidth: 4
BreakBeforeBraces: Attach
AllowShortFunctionsOnASingleLine: Inline
SortIncludes: CaseInsensitive
```

### `.clang-tidy`
```yaml
Checks: >
  -*,
  bugprone-*,
  cppcoreguidelines-*,
  modernize-*,
  performance-*,
  readability-*,
  -modernize-use-trailing-return-type,
  -readability-magic-numbers
```

### CI Sanitizers & Coverage
In `.github/workflows/ci.yml` add matrix:
- `asan` build: `-fsanitize=address,undefined`
- `tsan` build: `-fsanitize=thread` (for concurrent tests if added)
- Coverage: `gcov`/`lcov` + upload to Codecov

---

## 6. Implementation Order

1. **Header split** — move declarations into individual headers, create umbrella header
2. **`types.hpp` + `std::expected` migration** — new error types, update encode/url/file APIs
3. **`version.hpp` + CMake modernization** — presets, vcpkg, install targets
4. **`lines.hpp` / `split.hpp` modernization** — sentinel, deducing this, view adapters
5. **`url.hpp`** — `flat_map`, `std::expected` returns
6. **`file.hpp`** — Builder pattern, `std::expected` returns, `[[deprecated]]` old API
7. **Tests** — Catch2 integration, comprehensive coverage
8. **Code quality** — clang-format, clang-tidy, CI hardening
9. **Docs** — API reference, getting started guide
10. **C++23 polish** — `std::generator`, modules prep, `std::print` in CLI

---

## 7. Backward Compatibility Notes

- Keep `simdtext.hpp` umbrella header working (includes all sub-headers)
- `DecodeResult` old struct → `[[deprecated("Use std::expected")]]` alias for one release, then remove
- `FileScanner::each_line()` → `[[deprecated("Use FileScanner::lines()")]]`
- `parse_query()` return type change is binary-breaking; bump to v0.2.0
- All `*_to()` functions that returned `size_t` / `DecodeResult` now return `std::expected` — major API change, requires v0.2.0

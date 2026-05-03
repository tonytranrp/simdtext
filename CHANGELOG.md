# Changelog

All notable changes to simdtext will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.2.2] - 2026-05-03

### Performance
- **hex_decode**: 2.3 GB/s → 12.3 GB/s (**5.3×**) via AVX2 maddubs + packus_epi16 (fast-hex technique)
- **url_decode**: 676 MB/s → 1.55 GB/s (**2.3×**) — consecutive %XX processing eliminates SIMD scan overhead
- **lowercase/uppercase**: 4× loop unrolling + software prefetch for large buffer throughput
- **url_encode**: simplified to direct LUT classification (same speed, simpler code)

### Bug Fixes
- Fixed hex_encode SSSE3 lo_mask_arr only initializing 1 byte instead of all 16
- Fixed hex_decode roundtrip failures for sizes ≥16 bytes (SSSE3 cross-byte leakage bug)
- Replaced broken SSSE3 hex_decode with working AVX2 maddubs approach

## [0.2.1] - 2026-05-02

### Performance
- **hex_encode**: 1.16 GB/s → 17.2 GB/s (**15×**) via SSSE3/AVX2 SIMD pshufb-based implementation
- **hex_decode**: now 12.3 GB/s via AVX2 maddubs (was 2.24 GB/s scalar)
- **base64_encode**: 1.56 GB/s → 21.5 GB/s (**14×**) via AVX2 mulhi/mullo reshuffle
- **base64_decode**: 2.41 GB/s → 3.61 GB/s (**50%**) via AVX2 SIMD
- **url_encode**: 257 MB/s → 1.14 GB/s (**4.4×**) via LUT classification
- **url_decode**: 859 MB/s → 1.55 GB/s (**1.8×**) via Highway SIMD + consecutive %XX
- **lowercase/uppercase**: ~18 GB/s with 4× unrolled + prefetch
- **lines/split**: 5.1 GB/s → ~10 GB/s (**2×**) via SIMD-accelerated newline scanning
- **valid_utf8**: 104.1 GB/s via Highway
- **is_ascii**: 103.6 GB/s via Highway
- **count_byte**: 12.3 GB/s via AVX2

### Features
- NEON implementations for `count_code_points` and `validate_utf8` on ARM
- SSSE3 intrinsics backend for hex encode/decode
- AVX2 intrinsics backend for hex and base64 encode/decode
- Highway SIMD backend for url encode/decode (portable across x86 and ARM)

### Bug Fixes
- Fixed lowercase/uppercase cache thrashing that limited throughput to ~16 GB/s

### Build
- Zero compiler warnings across all configurations (GCC, Clang, MSVC)
- SIMD backends now accurately listed: SSSE3, AVX2, AVX-512, NEON, Highway

## [0.2.0] - 2026-05-02

### Added
- **Parallel processing** (`parallel.hpp`): `parallel_count_byte`, `parallel_is_ascii`, `parallel_count_newlines`, `parallel_find_byte`, `parallel_valid_utf8`, `parallel_for_each_chunk` — multi-threaded versions of core operations with `ParallelOptions` (configurable thread count, min chunk size)
- **Pattern scanning** (`pattern.hpp`): `find_pattern` with wildcard support (`??`), `byte_pattern_parse`, `BytePattern` class — SIMD-accelerated byte pattern matching
- **Hashing** (`hash.hpp`): `fnv1a` (constexpr), `SIMDTEXT_HASH` macro for switch-case string matching, `crc32` (software), `crc32c` (10.8 GB/s hardware), `xxhash64` (22 GB/s), `wyhash` (26 GB/s)
- **String utilities** (`str.hpp`): `trim_left`/`trim_right`/`trim`, `replace_all` (char and substring), `fields` (whitespace split), `split_vec`/`split_into`, `starts_with`/`ends_with`, `contains_char`
- **JSON tokenizer** (`json.hpp`): `JsonTokenizer` — zero-allocation pull-style tokenizer with all JSON types, `looks_like_json`, `is_json_number`, `json_unescape_inplace`
- **CSV parser** (`csv.hpp`): `CsvParser` — row-by-row iteration with quoted fields, custom delimiters, `parse_csv_row` helper
- **Diff utilities** (`diff.hpp`): `line_diff` (LCS-based), `count_diff_lines`, `text_equal`, `common_prefix_length`/`common_suffix_length`
- **Log parser** (`log.hpp`): `parse_log_line`, `parse_log_level`, `count_log_levels`, `filter_log_lines` — structured log analysis
- **UTF-8 streaming** (`utf8.hpp`): `Utf8Validator` class for chunk-by-chunk validation, `count_code_points`/`utf8_length`
- **C API**: `simdtext_find_pattern`, `simdtext_byte_pattern_parse`, `simdtext_parallel_count_byte`
- **AVX-512BW** build support and install rules

### Changed
- **UTF-8 validation**: 3.4 GB/s → **32.4 GB/s** via SSE2 pshufb-based byte classification with proper runtime dispatch
- **CRC32C**: 0.5 GB/s → **10.8 GB/s** via runtime SSE4.2 hardware dispatch (inline asm)
- **count_code_points**: 5.6 GB/s → **8.4 GB/s** via SSE2 SIMD dispatch
- **Scalar functions**: Fixed auto-vectorization to AVX-512 (added `no-tree-vectorize` attribute)
- **Scalar is_ascii/count_byte**: 4x unrolled SWAR (32 bytes/iteration, single branch)
- **AVX2 functions**: Added `no-avx512*` pragma targets to prevent instruction leakage

### Fixed
- **hex_decode_to**: Accepted 'G'-'Z' as valid hex digits (values 16-35) — now properly rejects them
- **url_decode**: `%GG` was incorrectly decoded — now checks `hi <= 15 && lo <= 15`
- **hex_val()** in url.cpp: Returned values > 15 for non-hex alpha chars — fixed to return -1
- **UTF-8 SIMD dispatch**: Was using compile-time `#if defined(__AVX2__)` which was never true — fixed to use runtime CPU detection
- **CRC32C software fallback**: Table was truncated/incomplete — fixed with full 256-entry Castagnoli table
- **cpu_detect.hpp**: Missing `#include <cstddef>` for `size_t`
- **Header hygiene**: Added missing `SIMDTEXT_API` on all parallel functions, missing includes

### Removed
- Removed incomplete SSSE3 base64 SIMD (compile issues in non-SSSE3 object files)

## [0.1.0] - 2026-05-02

### Added
- Initial release with core SIMD-accelerated operations
- `count_byte`, `count_newlines`, `is_ascii`, `find_byte` (scan.hpp)
- `lowercase_ascii_inplace`, `uppercase_ascii_inplace`, `trim_ascii` (ascii.hpp)
- `LineView`, `SplitView`, `lines`, `split` (lines.hpp)
- `hex_encode`/`decode`, `base64_encode`/`decode` (encode.hpp)
- `url_encode`/`decode`, `parse_query` (url.hpp)
- `valid_utf8` (utf8.hpp)
- `MappedFile`, `FileScanner` (file.hpp)
- C API (`c/simdtext.h`)
- CLI tool: `simdtext stats`, `grep`, `count`, `lower`, `upper`, `validate-utf8`, etc.
- Google Highway SIMD backend (SSE2 → AVX2 → AVX-512, NEON)
- Hand-written SSE2/AVX2/AVX-512BW intrinsics
- CMake build system with `add_subdirectory`, `find_package`, vcpkg support
- Cross-platform CI (GCC, Clang, MSVC on Linux, macOS, Windows)
- 163 tests, all passing

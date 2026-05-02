# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2025-05-02

### Added

- **Scanning** (`<simdtext/scan.hpp>`): `count_byte`, `count_newlines`, `contains`, `find_byte`
- **ASCII** (`<simdtext/ascii.hpp>`): `is_ascii`, `lowercase_ascii_inplace`, `uppercase_ascii_inplace`, `trim_ascii`
- **Lines & Splitting** (`<simdtext/lines.hpp>`): `LineView`, `SplitView`, `lines()`, `split()`
- **Encoding** (`<simdtext/encode.hpp>`): `hex_encode`, `hex_encode_to`, `hex_decode`, `hex_decode_to`, `base64_encode`, `base64_encode_to`, `base64_decode`, `base64_decode_to`
- **URL** (`<simdtext/url.hpp>`): `url_encode`, `url_encode_to`, `url_decode`, `url_decode_to`, `parse_query`
- **UTF-8** (`<simdtext/utf8.hpp>`): `valid_utf8`
- **File I/O** (`<simdtext/file.hpp>`): `MappedFile`, `FileScanner` with `each_line`, `each_line_containing`, `count_lines`, `count_matching`
- **Types** (`<simdtext/types.hpp>`): `ErrorCode` enum, `DecodeResult` struct
- **C API** (`<simdtext/c/simdtext.h>`): FFI-friendly bindings for scanning, ASCII, encoding, URL, UTF-8, and file operations
- **CLI tool**: `simdtext stats`, `grep`, `count`, `lower`, `upper`, `hex-encode`, `base64-encode`, `url-decode`, `url-encode`, `validate-utf8`
- **SIMD acceleration**: Google Highway (SSE2/AVX2/AVX-512/NEON) with hand-written SSE2/AVX2 intrinsics fallback
- **CMake integration**: `add_subdirectory`, `find_package`, FetchContent, vcpkg support
- **Tests**: Comprehensive test suite covering all modules
- **Benchmarks**: Google Benchmark integration for core operations
- **Documentation**: API reference, architecture guide, building guide, benchmark guide

[0.1.0]: https://github.com/tonytran-ai/simdtext/releases/tag/v0.1.0

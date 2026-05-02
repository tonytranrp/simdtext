# simdtext Architecture

## Overview

simdtext is a high-performance C++ text-processing library with SIMD acceleration. It provides fast scanning, zero-copy views, encoding/decoding, and file I/O for large text buffers.

## Architecture Layers

```
┌─────────────────────────────────────────────────────────────┐
│                    Public API Layer                          │
│  scan.hpp, ascii.hpp, lines.hpp, encode.hpp, url.hpp, ...  │
│  (returns string_view/span, zero-allocation hot paths)      │
├─────────────────────────────────────────────────────────────┤
│                    Dispatch Layer                            │
│  src/detail/simd_dispatch.cpp                               │
│  (CPU feature detection → select best backend)              │
├──────────┬──────────┬──────────┬──────────┬────────────────┤
│ Highway  │   AVX2   │   SSE2   │   NEON   │    Scalar      │
│ (portable│(intrinsics│(intrinsics│(intrinsics│  (SWAR +      │
│  SIMD)   │ + flags) │ + flags) │ + flags) │  fallback)     │
├──────────┴──────────┴──────────┴──────────┴────────────────┤
│                    Platform Layer                            │
│  cpu_detect.cpp (CPUID, getauxval, /proc/cpuinfo)           │
│  file.cpp (mmap, FileScanner)                               │
│  parallel.hpp (std::thread chunk processing)                │
└─────────────────────────────────────────────────────────────┘
```

## SIMD Dispatch Model

simdtext uses a **multi-layer SIMD strategy**:

1. **Google Highway (preferred)**: Portable SIMD with runtime dispatch. Single codebase that compiles for SSE2, AVX2, AVX-512, NEON, and more. Highway handles the dispatch internally using `HWY_DYNAMIC_DISPATCH`.

2. **Hand-written intrinsics (optional)**: Separate object files compiled with per-ISA flags (`-msse2`, `-mavx2`, `-mavx512bw`). Used when Highway can't express an operation, or for performance-critical paths where ISA-specific tricks matter (e.g., `pshufb` LUTs, `multishift`).

3. **Scalar SWAR fallback**: Process 8 bytes at a time using 64-bit integer operations. Always available, always correct. Used on platforms without SIMD or when SIMD is disabled.

### CPU Feature Detection

`src/detail/cpu_detect.cpp` detects available CPU features at startup:
- **x86**: Uses `__cpuid` / `__get_cpuid` to check SSE2, AVX2, AVX-512BW
- **ARM**: Uses `getauxval(AT_HWCAP)` to check NEON
- **Fallback**: If detection fails, assumes SSE2 (x86-64) or scalar

### Runtime Dispatch Flow

```
Public API: count_byte(data, byte)
    │
    ├── If Highway available: HWY_DYNAMIC_DISPATCH(count_byte_vec)
    │   └── Highway picks best target at runtime (AVX-512 > AVX2 > SSE2 > Scalar)
    │
    ├── Else if AVX2 detected: detail::avx2::count_byte()
    ├── Else if SSE2 detected: detail::sse2::count_byte()
    ├── Else if NEON detected: detail::neon::count_byte()
    └── Else: detail::scalar::count_byte() (SWAR)
```

## Zero-Allocation Design

### Core Principle
Hot-path functions **never allocate**. They return views (`string_view`, `span`) into existing memory.

### API Patterns
| Pattern | Allocates? | Example |
|---------|-----------|---------|
| View return | No | `trim_ascii()` → `string_view` |
| In-place mutation | No | `lowercase_ascii_inplace()` |
| Caller-allocated output | No | `hex_encode_to(buf, out)` |
| Convenience wrapper | Yes | `hex_encode()` → `string` (explicit allocation) |

## Directory Structure

```
simdtext/
├── include/simdtext/          # Public headers
│   ├── simdtext.hpp           # Main include (includes all)
│   ├── scan.hpp               # Byte counting, finding
│   ├── ascii.hpp              # ASCII classification/case
│   ├── lines.hpp              # Zero-alloc line iteration
│   ├── encode.hpp             # Hex, Base64
│   ├── url.hpp                # URL encode/decode, query parsing
│   ├── utf8.hpp               # UTF-8 validation
│   ├── file.hpp               # Memory-mapped file I/O
│   ├── hash.hpp               # FNV-1a, CRC32, xxHash, Wyhash
│   ├── str.hpp                # String utilities
│   ├── pattern.hpp            # Byte pattern scanning
│   ├── parallel.hpp           # Multi-threaded operations
│   ├── csv.hpp                # CSV parsing
│   ├── json.hpp               # JSON tokenizer
│   ├── xml.hpp                # XML tokenizer
│   ├── markdown.hpp           # Markdown parsing
│   ├── config.hpp             # Config file parsing
│   ├── diff.hpp               # Text diffing
│   ├── log.hpp                # Log format parsing
│   ├── types.hpp              # ErrorCode, DecodeResult
│   ├── config.hpp             # Build config
│   ├── version.hpp            # Version info
│   ├── export.hpp             # DLL export macros
│   └── detail/                # Internal headers
│       └── cpu_detect.hpp     # CPU feature detection
├── src/
│   ├── detail/                # SIMD backend implementations
│   │   ├── simd_dispatch.cpp  # Runtime dispatch
│   │   ├── simd_scalar.cpp    # SWAR scalar fallback
│   │   ├── simd_sse2.cpp      # SSE2/SSSE3 intrinsics
│   │   ├── simd_avx2.cpp      # AVX2 intrinsics
│   │   ├── simd_avx512.cpp    # AVX-512BW intrinsics
│   │   ├── simd_neon.cpp      # ARM NEON intrinsics
│   │   └── cpu_detect.cpp     # Feature detection
│   ├── highway/               # Google Highway implementations
│   │   └── simd_hwy.cpp       # Portable SIMD via Highway
│   ├── scan/                  # Scanning operations
│   ├── encode/                # Encoding operations
│   ├── url/                   # URL operations
│   ├── file/                  # File I/O
│   ├── hash/                  # Hash functions
│   ├── str/                   # String utilities
│   ├── pattern/               # Pattern scanning
│   ├── parallel/              # Parallel processing
│   ├── csv/                   # CSV parsing
│   ├── json/                  # JSON parsing
│   ├── xml/                   # XML parsing
│   ├── markdown/              # Markdown parsing
│   ├── config/                # Config parsing
│   ├── diff/                  # Diff operations
│   ├── log/                   # Log parsing
│   ├── scalar/                # Scalar implementations
│   └── c/                     # C API
│       └── c_api.cpp
├── benchmarks/                # Google Benchmark suite
│   └── bench_core.cpp
├── tests/                     # Test suite
├── examples/                  # Usage examples
└── cmake/                     # CMake modules
```

## Adding a New SIMD Backend

1. Create `src/detail/simd_<backend>.cpp`
2. Implement the same function signatures as `simd_scalar.cpp`
3. Add per-ISA CMake compile flags
4. Add the object library to `CMakeLists.txt`
5. Add the detection + dispatch case in `simd_dispatch.cpp`

## Build Options

| CMake Option | Default | Description |
|---|---|---|
| `SIMDTEXT_USE_HIGHWAY` | ON | Enable Google Highway SIMD |
| `SIMDTEXT_BENCHMARKS` | OFF | Build benchmarks |
| `SIMDTEXT_BUILD_CLI` | ON | Build CLI tool |
| `SIMDTEXT_BUILD_TESTS` | ON | Build tests |
| `SIMDTEXT_BUILD_EXAMPLES` | ON | Build examples |
| `SIMDTEXT_SANITIZER` | "" | Enable sanitizer (address, undefined) |
| `SIMDTEXT_COVERAGE` | OFF | Enable coverage |

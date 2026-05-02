# Building simdtext

## Prerequisites

- **C++23 compiler**: GCC 12+, Clang 16+, MSVC 19.34+
- **CMake 3.16+** (3.25+ recommended for presets)
- **Google Highway** (bundled in `deps/highway/` or system-installed)

## Quick Build

```bash
git clone https://github.com/yourorg/simdtext.git
cd simdtext
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

This builds the library, tests, CLI, and examples with SIMD enabled (Google Highway).

## CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `SIMDTEXT_USE_HIGHWAY` | `ON` | Enable SIMD via Google Highway |
| `SIMDTEXT_BUILD_CLI` | `ON` | Build the `simdtext` CLI tool |
| `SIMDTEXT_BENCHMARKS` | `OFF` | Build Google Benchmark targets |
| `CMAKE_BUILD_TYPE` | — | Use `Release` for performance, `Debug` for development |

### Build without SIMD (scalar-only)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSIMDTEXT_USE_HIGHWAY=OFF
cmake --build build
```

All operations fall back to scalar implementations. Useful for embedded targets or debugging.

### Build with benchmarks

```bash
# Install Google Benchmark first
pip install benchmark  # or build from source

cmake -B build -DSIMDTEXT_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bench_core
```

### Build shared library

```bash
cmake -B build -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## CMake Presets

simdtext ships presets for common configurations:

```bash
# Development (Debug, tests on)
cmake --preset dev
cmake --build --preset dev

# Release build
cmake --preset release
cmake --build --preset release

# Address sanitizer
cmake --preset asan
cmake --build --preset asan

# Coverage build
cmake --preset coverage
cmake --build --preset coverage

# No Highway (scalar-only)
cmake --preset no-hwy
cmake --build --preset no-hwy
```

## Running Tests

```bash
./build/test_simdtext
```

Or with presets:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

## Using simdtext in Your Project

### `add_subdirectory`

```cmake
# In your CMakeLists.txt
add_subdirectory(simdtext)
target_link_libraries(myapp PRIVATE simdtext)
```

### `find_package` (installed)

```bash
# Install simdtext
cd simdtext
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
sudo cmake --install build
```

```cmake
# In your project
find_package(simdtext REQUIRED)
target_link_libraries(myapp PRIVATE simdtext)
```

### vcpkg

Add to your `vcpkg.json`:

```json
{
  "dependencies": ["simdtext"]
}
```

Then:

```cmake
find_package(simdtext CONFIG REQUIRED)
target_link_libraries(myapp PRIVATE simdtext)
```

## Cross-Compilation

### ARM64 / AArch64 (NEON)

```bash
cmake -B build \
    -DCMAKE_TOOLCHAIN_FILE=/path/to/aarch64-toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

NEON is auto-detected on AArch64 targets. Highway provides portable SIMD that maps to NEON instructions.

### Windows (from Linux via MinGW)

```bash
cmake -B build \
    -DCMAKE_TOOLCHAIN_FILE=/usr/share/cmake/Modules/Platform/Windows-Clang.cmake \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### RISC-V / Other

Set `-DSIMDTEXT_USE_HIGHWAY=OFF` if Highway doesn't support the target. All operations use the scalar fallback.

## SIMD Dispatch Architecture

simdtext uses a multi-layer SIMD strategy:

1. **Google Highway** (portable SIMD) — compiled once with the default target flags. Highway handles runtime dispatch to the best available instruction set at the call site via its `HWY_DYNAMIC_DISPATCH` mechanism.

2. **Intrinsics objects** — SSE2 and AVX2 paths are compiled as separate CMake object libraries with per-file ISA flags (`-msse2`, `-mavx2`). This ensures correct code generation even when the rest of the project is compiled for a lower baseline.

3. **CPU detection** — `detail::detect_cpu()` is called once (cached via `static`) and returns a `CpuFeatures` struct indicating available instruction sets. The dispatch layer checks this to route to the best implementation.

4. **Scalar fallback** — every operation has a pure C++ scalar implementation for platforms without SIMD support.

The dispatch priority is: Highway (if enabled) → AVX2 → SSE2 → Scalar.

## Build Artifacts

| Target | Description |
|--------|-------------|
| `simdtext` | Static/shared library (C++ API) |
| `simdtext_c` | C API library |
| `simdtext` (CLI) | Command-line tool |
| `test_simdtext` | Test runner |
| `example_stats` | Example program |
| `bench_core` | Benchmark runner (if enabled) |

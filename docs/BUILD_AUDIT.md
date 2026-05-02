# Build System & CI Audit Report

**Date:** 2026-05-02  
**Scope:** CMakeLists.txt, CI workflows, project infrastructure  
**Status:** Analysis only — no code modified

---

## Summary

simdtext has a solid CMake foundation with proper package config, version files, sanitizer options, and multi-platform CI. The gaps are mostly about completeness: missing compiler versions, no `-Werror` in CI, no symbol visibility control, no pkg-config, no TSan, and no cross-compilation coverage.

---

## 1. CMakeLists.txt Findings

### ✅ What's Done Well
- Proper CMake package config with `simdtext-config.cmake.in` — correctly handles transitive Highway dependency via `find_dependency(hwy CONFIG)`
- Version file via `write_basic_package_version_file` with `SameMajorVersion` compatibility
- Sanitizer support (ASan, UBSan, combined) with proper flags
- Coverage support (`--coverage` / gcov)
- Good subproject handling (disables tests/examples/benchmarks when used via `add_subdirectory`)
- Per-architecture object libraries (SSE2, AVX2, AVX-512, NEON) with correct per-file ISA flags
- Compiler warnings: `-Wall -Wextra -Wpedantic -Wconversion` on GCC/Clang
- `CMAKE_EXPORT_COMPILE_COMMANDS ON` for tooling integration
- CMake presets provided

### 🔴 Critical Gaps

| Issue | Details |
|-------|---------|
| **No symbol visibility control** | `simdtext` and `simdtext_c` libraries have no `CXX_VISIBILITY_PRESET hidden` or `VISIBILITY_INLINES_HIDDEN`. All symbols are exported by default. Should add `set(CMAKE_CXX_VISIBILITY_PRESET hidden)` and `set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)` then mark public API with `__attribute__((visibility("default")))` or export macros. |
| **No `-Werror` in CI** | Warnings don't block CI. Should add a `SIMDTEXT_WARNINGS_AS_ERRORS` option that adds `-Werror` (or `/WX`) for CI builds. |
| **No pkg-config generation** | No `.pc` file generation via `configure_file()` + `install()`. Consumers using non-CMake build systems (Makefile, Meson) can't find simdtext easily. |

### 🟡 Moderate Gaps

| Issue | Details |
|-------|---------|
| **No TSan support** | `SIMDTEXT_SANITIZER` supports `address` and `undefined` but not `thread`. Add `thread` option for data-race detection. |
| **No static analysis in CMake** | No `clang-tidy` integration via `CMAKE_CXX_CLANG_TIDY` or `cmake_language(CXX)` property. A `.clang-tidy` file exists but isn't wired into the build. |
| **Coverage has no report target** | `SIMDTEXT_COVERAGE` adds `--coverage` flags but no `lcov`/`genhtml` target. Add a `coverage` custom target that generates HTML reports. |
| **`cmake_presets.json` incomplete** | Only 3 configure presets (release, debug, asan). Missing: ubsan, tsan, coverage, clang-tidy presets. No `testPresets`. |
| **CLI redundant Highway linking** | `simdtext-cli` links `hwy` directly even though it already links `simdtext` which has `PUBLIC` Highway linkage. Redundant — can remove the duplicate `target_link_libraries`. Same issue in `bench_core`. |
| **No `POSITION_INDEPENDENT_CODE`** | Object libraries (sse2, avx2, avx512, neon) don't set `POSITION_INDEPENDENT_CODE ON`. If simdtext is built as a shared library, these object files need `-fPIC`. |
| **No MSVC warnings** | `target_compile_options` for warnings only covers GCC/Clang. No `/W4` or `/WX` for MSVC. |

### 🟢 Minor

| Issue | Details |
|-------|---------|
| **No `CMakePresets.json` vs `cmake_presets.json`** | Using lowercase name; CMake 3.21+ supports `CMakePresets.json` (capitalized) as the standard. Both work but the capitalized name is conventional. |
| **Hardcoded Highway version** | `GIT_TAG 1.2.0` in FetchContent. Consider making this a cache variable for easy updates. |

---

## 2. CI Workflow Findings

### ✅ What's Done Well
- Multi-OS: Linux, macOS (x86_64 + ARM64), Windows
- Multiple compilers: GCC 13, GCC 14, Clang 17, Clang 18
- Sanitizer job: ASan (GCC 14) + UBSan (Clang 18)
- Build cache via `actions/cache@v4`
- Dependabot for GitHub Actions + vcpkg
- Release automation: tag → build → GitHub Release with per-platform artifacts
- `fail-fast: false` on all matrix jobs

### 🔴 Critical Gaps

| Issue | Details |
|-------|---------|
| **No GCC 12** | Oldest GCC tested is 13. GCC 12 is the minimum for many distros and has C++23 partial support. Should add. |
| **No MSVC version matrix** | Only `windows-2022` (MSVC 2022). No version variation. Consider adding `windows-2019` or explicit `VCPKG_PLATFORM_TOOLSET`. |
| **Sanitizers only with Highway OFF** | Both sanitizer builds use `-DSIMDTEXT_USE_HIGHWAY=OFF`. SIMD code paths are untested under sanitizers — exactly where bugs hide. |
| **No cross-compilation tests** | No ARM (aarch64-linux-gnu) or RISC-V cross-compilation. The NEON path is only tested on macOS ARM, not Linux ARM. |

### 🟡 Moderate Gaps

| Issue | Details |
|-------|---------|
| **No TSan in CI** | No thread sanitizer job despite parallel/ module existing. |
| **No `-Werror` CI job** | No job enforces zero warnings. Warnings can accumulate unnoticed. |
| **No benchmark regression** | No CI job runs benchmarks and compares against baselines. Performance regressions go undetected. |
| **No coverage reporting** | `SIMDTEXT_COVERAGE` option exists but no CI job uses it or uploads to Codecov/Coveralls. |
| **No clang-tidy CI step** | `.clang-tidy` exists but no CI step runs it. Static analysis is not enforced. |
| **No CMake install test** | No CI job verifies `cmake --install` works and produces a consumable package (find_package from another project). |
| **Missing Clang 16** | Gap between Clang 16 and 17. Clang 16 is still common on Ubuntu 22.04. |

### 🟢 Minor

| Issue | Details |
|-------|---------|
| **Release workflow doesn't build with sanitizers** | Release artifacts are built without any verification beyond tests. Consider at least running ASan on release builds. |
| **No vcpkg/conan package publish** | Release only creates GitHub tarballs. No automated publish to vcpkg registry or Conan Center. |
| **No source tarball in release** | Only binary artifacts. No `simdtext-vX.Y.Z.tar.gz` source archive. |

---

## 3. Project Infrastructure Findings

### ✅ What Exists
- **LICENSE** — MIT ✓
- **CONTRIBUTING.md** ✓
- **.clang-format** — Google style, 4-space indent ✓
- **.clang-tidy** — bugprone, modernize, readability, performance, clang-analyzer ✓
- **vcpkg.json** ✓
- **.gitignore** ✓
- **CHANGELOG.md** ✓

### 🟡 Missing

| Item | Priority | Notes |
|------|----------|-------|
| **.editorconfig** | Medium | Projects with `.clang-format` should also have `.editorconfig` for non-C++ files (Markdown, YAML, CMake). Ensures consistent line endings and indentation. |
| **SPDX headers in source files** | Low | No `SPDX-License-Identifier: MIT` in source files. Good practice for license compliance tooling but not critical. |
| **PR/issue templates** | Low | No `.github/PULL_REQUEST_TEMPLATE.md` or `.github/ISSUE_TEMPLATE/`. CONTRIBUTING.md exists but no structured template. |
| **CODEOWNERS** | Low | No `.github/CODEOWNERS` for automated review assignment. |

---

## 4. Recommended Priority Actions

1. **Add symbol visibility** — Set `CMAKE_CXX_VISIBILITY_PRESET hidden` and define export macros. This is the most impactful correctness issue.
2. **Add `-Werror` to CI** — Create a `SIMDTEXT_WARNINGS_AS_ERRORS` option and enable it in CI.
3. **Test sanitizers WITH Highway** — Change at least one sanitizer CI build to `SIMDTEXT_USE_HIGHWAY=ON`.
4. **Add TSan support** — Both in CMake and CI.
5. **Add pkg-config** — Generate `.pc` file for non-CMake consumers.
6. **Add `-fPIC` to object libraries** — `POSITION_INDEPENDENT_CODE ON` on all OBJECT targets.
7. **Add cross-compilation CI** — At minimum `aarch64-linux-gnu` with NEON path.
8. **Add coverage CI job** — Enable `SIMDTEXT_COVERAGE` and upload to Codecov.
9. **Add clang-tidy CI step** — Run `clang-tidy` in CI to enforce the `.clang-tidy` config.
10. **Add `.editorconfig`** — For consistent formatting across file types.

---

*End of audit. No files were modified.*

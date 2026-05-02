# Contributing to simdtext

Thanks for your interest! Here's how to contribute effectively.

## How to Build

```bash
# Clone with submodules
git clone --recurse-submodules https://github.com/tonytran-ai/simdtext.git
cd simdtext

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run tests
./build/test_simdtext
```

See [docs/BUILDING.md](docs/BUILDING.md) for cross-compilation, sanitizers, and CMake presets.

## Code Style

### Formatting

We use **clang-format** with the LLVM base style, 100-column limit, 4-space indent:

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

Run before committing:

```bash
find include src tests -name '*.hpp' -o -name '*.cpp' | xargs clang-format -i
```

### Naming

| Element | Style | Example |
|---------|-------|---------|
| Namespaces | `snake_case` | `simdtext`, `detail` |
| Functions | `snake_case` | `count_byte`, `is_ascii` |
| Classes | `PascalCase` | `LineView`, `MappedFile` |
| Member variables | `trailing_` | `input_`, `impl_` |
| Constants / macros | `UPPER_CASE` | `SIMDTEXT_API`, `SIMDTEXT_VERSION` |
| Template params | `PascalCase` | `D`, `Iterator` |

### C++ Guidelines

- **C++23** — use `std::span`, `std::expected` where available, `[[nodiscard]]`, concepts
- **No `using namespace` in headers** — always qualify
- **No heap allocation in hot paths** — use `string_view`, `span`, in-place operations
- **Every public function needs `[[nodiscard]]`** unless it returns `void`
- **Mark SIMD intrinsics with `SIMDTEXT_TARGET_*` macros** — not raw `__attribute__`
- **Include what you use** — no transitive include dependencies

## Commit Format

We use [Conventional Commits](https://www.conventionalcommits.org/):

```
type(scope): description
```

| Type | When |
|------|------|
| `feat` | New feature or API |
| `fix` | Bug fix |
| `perf` | Performance improvement |
| `refactor` | Code restructuring without behavior change |
| `test` | Adding or updating tests |
| `docs` | Documentation changes |
| `build` | Build system changes |
| `ci` | CI configuration |

Examples:

```
feat(scan): add count_byte with AVX2 intrinsics
fix(encode): handle odd-length hex input correctly
perf(utf8): use Highway for UTF-8 validation
```

## Pull Request Process

1. **Fork and branch** — create a feature branch from `main`
2. **Write code** — follow the style above, run clang-format
3. **Add tests** — every new function or behavior needs test coverage
4. **Run tests** — `./build/test_simdtext` must pass
5. **Run benchmarks** (if performance-related) — no regressions without justification
6. **Open PR** — describe what changed and why
7. **Review** — address feedback, keep force-pushes minimal

### PR Checklist

- [ ] Code compiles cleanly with `-Wall -Wextra -Wpedantic -Wconversion`
- [ ] All tests pass
- [ ] New functions have `[[nodiscard]]` and `SIMDTEXT_API`
- [ ] No allocation in hot paths (use `_to` variants or views)
- [ ] Public API is documented in header comments
- [ ] Commit messages follow conventional commits

## Adding a New Module

1. **Declare** in a new header under `include/simdtext/`
2. **Implement scalar** version in `src/scalar/scalar.cpp` or a new module source
3. **Add SIMD** version — in `src/highway/simd_hwy.cpp` for Highway, or `src/detail/simd_*.cpp` for intrinsics
4. **Wire dispatch** in `src/detail/simd_dispatch.cpp` if SIMD-accelerated
5. **Add C API** wrapper in `src/c/c_api.cpp` and `include/simdtext/c/simdtext.h`
6. **Add tests** in `tests/test_*.cpp`
7. **Add benchmark** in `benchmarks/bench_core.cpp`
8. **Document** in the header and `docs/API.md`
9. **Update** this file's module list if adding a major new area

## Reporting Issues

- Include compiler, platform, and simdtext version
- Provide minimal reproducer
- For performance issues, include benchmark output

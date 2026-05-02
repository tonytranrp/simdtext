# Contributing to simdtext

Thanks for your interest! Here's how to contribute effectively.

## Code Style

### Formatting

We use clang-format with the LLVM base style, 100-column limit, 4-space indent:

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

- **C++23** — use modern features (`std::span`, `std::expected` where available, `[[nodiscard]]`, concepts)
- **No `using namespace` in headers** — always qualify
- **No heap allocation in hot paths** — use `string_view`, `span`, in-place operations
- **Every public function needs `[[nodiscard]]`** unless it returns `void`
- **Mark SIMD intrinsics functions with `SIMDTEXT_TARGET_*` macros** — not raw `__attribute__`
- **Include what you use** — no transitive include dependencies

## Commit Format

We use [Conventional Commits](https://www.conventionalcommits.org/):

```
type(scope): description

[optional body]
```

Types:

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
docs(api): add DecodeResult documentation
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

## Testing Requirements

### Minimum Coverage

Every new public function must have:

1. **Happy path** — correct output for valid input
2. **Edge cases** — empty input, single byte, maximum alignment boundary
3. **Error cases** — invalid input returns appropriate error (for decode operations)

### Test Framework

We use a lightweight custom framework in `tests/test_framework.hpp`:

```cpp
#include "test_framework.hpp"

TEST(my_feature) {
    ASSERT_EQ(simdtext::count_byte("aaa", 'a'), 3u);
    ASSERT(simdtext::is_ascii("hello"));
}
```

### Running Tests

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/test_simdtext
```

### Property-Based Testing

For encoding operations, test roundtrip properties:

```cpp
TEST(hex_roundtrip) {
    auto bytes = random_bytes(100);
    auto hex = simdtext::hex_encode(bytes);
    auto decoded = simdtext::hex_decode(hex);
    ASSERT_EQ(bytes, decoded);
}
```

## Adding a New Function

1. **Declare** in the appropriate header under `include/simdtext/`
2. **Implement scalar** version in `src/scalar/scalar.cpp` or the module-specific source
3. **Add SIMD** version if applicable — in `src/highway/simd_hwy.cpp` for Highway, or `src/detail/simd_*.cpp` for intrinsics
4. **Wire dispatch** in `src/detail/simd_dispatch.cpp` if SIMD-accelerated
5. **Add C API** wrapper in `src/c/c_api.cpp` and `include/simdtext/c/simdtext.h`
6. **Add tests** in `tests/test_*.cpp`
7. **Add benchmark** in `benchmarks/bench_core.cpp`
8. **Document** in the header and `docs/API.md`

## Reporting Issues

- Include compiler, platform, and simdtext version
- Provide minimal reproducer
- For performance issues, include benchmark output

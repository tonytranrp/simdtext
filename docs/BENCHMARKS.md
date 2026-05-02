# Benchmarks

## Running Benchmarks

```bash
cmake -B build -DSIMDTEXT_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bench_core
```

Google Benchmark must be installed (`pip install benchmark` or build from source).

## Benchmark Suite

`bench_core.cpp` covers all major operations. Each benchmark runs at four buffer sizes: 1 KB, 64 KB, 1 MB, and 16 MB (some at fewer sizes).

| Benchmark | Operation | Sizes |
|-----------|-----------|-------|
| `BM_CountNewlines` | `count_newlines` | 1K, 64K, 1M, 16M |
| `BM_IsASCII` | `is_ascii` | 1K, 64K, 1M, 16M |
| `BM_LowercaseASCII` | `lowercase_ascii_inplace` | 1K, 64K, 1M, 16M |
| `BM_UppercaseASCII` | `uppercase_ascii_inplace` | 1K, 64K, 1M, 16M |
| `BM_FindByte` | `find_byte` | 1K, 64K, 1M, 16M |
| `BM_LinesIter` | `lines()` iteration | 1K, 64K, 1M, 16M |
| `BM_HexEncode` | `hex_encode` | 1K, 64K, 1M, 16M |
| `BM_Base64Encode` | `base64_encode` | 1K, 64K, 1M, 16M |
| `BM_URLDecode` | `url_decode` | 1K, 64K, 1M |
| `BM_ValidUTF8` | `valid_utf8` | 1K, 64K, 1M, 16M |

Benchmarks report throughput in bytes/second via `SetBytesProcessed()`.

## Filtering

Run a specific benchmark:

```bash
./build/bench_core --benchmark_filter=BM_CountNewlines
```

Run only large sizes:

```bash
./build/bench_core --benchmark_filter=BM_IsASCII/1<<24
```

## Output Formats

```bash
# Console (default)
./build/bench_core

# JSON for plotting
./build/bench_core --benchmark_format=json --benchmark_out=results.json

# CSV
./build/bench_core --benchmark_format=csv --benchmark_out=results.csv
```

## Comparison with Other Libraries

### What to compare against

| Library | Operations | Notes |
|---------|------------|-------|
| [simdutf](https://github.com/simdutf/simdutf) | UTF-8 validation, ASCII check, case conversion | Industry-standard SIMD text library |
| [fast_float](https://github.com/fastfloat/fast_float) | Number parsing | Not directly comparable but same design philosophy |
| [ada](https://github.com/ada-url/ada) | URL parsing | WHATWG URL standard parser |

### How to add a comparison benchmark

1. Add the library as a dependency in CMakeLists.txt
2. Create a new benchmark file (e.g., `benchmarks/bench_simdutf.cpp`)
3. Use the same data generation and sizing as `bench_core.cpp`
4. Build with `-DSIMDTEXT_BENCHMARKS=ON`

Example comparing `count_newlines` with a scalar loop:

```cpp
static void BM_ScalarCountNewlines(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_data(size);

    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        size_t count = 0;
        for (char c : data)
            if (c == '\n') ++count;
        benchmark::DoNotOptimize(count);
    }
    state.SetBytesProcessed(state.iterations() * size);
}
BENCHMARK(BM_ScalarCountNewlines)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24);
```

## Adding New Benchmarks

1. Add your benchmark function to `benchmarks/bench_core.cpp` (or create a new file)
2. If a new file, add it to CMakeLists.txt under the `SIMDTEXT_BENCHMARKS` section
3. Use `make_data()` or `make_uppercase()` for test data, or create your own generator
4. Register with `BENCHMARK(func)->Arg(...)` for each size
5. Always call `state.SetBytesProcessed()` for throughput reporting

## Interpreting Results

- **Bytes/second** is the primary metric — compare across SIMD levels and against scalar
- Small buffers (1K) may not show SIMD benefit due to overhead
- Large buffers (16M) show maximum throughput — limited by memory bandwidth
- `BM_LowercaseASCII` and `BM_UppercaseASCII` include a string copy per iteration (the in-place API modifies the buffer), so their throughput includes the copy cost

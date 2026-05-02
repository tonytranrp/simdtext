# simdtext Benchmark Results

## Environment

| Field | Value |
|-------|-------|
| **CPU** | _(fill in: e.g. AMD Ryzen 9 7950X)_ |
| **Compiler** | _(fill in: e.g. GCC 13.2.0)_ |
| **Compile flags** | _(fill in: e.g. -O2 -march=native)_ |
| **OS** | _(fill in)_ |
| **Date** | _(fill in)_ |
| **simdtext version** | 0.1.0 |
| **simdutf version** | _(if available)_ |

## How to Run

```bash
cmake -B build -DSIMDTEXT_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/bench_core --benchmark_format=console
```

For JSON output (easier to parse):

```bash
./build/bench_core --benchmark_format=json > results.json
```

## Count Newlines

| Input Size | simdtext (ns) | STL (ns) | Scalar (ns) | simdtext GB/s | STL GB/s | Speedup vs STL |
|-----------|--------------|----------|-------------|--------------|----------|---------------|
| 1 KB | | | | | | |
| 64 KB | | | | | | |
| 1 MB | | | | | | |
| 16 MB | | | | | | |
| 256 MB | | | | | | |

## Is ASCII

| Input Size | simdtext (ns) | STL (ns) | simdtext GB/s | STL GB/s | Speedup vs STL |
|-----------|--------------|----------|--------------|----------|---------------|
| 1 KB | | | | | |
| 64 KB | | | | | |
| 1 MB | | | | | |
| 16 MB | | | | | |
| 256 MB | | | | | |

## Lowercase ASCII (In-place)

| Input Size | simdtext (ns) | STL (ns) | Scalar Bitwise (ns) | simdtext GB/s | STL GB/s | Speedup vs STL |
|-----------|--------------|----------|--------------------|--------------|----------|---------------|
| 1 KB | | | | | | |
| 64 KB | | | | | | |
| 1 MB | | | | | | |
| 16 MB | | | | | | |
| 256 MB | | | | | | |

## Lines Iteration

| Input Size | simdtext (ns) | getline (ns) | simdtext GB/s | getline GB/s | Speedup |
|-----------|--------------|-------------|--------------|-------------|---------|
| 1 KB | | | | | |
| 64 KB | | | | | |
| 1 MB | | | | | |
| 16 MB | | | | | |

## Hex Encode

| Input Size | simdtext (ns) | Scalar sprintf (ns) | simdtext GB/s | Scalar GB/s | Speedup |
|-----------|--------------|--------------------|--------------|------------|---------|
| 1 KB | | | | | |
| 64 KB | | | | | |
| 1 MB | | | | | |
| 16 MB | | | | | |

## Base64 Encode

| Input Size | simdtext (ns) | Scalar (ns) | simdtext GB/s | Scalar GB/s | Speedup |
|-----------|--------------|------------|--------------|------------|---------|
| 1 KB | | | | | |
| 64 KB | | | | | |
| 1 MB | | | | | |
| 16 MB | | | | | |

## URL Decode

| Input Size | simdtext (ns) | Scalar (ns) | simdtext GB/s | Scalar GB/s | Speedup |
|-----------|--------------|------------|--------------|------------|---------|
| 1 KB | | | | | |
| 64 KB | | | | | |
| 1 MB | | | | | |
| 16 MB | | | | | |

## UTF-8 Validation

| Input Size | simdtext (ns) | simdutf (ns) | simdtext GB/s | simdutf GB/s | Speedup |
|-----------|--------------|-------------|--------------|-------------|---------|
| 1 KB | | | | | |
| 64 KB | | | | | |
| 1 MB | | | | | |
| 16 MB | | | | | |
| 256 MB | | | | | |

## Notes

- GB/s = bytes processed per second (input size / elapsed time)
- Speedup = baseline time / simdtext time
- All benchmarks use `benchmark::DoNotOptimize` to prevent dead-code elimination
- URL decode input is 3× larger than decoded output (%XX encoding)
- Lines iteration compares zero-copy view (simdtext) vs heap-allocated strings (getline)

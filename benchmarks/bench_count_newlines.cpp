#include <simdtext/simdtext.hpp>
#include <benchmark/benchmark.h>
#include <string>
#include <random>

static void BM_CountNewlines_Scalar(benchmark::State& state) {
    const size_t size = state.range(0);
    std::string data(size, 'x');
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 99);
    for (size_t i = 0; i < size; i++) {
        if (dist(rng) < 2) data[i] = '\n'; // ~2% newlines
    }

    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        size_t count = simdtext::count_newlines(data);
        benchmark::DoNotOptimize(count);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_LowercaseASCII_Scalar(benchmark::State& state) {
    const size_t size = state.range(0);
    std::string data(size, 'A');

    for (auto _ : state) {
        std::string copy = data;
        simdtext::lowercase_ascii_inplace(copy);
        benchmark::DoNotOptimize(copy.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_IsASCII_Scalar(benchmark::State& state) {
    const size_t size = state.range(0);
    std::string data(size, 'a');

    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        bool result = simdtext::is_ascii(data);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

BENCHMARK(BM_CountNewlines_Scalar)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24);
BENCHMARK(BM_LowercaseASCII_Scalar)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24);
BENCHMARK(BM_IsASCII_Scalar)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24);

BENCHMARK_MAIN();

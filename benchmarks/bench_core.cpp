#include <simdtext/simdtext.hpp>
#include <benchmark/benchmark.h>
#include <string>
#include <random>

// ── Data generation ────────────────────────────────────────

static std::string make_data(size_t size, double newline_pct = 0.02) {
    std::string data(size, 'x');
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 99);
    for (size_t i = 0; i < size; i++) {
        if (dist(rng) < static_cast<int>(newline_pct * 100)) data[i] = '\n';
    }
    return data;
}

static std::string make_uppercase(size_t size) {
    std::string data(size, 'A');
    for (size_t i = 0; i < size; i++) data[i] = 'A' + (i % 26);
    return data;
}

// ── Count byte ─────────────────────────────────────────────

static void BM_CountNewlines(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_data(size);

    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        size_t count = simdtext::count_newlines(data);
        benchmark::DoNotOptimize(count);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

// ── Is ASCII ───────────────────────────────────────────────

static void BM_IsASCII(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_data(size);

    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        bool result = simdtext::is_ascii(data);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

// ── Lowercase ASCII ────────────────────────────────────────

static void BM_LowercaseASCII(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_uppercase(size);

    for (auto _ : state) {
        std::string copy = data;
        simdtext::lowercase_ascii_inplace(copy);
        benchmark::DoNotOptimize(copy.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

// ── Uppercase ASCII ────────────────────────────────────────

static void BM_UppercaseASCII(benchmark::State& state) {
    const size_t size = state.range(0);
    std::string data(size, 'a');
    for (size_t i = 0; i < size; i++) data[i] = 'a' + (i % 26);

    for (auto _ : state) {
        std::string copy = data;
        simdtext::uppercase_ascii_inplace(copy);
        benchmark::DoNotOptimize(copy.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

// ── Find byte ──────────────────────────────────────────────

static void BM_FindByte(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_data(size);

    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        const char* result = simdtext::find_byte(data.data(), data.data() + data.size(), '\n');
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

// ── Lines iteration ────────────────────────────────────────

static void BM_LinesIter(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_data(size);

    size_t count = 0;
    for (auto _ : state) {
        count = 0;
        for (auto line : simdtext::lines(data)) {
            (void)line;
            count++;
        }
        benchmark::DoNotOptimize(count);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

// ── Hex encode ─────────────────────────────────────────────

static void BM_HexEncode(benchmark::State& state) {
    const size_t size = state.range(0);
    std::string data(size, 'A');

    for (auto _ : state) {
        auto result = simdtext::hex_encode(
            std::span<const std::byte>(reinterpret_cast<const std::byte*>(data.data()), data.size()));
        benchmark::DoNotOptimize(result.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

// ── Base64 encode ──────────────────────────────────────────

static void BM_Base64Encode(benchmark::State& state) {
    const size_t size = state.range(0);
    std::string data(size, 'A');

    for (auto _ : state) {
        auto result = simdtext::base64_encode(
            std::span<const std::byte>(reinterpret_cast<const std::byte*>(data.data()), data.size()));
        benchmark::DoNotOptimize(result.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

// ── URL decode ─────────────────────────────────────────────

static void BM_URLDecode(benchmark::State& state) {
    const size_t size = state.range(0);
    std::string data;
    data.reserve(size * 3);
    for (size_t i = 0; i < size; i++) {
        data += "%20";
    }

    for (auto _ : state) {
        auto result = simdtext::url_decode(data);
        benchmark::DoNotOptimize(result.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

// ── UTF-8 validation ───────────────────────────────────────

static void BM_ValidUTF8(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_data(size);

    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        bool result = simdtext::valid_utf8(data);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

// ── Register benchmarks ────────────────────────────────────

BENCHMARK(BM_CountNewlines)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24);
BENCHMARK(BM_IsASCII)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24);
BENCHMARK(BM_LowercaseASCII)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24);
BENCHMARK(BM_UppercaseASCII)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24);
BENCHMARK(BM_FindByte)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24);
BENCHMARK(BM_LinesIter)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24);
BENCHMARK(BM_HexEncode)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24);
BENCHMARK(BM_Base64Encode)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24);
BENCHMARK(BM_URLDecode)->Arg(1024)->Arg(65536)->Arg(1<<20);
BENCHMARK(BM_ValidUTF8)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24);

BENCHMARK_MAIN();

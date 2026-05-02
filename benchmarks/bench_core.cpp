#include <simdtext/simdtext.hpp>
#include <benchmark/benchmark.h>
#include <string>
#include <string_view>
#include <span>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <random>
#include <vector>

#ifdef SIMDTEXT_HAVE_SIMDUTF
#include <simdutf.h>
#endif

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

static std::string make_url_encoded(size_t size) {
    std::string data;
    data.reserve(size * 3);
    for (size_t i = 0; i < size; i++) {
        data += "%20";
    }
    return data;
}

// Common input sizes
static const auto kSizes = {1024, 65536, 1 << 20, 1 << 24, 1 << 28};
static const auto kSizesNoHuge = {1024, 65536, 1 << 20, 1 << 24};

// ═══════════════════════════════════════════════════════════
//  COUNT NEWLINES
// ═══════════════════════════════════════════════════════════

static void BM_CountNewlines_SimdText(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_data(size);
    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        size_t count = simdtext::count_newlines(data);
        benchmark::DoNotOptimize(count);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_CountNewlines_STL(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_data(size);
    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        size_t count = std::count(data.begin(), data.end(), '\n');
        benchmark::DoNotOptimize(count);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_CountNewlines_ScalarLoop(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_data(size);
    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        size_t count = 0;
        const char* p = data.data();
        const char* end = p + data.size();
        while (p < end) { count += (*p == '\n'); ++p; }
        benchmark::DoNotOptimize(count);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

BENCHMARK(BM_CountNewlines_SimdText)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24)->Arg(1<<28);
BENCHMARK(BM_CountNewlines_STL)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24)->Arg(1<<28);
BENCHMARK(BM_CountNewlines_ScalarLoop)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24)->Arg(1<<28);

// ═══════════════════════════════════════════════════════════
//  IS ASCII
// ═══════════════════════════════════════════════════════════

static void BM_IsASCII_SimdText(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_data(size);
    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        bool result = simdtext::is_ascii(data);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_IsASCII_STL(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_data(size);
    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        bool result = std::all_of(data.begin(), data.end(),
            [](unsigned char c) { return c < 128; });
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

BENCHMARK(BM_IsASCII_SimdText)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24)->Arg(1<<28);
BENCHMARK(BM_IsASCII_STL)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24)->Arg(1<<28);

// ═══════════════════════════════════════════════════════════
//  LOWERCASE ASCII
// ═══════════════════════════════════════════════════════════

static void BM_Lowercase_SimdText(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_uppercase(size);
    for (auto _ : state) {
        std::string copy = data;
        simdtext::lowercase_ascii_inplace(copy);
        benchmark::DoNotOptimize(copy.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_Lowercase_STL(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_uppercase(size);
    for (auto _ : state) {
        std::string copy = data;
        std::transform(copy.begin(), copy.end(), copy.begin(),
            [](unsigned char c) { return std::tolower(c); });
        benchmark::DoNotOptimize(copy.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_Lowercase_ScalarBitwise(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_uppercase(size);
    for (auto _ : state) {
        std::string copy = data;
        for (auto& c : copy) { c |= 0x20; }
        benchmark::DoNotOptimize(copy.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

BENCHMARK(BM_Lowercase_SimdText)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24)->Arg(1<<28);
BENCHMARK(BM_Lowercase_STL)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24)->Arg(1<<28);
BENCHMARK(BM_Lowercase_ScalarBitwise)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24)->Arg(1<<28);

// ═══════════════════════════════════════════════════════════
//  LINES ITERATION
// ═══════════════════════════════════════════════════════════

static void BM_Lines_SimdText(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_data(size);
    for (auto _ : state) {
        size_t count = 0;
        for (auto line : simdtext::lines(data)) { (void)line; count++; }
        benchmark::DoNotOptimize(count);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_Lines_Getline(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_data(size);
    for (auto _ : state) {
        std::istringstream iss(data);
        std::string line;
        size_t count = 0;
        while (std::getline(iss, line)) { count++; }
        benchmark::DoNotOptimize(count);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

BENCHMARK(BM_Lines_SimdText)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24);
BENCHMARK(BM_Lines_Getline)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24);

// ═══════════════════════════════════════════════════════════
//  HEX ENCODE
// ═══════════════════════════════════════════════════════════

static const char hex_chars[] = "0123456789abcdef";

static std::string hex_encode_scalar(std::span<const std::byte> input) {
    std::string out;
    out.reserve(input.size() * 2);
    for (auto b : input) {
        unsigned char byte = static_cast<unsigned char>(b);
        char buf[4];
        std::snprintf(buf, sizeof(buf), "%02x", byte);
        out.append(buf, 2);
    }
    return out;
}

static void BM_HexEncode_SimdText(benchmark::State& state) {
    const size_t size = state.range(0);
    std::string data(size, 'A');
    auto span = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data.data()), data.size());
    for (auto _ : state) {
        auto result = simdtext::hex_encode(span);
        benchmark::DoNotOptimize(result.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_HexEncode_ScalarSprintf(benchmark::State& state) {
    const size_t size = state.range(0);
    std::string data(size, 'A');
    auto span = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data.data()), data.size());
    for (auto _ : state) {
        auto result = hex_encode_scalar(span);
        benchmark::DoNotOptimize(result.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

BENCHMARK(BM_HexEncode_SimdText)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24);
BENCHMARK(BM_HexEncode_ScalarSprintf)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24);

// ═══════════════════════════════════════════════════════════
//  BASE64 ENCODE
// ═══════════════════════════════════════════════════════════

static std::string base64_encode_scalar(std::span<const std::byte> input) {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((input.size() + 2) / 3 * 4);
    size_t i = 0;
    for (; i + 2 < input.size(); i += 3) {
        uint32_t n = (static_cast<uint32_t>(static_cast<unsigned char>(input[i])) << 16) |
                     (static_cast<uint32_t>(static_cast<unsigned char>(input[i+1])) << 8) |
                      static_cast<uint32_t>(static_cast<unsigned char>(input[i+2]));
        out += table[(n >> 18) & 0x3F];
        out += table[(n >> 12) & 0x3F];
        out += table[(n >> 6) & 0x3F];
        out += table[n & 0x3F];
    }
    if (i < input.size()) {
        uint32_t n = static_cast<uint32_t>(static_cast<unsigned char>(input[i])) << 16;
        if (i + 1 < input.size()) n |= static_cast<uint32_t>(static_cast<unsigned char>(input[i+1])) << 8;
        out += table[(n >> 18) & 0x3F];
        out += table[(n >> 12) & 0x3F];
        out += (i + 1 < input.size()) ? table[(n >> 6) & 0x3F] : '=';
        out += '=';
    }
    return out;
}

static void BM_Base64Encode_SimdText(benchmark::State& state) {
    const size_t size = state.range(0);
    std::string data(size, 'A');
    auto span = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data.data()), data.size());
    for (auto _ : state) {
        auto result = simdtext::base64_encode(span);
        benchmark::DoNotOptimize(result.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_Base64Encode_Scalar(benchmark::State& state) {
    const size_t size = state.range(0);
    std::string data(size, 'A');
    auto span = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data.data()), data.size());
    for (auto _ : state) {
        auto result = base64_encode_scalar(span);
        benchmark::DoNotOptimize(result.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

BENCHMARK(BM_Base64Encode_SimdText)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24);
BENCHMARK(BM_Base64Encode_Scalar)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24);

// ═══════════════════════════════════════════════════════════
//  URL DECODE
// ═══════════════════════════════════════════════════════════

static std::string url_decode_scalar(std::string_view input) {
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size(); i++) {
        if (input[i] == '%' && i + 2 < input.size()) {
            char hex[3] = {input[i+1], input[i+2], '\0'};
            out += static_cast<char>(std::strtoul(hex, nullptr, 16));
            i += 2;
        } else if (input[i] == '+') {
            out += ' ';
        } else {
            out += input[i];
        }
    }
    return out;
}

static void BM_URLDecode_SimdText(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_url_encoded(size);
    for (auto _ : state) {
        auto result = simdtext::url_decode(data);
        benchmark::DoNotOptimize(result.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_URLDecode_Scalar(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_url_encoded(size);
    for (auto _ : state) {
        auto result = url_decode_scalar(data);
        benchmark::DoNotOptimize(result.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

BENCHMARK(BM_URLDecode_SimdText)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24);
BENCHMARK(BM_URLDecode_Scalar)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24);

// ═══════════════════════════════════════════════════════════
//  UTF-8 VALIDATION
// ═══════════════════════════════════════════════════════════

static void BM_ValidUTF8_SimdText(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_data(size);
    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        bool result = simdtext::valid_utf8(data);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

#ifdef SIMDTEXT_HAVE_SIMDUTF
static void BM_ValidUTF8_SimdUtf(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_data(size);
    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        bool result = simdutf::validate_utf8(data);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * size);
}
#endif

BENCHMARK(BM_ValidUTF8_SimdText)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24)->Arg(1<<28);
#ifdef SIMDTEXT_HAVE_SIMDUTF
BENCHMARK(BM_ValidUTF8_SimdUtf)->Arg(1024)->Arg(65536)->Arg(1<<20)->Arg(1<<24)->Arg(1<<28);
#endif

BENCHMARK_MAIN();

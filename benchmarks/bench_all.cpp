#include <simdtext/simdtext.hpp>
#include <simdtext/pattern.hpp>
#include <benchmark/benchmark.h>
#include <string>
#include <string_view>
#include <span>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <random>
#include <vector>
#include <sstream>

// ── Cache-hierarchy buffer sizes ───────────────────────────
// L1d ~32KB, L2 ~256KB–1MB, L3 ~6–30MB
static constexpr size_t kSizes[] = {
    64, 256, 1024, 4096, 16384, 65536, 262144,
    1048576, 4194304, 16777216
};
static const char* kSizeLabels[] = {
    "64B", "256B", "1KB", "4KB", "16KB", "64KB", "256KB",
    "1MB", "4MB", "16MB"
};
static constexpr size_t kNumSizes = sizeof(kSizes) / sizeof(kSizes[0]);

// ── Data generation ────────────────────────────────────────

static std::string make_text(size_t size, double newline_pct = 0.02) {
    std::string data(size, 'x');
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 99);
    for (size_t i = 0; i < size; i++) {
        if (dist(rng) < static_cast<int>(newline_pct * 100)) data[i] = '\n';
    }
    return data;
}

static std::string make_uppercase_alpha(size_t size) {
    std::string data(size, 'A');
    for (size_t i = 0; i < size; i++) data[i] = 'A' + (i % 26);
    return data;
}

static std::string make_url_encoded(size_t num_tokens) {
    std::string data;
    data.reserve(num_tokens * 3);
    for (size_t i = 0; i < num_tokens; i++) data += "%20";
    return data;
}

static std::string make_base64_decodable(size_t out_size) {
    // Generate base64 that can be decoded back
    std::string raw(out_size, 'A');
    for (size_t i = 0; i < out_size; i++) raw[i] = 'A' + (i % 26);
    return simdtext::base64_encode(
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(raw.data()), raw.size()));
}

static std::string make_utf8_valid(size_t size) {
    return make_text(size, 0.01);
}

// ── Helper: register benchmark with all sizes ──────────────

// We'll use ArgName + Args manually per benchmark since Google Benchmark
// doesn't have a super convenient way to name args programmatically.

// ═══════════════════════════════════════════════════════════
//  1. MEMORY BANDWIDTH REFERENCE — memcpy
// ═══════════════════════════════════════════════════════════

static void BM_Memcpy(benchmark::State& state) {
    const size_t size = state.range(0);
    std::string src(size, 'x');
    std::string dst(size, ' ');
    for (auto _ : state) {
        std::memcpy(dst.data(), src.data(), size);
        benchmark::DoNotOptimize(dst.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

// ═══════════════════════════════════════════════════════════
//  2. COUNT_BYTE
// ═══════════════════════════════════════════════════════════

static void BM_CountByte_SimdText(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_text(size);
    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        size_t count = simdtext::count_byte(data, '\n');
        benchmark::DoNotOptimize(count);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_CountByte_STL(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_text(size);
    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        size_t count = std::count(data.begin(), data.end(), '\n');
        benchmark::DoNotOptimize(count);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_CountByte_Scalar(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_text(size);
    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        size_t count = 0;
        const char* p = data.data();
        const char* end = p + size;
        while (p < end) { count += (*p == '\n'); ++p; }
        benchmark::DoNotOptimize(count);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

// ═══════════════════════════════════════════════════════════
//  3. COUNT_NEWLINES
// ═══════════════════════════════════════════════════════════

static void BM_CountNewlines_SimdText(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_text(size);
    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        size_t count = simdtext::count_newlines(data);
        benchmark::DoNotOptimize(count);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_CountNewlines_STL(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_text(size);
    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        size_t count = std::count(data.begin(), data.end(), '\n');
        benchmark::DoNotOptimize(count);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

// ═══════════════════════════════════════════════════════════
//  4. IS_ASCII
// ═══════════════════════════════════════════════════════════

static void BM_IsAscii_SimdText(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_text(size);
    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        bool result = simdtext::is_ascii(data);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_IsAscii_STL(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_text(size);
    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        bool result = std::all_of(data.begin(), data.end(),
            [](unsigned char c) { return c < 128; });
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_IsAscii_Scalar(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_text(size);
    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        bool result = true;
        for (size_t i = 0; i < size; i++) {
            if (static_cast<unsigned char>(data[i]) >= 128) { result = false; break; }
        }
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

// ═══════════════════════════════════════════════════════════
//  5. LOWERCASE ASCII (in-place)
// ═══════════════════════════════════════════════════════════

static void BM_Lowercase_SimdText(benchmark::State& state) {
    const size_t size = state.range(0);
    auto src = make_uppercase_alpha(size);
    for (auto _ : state) {
        std::string copy = src;
        simdtext::lowercase_ascii_inplace(copy);
        benchmark::DoNotOptimize(copy.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_Lowercase_STL(benchmark::State& state) {
    const size_t size = state.range(0);
    auto src = make_uppercase_alpha(size);
    for (auto _ : state) {
        std::string copy = src;
        std::transform(copy.begin(), copy.end(), copy.begin(),
            [](unsigned char c) { return std::tolower(c); });
        benchmark::DoNotOptimize(copy.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_Lowercase_ScalarBitwise(benchmark::State& state) {
    const size_t size = state.range(0);
    auto src = make_uppercase_alpha(size);
    for (auto _ : state) {
        std::string copy = src;
        for (auto& c : copy) { if (c >= 'A' && c <= 'Z') c |= 0x20; }
        benchmark::DoNotOptimize(copy.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

// ═══════════════════════════════════════════════════════════
//  6. UPPERCASE ASCII (in-place)
// ═══════════════════════════════════════════════════════════

static void BM_Uppercase_SimdText(benchmark::State& state) {
    const size_t size = state.range(0);
    auto src = make_text(size); // lowercase text
    for (auto _ : state) {
        std::string copy = src;
        simdtext::uppercase_ascii_inplace(copy);
        benchmark::DoNotOptimize(copy.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_Uppercase_STL(benchmark::State& state) {
    const size_t size = state.range(0);
    auto src = make_text(size);
    for (auto _ : state) {
        std::string copy = src;
        std::transform(copy.begin(), copy.end(), copy.begin(),
            [](unsigned char c) { return std::toupper(c); });
        benchmark::DoNotOptimize(copy.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_Uppercase_ScalarBitwise(benchmark::State& state) {
    const size_t size = state.range(0);
    auto src = make_text(size);
    for (auto _ : state) {
        std::string copy = src;
        for (auto& c : copy) { if (c >= 'a' && c <= 'z') c &= ~0x20; }
        benchmark::DoNotOptimize(copy.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

// ═══════════════════════════════════════════════════════════
//  7. FIND_BYTE
// ═══════════════════════════════════════════════════════════

static void BM_FindByte_SimdText(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_text(size);
    // Put a newline near the end so find has to scan
    data.back() = '\n';
    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        const char* result = simdtext::find_byte(data.data(), data.data() + size, '\n');
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_FindByte_STL(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_text(size);
    data.back() = '\n';
    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        auto it = std::find(data.begin(), data.end(), '\n');
        benchmark::DoNotOptimize(&*it);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_FindByte_Scalar(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_text(size);
    data.back() = '\n';
    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        const char* p = data.data();
        const char* end = p + size;
        while (p < end && *p != '\n') ++p;
        benchmark::DoNotOptimize(p);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

// ═══════════════════════════════════════════════════════════
//  8. VALID_UTF8
// ═══════════════════════════════════════════════════════════

static void BM_ValidUTF8_SimdText(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_utf8_valid(size);
    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        bool result = simdtext::valid_utf8(data);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_ValidUTF8_Scalar(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_utf8_valid(size);
    // Simple scalar UTF-8 validator
    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        bool valid = true;
        size_t i = 0;
        while (i < size) {
            unsigned char c = static_cast<unsigned char>(data[i]);
            int len = 1;
            if (c >= 0xF0) len = 4;
            else if (c >= 0xE0) len = 3;
            else if (c >= 0xC0) len = 2;
            else if (c >= 0x80) { valid = false; break; }
            if (i + len > size) { valid = false; break; }
            for (int j = 1; j < len; j++) {
                if ((static_cast<unsigned char>(data[i+j]) & 0xC0) != 0x80) { valid = false; break; }
            }
            if (!valid) break;
            i += len;
        }
        benchmark::DoNotOptimize(valid);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

// ═══════════════════════════════════════════════════════════
//  9. URL_DECODE
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
    auto data = make_url_encoded(size / 3); // url encoded is ~3x larger
    for (auto _ : state) {
        auto result = simdtext::url_decode(data);
        benchmark::DoNotOptimize(result.data());
    }
    state.SetBytesProcessed(state.iterations() * data.size());
}

static void BM_URLDecode_Scalar(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_url_encoded(size / 3);
    for (auto _ : state) {
        auto result = url_decode_scalar(data);
        benchmark::DoNotOptimize(result.data());
    }
    state.SetBytesProcessed(state.iterations() * data.size());
}

// ═══════════════════════════════════════════════════════════
//  10. BASE64_DECODE
// ═══════════════════════════════════════════════════════════

static void BM_Base64Decode_SimdText(benchmark::State& state) {
    const size_t size = state.range(0);
    auto encoded = make_base64_decodable(size);
    for (auto _ : state) {
        auto result = simdtext::base64_decode(encoded);
        benchmark::DoNotOptimize(result.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static std::vector<std::byte> base64_decode_scalar(std::string_view input) {
    static const int8_t dtable[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    };
    std::vector<std::byte> out;
    out.reserve(input.size() * 3 / 4);
    int val = 0, valb = -8;
    for (unsigned char c : input) {
        if (dtable[c] == -1) break;
        val = (val << 6) + dtable[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<std::byte>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

static void BM_Base64Decode_Scalar(benchmark::State& state) {
    const size_t size = state.range(0);
    auto encoded = make_base64_decodable(size);
    for (auto _ : state) {
        auto result = base64_decode_scalar(encoded);
        benchmark::DoNotOptimize(result.data());
    }
    state.SetBytesProcessed(state.iterations() * size);
}

// ═══════════════════════════════════════════════════════════
//  11. SPLIT
// ═══════════════════════════════════════════════════════════

static void BM_Split_SimdText(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_text(size);
    for (auto _ : state) {
        size_t count = 0;
        for (auto seg : simdtext::split(data, '\n')) { (void)seg; count++; }
        benchmark::DoNotOptimize(count);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_Split_ScalarFind(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_text(size);
    for (auto _ : state) {
        size_t count = 0;
        size_t pos = 0;
        while (pos < data.size()) {
            auto next = data.find('\n', pos);
            if (next == std::string::npos) { count++; break; }
            count++;
            pos = next + 1;
        }
        benchmark::DoNotOptimize(count);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

// ═══════════════════════════════════════════════════════════
//  12. LINES
// ═══════════════════════════════════════════════════════════

static void BM_Lines_SimdText(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_text(size);
    for (auto _ : state) {
        size_t count = 0;
        for (auto line : simdtext::lines(data)) { (void)line; count++; }
        benchmark::DoNotOptimize(count);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

static void BM_Lines_Getline(benchmark::State& state) {
    const size_t size = state.range(0);
    auto data = make_text(size);
    for (auto _ : state) {
        std::istringstream iss(data);
        std::string line;
        size_t count = 0;
        while (std::getline(iss, line)) { count++; }
        benchmark::DoNotOptimize(count);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

// ═══════════════════════════════════════════════════════════
//  13. FIND_PATTERN (with wildcards)
// ═══════════════════════════════════════════════════════════

static void BM_FindPattern_SimdText(benchmark::State& state) {
    const size_t size = state.range(0);
    std::string data(size, '\x00');
    std::mt19937 rng(123);
    for (auto& c : data) c = static_cast<char>(rng() & 0xFF);
    // Embed pattern at end
    data[data.size()-8] = '\x48';
    data[data.size()-7] = '\x8B';
    data[data.size()-4] = '\x05';
    auto pat = simdtext::BytePattern::parse("48 8B ? ? 05");
    if (!pat) { state.SkipWithError("pattern parse failed"); return; }
    for (auto _ : state) {
        benchmark::DoNotOptimize(data.data());
        auto result = simdtext::find_pattern(
            reinterpret_cast<const uint8_t*>(data.data()), data.size(), *pat);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * size);
}

// ═══════════════════════════════════════════════════════════
//  Register all benchmarks with all sizes
// ═══════════════════════════════════════════════════════════

// Helper macro to register with all cache-hierarchy sizes
#define BENCH_ALL(fn) \
    BENCHMARK(fn)->Args({64})->Args({256})->Args({1024})->Args({4096}) \
                  ->Args({16384})->Args({65536})->Args({262144}) \
                  ->Args({1048576})->Args({4194304})->Args({16777216})

// Bandwidth reference
BENCH_ALL(BM_Memcpy);

// count_byte
BENCH_ALL(BM_CountByte_SimdText);
BENCH_ALL(BM_CountByte_STL);
BENCH_ALL(BM_CountByte_Scalar);

// count_newlines
BENCH_ALL(BM_CountNewlines_SimdText);
BENCH_ALL(BM_CountNewlines_STL);

// is_ascii
BENCH_ALL(BM_IsAscii_SimdText);
BENCH_ALL(BM_IsAscii_STL);
BENCH_ALL(BM_IsAscii_Scalar);

// lowercase
BENCH_ALL(BM_Lowercase_SimdText);
BENCH_ALL(BM_Lowercase_STL);
BENCH_ALL(BM_Lowercase_ScalarBitwise);

// uppercase
BENCH_ALL(BM_Uppercase_SimdText);
BENCH_ALL(BM_Uppercase_STL);
BENCH_ALL(BM_Uppercase_ScalarBitwise);

// find_byte
BENCH_ALL(BM_FindByte_SimdText);
BENCH_ALL(BM_FindByte_STL);
BENCH_ALL(BM_FindByte_Scalar);

// valid_utf8
BENCH_ALL(BM_ValidUTF8_SimdText);
BENCH_ALL(BM_ValidUTF8_Scalar);

// url_decode
BENCH_ALL(BM_URLDecode_SimdText);
BENCH_ALL(BM_URLDecode_Scalar);

// base64_decode
BENCH_ALL(BM_Base64Decode_SimdText);
BENCH_ALL(BM_Base64Decode_Scalar);

// split
BENCH_ALL(BM_Split_SimdText);
BENCH_ALL(BM_Split_ScalarFind);

// lines
BENCH_ALL(BM_Lines_SimdText);
BENCH_ALL(BM_Lines_Getline);

// find_pattern
BENCH_ALL(BM_FindPattern_SimdText);

BENCHMARK_MAIN();

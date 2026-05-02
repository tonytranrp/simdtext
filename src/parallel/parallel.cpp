#include "simdtext/parallel.hpp"
#include "simdtext/scan.hpp"
#include "simdtext/ascii.hpp"
#include "simdtext/utf8.hpp"

#include <thread>
#include <vector>
#include <atomic>
#include <algorithm>

namespace simdtext {

namespace {

unsigned int effective_threads(const ParallelOptions& opts, size_t data_size) {
    unsigned int n = opts.num_threads;
    if (n == 0) n = std::max(1u, std::thread::hardware_concurrency());
    const size_t max_chunks = data_size / opts.min_chunk_size;
    if (max_chunks > 0) n = std::min(n, static_cast<unsigned int>(max_chunks));
    if (data_size < opts.min_chunk_size) n = 1;
    return std::max(1u, n);
}

} // anonymous namespace

size_t parallel_count_byte(std::string_view data, char byte, const ParallelOptions& opts) {
    const size_t size = data.size();
    const unsigned int nthreads = effective_threads(opts, size);
    if (nthreads == 1) return count_byte(std::span<const char>(data.data(), size), byte);

    const size_t chunk = size / nthreads;
    std::vector<size_t> counts(nthreads, 0);
    std::vector<std::thread> threads;
    threads.reserve(nthreads);

    for (unsigned int t = 0; t < nthreads; ++t) {
        const size_t start = t * chunk;
        const size_t end = (t == nthreads - 1) ? size : (t + 1) * chunk;
        threads.emplace_back([&counts, &data, byte, start, end, t]() {
            counts[t] = count_byte(
                std::span<const char>(data.data() + start, end - start), byte);
        });
    }

    size_t total = 0;
    for (unsigned int t = 0; t < nthreads; ++t) {
        threads[t].join();
        total += counts[t];
    }
    return total;
}

bool parallel_is_ascii(std::string_view data, const ParallelOptions& opts) {
    const size_t size = data.size();
    const unsigned int nthreads = effective_threads(opts, size);
    if (nthreads == 1) return is_ascii(std::span<const char>(data.data(), size));

    const size_t chunk = size / nthreads;
    std::atomic<bool> found_non_ascii{false};
    std::vector<std::thread> threads;
    threads.reserve(nthreads);

    for (unsigned int t = 0; t < nthreads; ++t) {
        if (found_non_ascii.load(std::memory_order_relaxed)) break;
        const size_t start = t * chunk;
        const size_t end = (t == nthreads - 1) ? size : (t + 1) * chunk;
        threads.emplace_back([&found_non_ascii, &data, start, end]() {
            if (found_non_ascii.load(std::memory_order_relaxed)) return;
            bool result = is_ascii(
                std::span<const char>(data.data() + start, end - start));
            if (!result) found_non_ascii.store(true, std::memory_order_relaxed);
        });
    }

    for (auto& th : threads) th.join();
    return !found_non_ascii.load(std::memory_order_relaxed);
}

size_t parallel_count_newlines(std::string_view data, const ParallelOptions& opts) {
    return parallel_count_byte(data, '\n', opts);
}

const char* parallel_find_byte(std::string_view data, char byte, const ParallelOptions& opts) {
    const size_t size = data.size();
    const unsigned int nthreads = effective_threads(opts, size);
    if (nthreads == 1) return find_byte(data.data(), data.data() + size, byte);

    const size_t chunk = size / nthreads;
    std::atomic<const char*> earliest{nullptr};
    std::vector<std::thread> threads;
    threads.reserve(nthreads);

    for (unsigned int t = 0; t < nthreads; ++t) {
        const size_t start = t * chunk;
        const size_t end = (t == nthreads - 1) ? size : (t + 1) * chunk;
        threads.emplace_back([&earliest, &data, byte, start, end]() {
            const char* result = find_byte(data.data() + start, data.data() + end, byte);
            if (result != data.data() + end) {
                const char* current = earliest.load(std::memory_order_relaxed);
                while (current == nullptr || result < current) {
                    if (earliest.compare_exchange_weak(current, result,
                            std::memory_order_relaxed, std::memory_order_relaxed)) {
                        break;
                    }
                }
            }
        });
    }

    for (auto& th : threads) th.join();
    const char* result = earliest.load(std::memory_order_relaxed);
    return result ? result : data.data() + size;
}

bool parallel_valid_utf8(std::string_view data, const ParallelOptions& opts) {
    const size_t size = data.size();
    const unsigned int nthreads = effective_threads(opts, size);
    if (nthreads == 1) return valid_utf8(std::span<const char>(data.data(), size));

    const size_t chunk = size / nthreads;
    std::atomic<bool> invalid{false};
    std::vector<std::thread> threads;
    threads.reserve(nthreads);

    for (unsigned int t = 0; t < nthreads; ++t) {
        if (invalid.load(std::memory_order_relaxed)) break;
        const size_t start = t * chunk;
        const size_t end = (t == nthreads - 1) ? size : (t + 1) * chunk;
        threads.emplace_back([&invalid, &data, start, end]() {
            if (invalid.load(std::memory_order_relaxed)) return;
            bool result = valid_utf8(
                std::span<const char>(data.data() + start, end - start));
            if (!result) invalid.store(true, std::memory_order_relaxed);
        });
    }

    for (auto& th : threads) th.join();
    return !invalid.load(std::memory_order_relaxed);
}

void parallel_for_each_chunk(std::string_view data,
    std::function<void(std::string_view chunk, size_t offset)> callback,
    const ParallelOptions& opts) {
    const size_t size = data.size();
    const unsigned int nthreads = effective_threads(opts, size);
    if (nthreads == 1) {
        callback(data, 0);
        return;
    }

    const size_t chunk = size / nthreads;
    std::vector<std::thread> threads;
    threads.reserve(nthreads);

    for (unsigned int t = 0; t < nthreads; ++t) {
        const size_t start = t * chunk;
        const size_t end = (t == nthreads - 1) ? size : (t + 1) * chunk;
        threads.emplace_back([&callback, &data, start, end]() {
            callback(std::string_view(data.data() + start, end - start), start);
        });
    }

    for (auto& th : threads) th.join();
}

} // namespace simdtext

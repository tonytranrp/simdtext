#include <simdtext/c/simdtext.h>

#include <cstdlib>
#include <cstring>
#include <new>

#include <simdtext/simdtext.hpp>

// ── Helpers ────────────────────────────────────────────────

static char* alloc_copy(const std::string& s) {
    if (s.empty()) {
        char* p = static_cast<char*>(std::malloc(1));
        if (!p) return nullptr;
        p[0] = '\0';
        return p;
    }
    char* p = static_cast<char*>(std::malloc(s.size() + 1));
    if (!p) return nullptr;
    std::memcpy(p, s.data(), s.size());
    p[s.size()] = '\0';
    return p;
}

// ── Opaque handle implementation ───────────────────────────

struct simdtext_file {
    simdtext::MappedFile mapped;
    simdtext::FileScanner scanner;
    bool scanner_valid;

    explicit simdtext_file(const char* path)
        : mapped(), scanner(path), scanner_valid(true) {}
};

// ── Version ────────────────────────────────────────────────

int simdtext_version_major(void) {
    return SIMDTEXT_VERSION_MAJOR;
}

int simdtext_version_minor(void) {
    return SIMDTEXT_VERSION_MINOR;
}

int simdtext_version_patch(void) {
    return SIMDTEXT_VERSION_PATCH;
}

// ── Scanning ───────────────────────────────────────────────

size_t simdtext_count_byte(const char* data, size_t len, char byte) {
    if (!data || len == 0) return 0;
    return simdtext::count_byte({data, len}, byte);
}

size_t simdtext_count_newlines(const char* data, size_t len) {
    if (!data || len == 0) return 0;
    return simdtext::count_newlines({data, len});
}

int simdtext_contains(const char* haystack, size_t haystack_len,
                       const char* needle, size_t needle_len) {
    if (!haystack || !needle) return 0;
    return simdtext::contains(std::string_view(haystack, haystack_len),
                              std::string_view(needle, needle_len)) ? 1 : 0;
}

// ── ASCII ──────────────────────────────────────────────────

int simdtext_is_ascii(const char* data, size_t len) {
    if (!data || len == 0) return 1;
    return simdtext::is_ascii({data, len}) ? 1 : 0;
}

void simdtext_lowercase_ascii(char* data, size_t len) {
    if (!data || len == 0) return;
    simdtext::lowercase_ascii_inplace({data, len});
}

void simdtext_uppercase_ascii(char* data, size_t len) {
    if (!data || len == 0) return;
    simdtext::uppercase_ascii_inplace({data, len});
}

// ── UTF-8 ──────────────────────────────────────────────────

int simdtext_valid_utf8(const char* data, size_t len) {
    if (!data || len == 0) return 1;
    return simdtext::valid_utf8({data, len}) ? 1 : 0;
}

// ── Encoding ───────────────────────────────────────────────

char* simdtext_hex_encode(const char* data, size_t len) {
    if (!data) return nullptr;
    auto result = simdtext::hex_encode(
        {reinterpret_cast<const std::byte*>(data), len});
    return alloc_copy(result);
}

char* simdtext_base64_encode(const char* data, size_t len) {
    if (!data) return nullptr;
    auto result = simdtext::base64_encode(
        {reinterpret_cast<const std::byte*>(data), len});
    return alloc_copy(result);
}

// ── URL ────────────────────────────────────────────────────

char* simdtext_url_encode(const char* data, size_t len) {
    if (!data) return nullptr;
    auto result = simdtext::url_encode(std::string_view(data, len));
    return alloc_copy(result);
}

char* simdtext_url_decode(const char* data, size_t len) {
    if (!data) return nullptr;
    auto result = simdtext::url_decode(std::string_view(data, len));
    return alloc_copy(result);
}

// ── File I/O ───────────────────────────────────────────────

simdtext_file_t simdtext_file_open(const char* path) {
    if (!path) return nullptr;
    try {
        auto* f = new simdtext_file(path);
        if (!f->scanner.is_open()) {
            delete f;
            return nullptr;
        }
        return f;
    } catch (...) {
        return nullptr;
    }
}

void simdtext_file_close(simdtext_file_t file) {
    if (file) {
        // Manually destruct since C callers can't call delete
        file->~simdtext_file();
        std::free(file);
    }
}

const char* simdtext_file_data(simdtext_file_t file) {
    if (!file) return nullptr;
    auto v = file->mapped.view();
    return v.data();
}

size_t simdtext_file_size(simdtext_file_t file) {
    if (!file) return 0;
    return file->mapped.size();
}

size_t simdtext_file_count_lines(simdtext_file_t file) {
    if (!file) return 0;
    return file->scanner.count_lines();
}

// ── Pattern scanning ──────────────────────────────────────

const uint8_t* simdtext_find_pattern(const uint8_t* data, size_t length, const char* hex_pattern) {
    if (!data || !hex_pattern || length == 0) return nullptr;
    return simdtext::find_pattern(data, length, std::string_view(hex_pattern));
}

int simdtext_byte_pattern_parse(const char* hex_pattern, uint8_t* out_bytes, uint8_t* out_masks, size_t* out_length, size_t capacity) {
    if (!hex_pattern || !out_length) return 0;
    auto pat = simdtext::BytePattern::parse(std::string_view(hex_pattern));
    if (!pat) return 0;
    *out_length = pat->size();
    if (out_bytes && out_masks) {
        size_t n = pat->size() < capacity ? pat->size() : capacity;
        for (size_t i = 0; i < n; ++i) {
            out_bytes[i] = pat->byte(i);
            out_masks[i] = pat->mask(i);
        }
    }
    return 1;
}

// ── Parallel processing ────────────────────────────────────

size_t simdtext_parallel_count_byte(const char* data, size_t len, char byte, unsigned int num_threads) {
    if (!data || len == 0) return 0;
    simdtext::ParallelOptions opts;
    opts.num_threads = num_threads;
    return simdtext::parallel_count_byte(std::string_view(data, len), byte, opts);
}

// ── Memory management ──────────────────────────────────────

void simdtext_free(void* ptr) {
    std::free(ptr);
}

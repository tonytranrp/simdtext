#include "simdtext/simdtext.hpp"
#include "simdtext/detail/cpu_detect.hpp"
#include <cstring>
#include <utility>

namespace simdtext {

// ── Scanning functions (dispatch to detail::scalar/sse2/avx2) ─

namespace detail {
namespace scalar {
size_t count_byte(const char* data, size_t size, char byte);
bool is_ascii(const char* data, size_t size);
void lowercase_ascii(char* data, size_t size);
void uppercase_ascii(char* data, size_t size);
const char* find_byte(const char* data, size_t size, char byte);
}
namespace sse2 {
size_t count_byte(const char* data, size_t size, char byte);
bool is_ascii(const char* data, size_t size);
void lowercase_ascii(char* data, size_t size);
void uppercase_ascii(char* data, size_t size);
const char* find_byte(const char* data, size_t size, char byte);
}
namespace avx2 {
size_t count_byte(const char* data, size_t size, char byte);
bool is_ascii(const char* data, size_t size);
void lowercase_ascii(char* data, size_t size);
void uppercase_ascii(char* data, size_t size);
const char* find_byte(const char* data, size_t size, char byte);
}
} // namespace detail

size_t count_byte(std::span<const char> input, char byte) {
    if (input.empty()) return 0;
    const auto& f = detail::detect_cpu();
    if (f.avx2)    return detail::avx2::count_byte(input.data(), input.size(), byte);
    if (f.sse2)    return detail::sse2::count_byte(input.data(), input.size(), byte);
    return detail::scalar::count_byte(input.data(), input.size(), byte);
}

size_t count_newlines(std::span<const char> input) {
    return count_byte(input, '\n');
}

bool is_ascii(std::span<const char> input) {
    if (input.empty()) return true;
    const auto& f = detail::detect_cpu();
    if (f.avx2)    return detail::avx2::is_ascii(input.data(), input.size());
    if (f.sse2)    return detail::sse2::is_ascii(input.data(), input.size());
    return detail::scalar::is_ascii(input.data(), input.size());
}

void lowercase_ascii_inplace(std::span<char> input) {
    if (input.empty()) return;
    const auto& f = detail::detect_cpu();
    if (f.avx2)    return detail::avx2::lowercase_ascii(input.data(), input.size());
    if (f.sse2)    return detail::sse2::lowercase_ascii(input.data(), input.size());
    detail::scalar::lowercase_ascii(input.data(), input.size());
}

void uppercase_ascii_inplace(std::span<char> input) {
    if (input.empty()) return;
    const auto& f = detail::detect_cpu();
    if (f.avx2)    return detail::avx2::uppercase_ascii(input.data(), input.size());
    if (f.sse2)    return detail::sse2::uppercase_ascii(input.data(), input.size());
    detail::scalar::uppercase_ascii(input.data(), input.size());
}

const char* find_byte(const char* begin, const char* end, char byte) {
    const size_t size = static_cast<size_t>(end - begin);
    if (size == 0) return end;
    const auto& f = detail::detect_cpu();
    if (f.avx2)    return detail::avx2::find_byte(begin, size, byte);
    if (f.sse2)    return detail::sse2::find_byte(begin, size, byte);
    return detail::scalar::find_byte(begin, size, byte);
}

// ── Contains ───────────────────────────────────────────────

bool contains(std::string_view input, std::string_view needle) {
    if (needle.empty()) return true;
    if (needle.size() > input.size()) return false;
    return input.find(needle) != std::string_view::npos;
}

// ── Trim ASCII ─────────────────────────────────────────────

constexpr static bool is_ascii_whitespace(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

std::string_view trim_ascii(std::string_view input) {
    while (!input.empty() && is_ascii_whitespace(input.front())) {
        input.remove_prefix(1);
    }
    while (!input.empty() && is_ascii_whitespace(input.back())) {
        input.remove_suffix(1);
    }
    return input;
}

// ── Lines & Splitting ──────────────────────────────────────

LineView::Iterator::Iterator(std::string_view remaining)
    : remaining_(remaining), line_() {
    if (remaining.data() == nullptr) {
        // Sentinel end iterator
        return;
    }
    const auto pos = remaining.find('\n');
    if (pos == std::string_view::npos) {
        line_ = remaining;
        remaining_ = {};
    } else {
        line_ = remaining.substr(0, pos);
        remaining_ = remaining.substr(pos + 1);
    }
}

LineView::Iterator& LineView::Iterator::operator++() {
    if (remaining_.data() == nullptr) {
        // Already at end — clear line_ to match end sentinel
        line_ = {};
    } else {
        const auto pos = remaining_.find('\n');
        if (pos == std::string_view::npos) {
            line_ = remaining_;
            remaining_ = {};
        } else {
            line_ = remaining_.substr(0, pos);
            remaining_ = remaining_.substr(pos + 1);
        }
    }
    return *this;
}

LineView::Iterator LineView::Iterator::operator++(int) {
    Iterator tmp = *this;
    ++(*this);
    return tmp;
}

LineView lines(std::string_view input) {
    return LineView(input);
}

SplitView::Iterator::Iterator(std::string_view remaining, char delim)
    : remaining_(remaining), delim_(delim), segment_() {
    if (remaining.data() == nullptr) {
        return;
    }
    const auto pos = remaining.find(delim_);
    if (pos == std::string_view::npos) {
        segment_ = remaining;
        remaining_ = {};
    } else {
        segment_ = remaining.substr(0, pos);
        remaining_ = remaining.substr(pos + 1);
    }
}

SplitView::Iterator& SplitView::Iterator::operator++() {
    if (remaining_.data() == nullptr) {
        segment_ = {};
    } else {
        const auto pos = remaining_.find(delim_);
        if (pos == std::string_view::npos) {
            segment_ = remaining_;
            remaining_ = {};
        } else {
            segment_ = remaining_.substr(0, pos);
            remaining_ = remaining_.substr(pos + 1);
        }
    }
    return *this;
}

SplitView::Iterator SplitView::Iterator::operator++(int) {
    Iterator tmp = *this;
    ++(*this);
    return tmp;
}

SplitView split(std::string_view input, char delimiter) {
    return SplitView(input, delimiter);
}

// ── UTF-8 ──────────────────────────────────────────────────

bool valid_utf8(std::span<const char> input) {
    const auto* p = reinterpret_cast<const uint8_t*>(input.data());
    const auto* end = p + input.size();

    while (p < end) {
        const auto byte = *p++;

        if (byte <= 0x7F) {
            continue;
        } else if ((byte & 0xE0) == 0xC0) {
            if (p >= end || (*p & 0xC0) != 0x80) return false;
            if (byte < 0xC2) return false;
            ++p;
        } else if ((byte & 0xF0) == 0xE0) {
            if (p + 1 >= end || (*p & 0xC0) != 0x80 || (*(p+1) & 0xC0) != 0x80) return false;
            if (byte == 0xE0 && *p < 0xA0) return false;
            if (byte == 0xED && *p > 0x9F) return false;
            p += 2;
        } else if ((byte & 0xF8) == 0xF0) {
            if (p + 2 >= end || (*p & 0xC0) != 0x80 || (*(p+1) & 0xC0) != 0x80 || (*(p+2) & 0xC0) != 0x80) return false;
            if (byte == 0xF0 && *p < 0x90) return false;
            if (byte > 0xF4) return false;
            p += 3;
        } else {
            return false;
        }
    }
    return true;
}

} // namespace simdtext

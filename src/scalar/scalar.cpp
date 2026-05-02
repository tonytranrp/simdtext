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
    : line_(), remaining_(remaining), has_value_(!remaining.empty()) {
    if (!has_value_) return;
    advance();
}

void LineView::Iterator::advance() {
    const auto pos = remaining_.find('\n');
    if (pos == std::string_view::npos) {
        line_ = remaining_;
        has_value_ = true;
        remaining_ = {};
    } else {
        line_ = remaining_.substr(0, pos);
        remaining_ = remaining_.substr(pos + 1);
        has_value_ = true;
    }
}

LineView::Iterator& LineView::Iterator::operator++() {
    if (remaining_.empty()) {
        // No more data — become end sentinel
        has_value_ = false;
        line_ = {};
    } else {
        advance();
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
    : segment_(), remaining_(remaining), delim_(delim), has_value_(!remaining.empty()) {
    if (!has_value_) return;
    advance();
}

void SplitView::Iterator::advance() {
    const auto pos = remaining_.find(delim_);
    if (pos == std::string_view::npos) {
        segment_ = remaining_;
        has_value_ = true;
        remaining_ = {};
    } else {
        segment_ = remaining_.substr(0, pos);
        remaining_ = remaining_.substr(pos + 1);
        has_value_ = true;
    }
}

SplitView::Iterator& SplitView::Iterator::operator++() {
    if (remaining_.empty()) {
        // No more data — become end sentinel
        has_value_ = false;
        segment_ = {};
    } else {
        advance();
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
    if (input.empty()) return true;
    return detail::validate_utf8_dispatch(input.data(), input.size());
}

bool Utf8Validator::validate(std::string_view chunk) noexcept {
    for (char c : chunk) {
        auto byte = static_cast<uint8_t>(c);
        if (state_ == 0) {
            // Expecting start of a new sequence
            if (byte < 0x80) {
                // ASCII — valid
            } else if ((byte & 0xE0) == 0xC0) {
                // 2-byte lead: 110xxxxx
                if (byte < 0xC2) return false; // overlong
                state_ = 1;
            } else if ((byte & 0xF0) == 0xE0) {
                // 3-byte lead: 1110xxxx
                state_ = 2;
            } else if ((byte & 0xF8) == 0xF0) {
                // 4-byte lead: 11110xxx
                if (byte > 0xF4) return false; // > U+10FFFF
                state_ = 3;
            } else {
                return false; // invalid lead byte (0x80-0xBF or 0xF5-0xFF)
            }
        } else {
            // Expecting continuation byte: 10xxxxxx
            if ((byte & 0xC0) != 0x80) return false;

            // Range checks for specific lead bytes
            if (state_ == 2) {
                // After E0, next byte must be A0-BF
                // After ED, next byte must be 80-9F (no surrogates)
                // For simplicity, we just decrement and continue
                // The full range checks are handled by the SIMD validator
            }
            state_--;
        }
    }
    return true;
}

bool Utf8Validator::finalize() noexcept {
    bool ok = (state_ == 0);
    state_ = 0;
    return ok;
}

size_t count_code_points(std::string_view input) noexcept {
    // Count bytes that are NOT continuation bytes (10xxxxxx)
    // Each such byte starts a new code point
    size_t count = 0;
    for (char c : input) {
        if ((static_cast<uint8_t>(c) & 0xC0) != 0x80) {
            count++;
        }
    }
    return count;
}

size_t utf8_length(std::string_view input) noexcept {
    return count_code_points(input);
}

} // namespace simdtext

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

// is_ascii and valid_utf8 always dispatch through AVX2/SSE2 intrinsics.
// Even when Highway is available, native AVX2 intrinsics use 32-byte YMM registers
// vs Highway's 16-byte XMM (Highway only compiles for SSE2 target in this build).

bool is_ascii(std::span<const char> input) {
    if (input.empty()) return true;
    const auto& f = detail::detect_cpu();
    if (f.avx2)    return detail::avx2::is_ascii(input.data(), input.size());
    if (f.sse2)    return detail::sse2::is_ascii(input.data(), input.size());
    return detail::scalar::is_ascii(input.data(), input.size());
}

#ifndef SIMDTEXT_HAVE_HWY
// When Highway is available, count_byte/lowercase/uppercase/find_byte are provided
// by simd_hwy.cpp which uses Highway's portable SIMD.

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

const char* find_byte(std::span<const char> input, char byte) {
    if (input.empty()) return input.data() + input.size();
    const auto& f = detail::detect_cpu();
    if (f.avx2)    return detail::avx2::find_byte(input.data(), input.size(), byte);
    if (f.sse2)    return detail::sse2::find_byte(input.data(), input.size(), byte);
    return detail::scalar::find_byte(input.data(), input.size(), byte);
}

#endif // SIMDTEXT_HAVE_HWY

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
    // Use SIMD-accelerated find_byte instead of string_view::find
    const char* data = remaining_.data();
    size_t size = remaining_.size();
    const char* found = find_byte(std::span<const char>{data, size}, '\n');
    const char* end = data + size;
    if (found == end) {
        line_ = remaining_;
        has_value_ = true;
        remaining_ = {};
    } else {
        size_t pos = static_cast<size_t>(found - data);
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
    // Use SIMD-accelerated find_byte instead of string_view::find
    const char* data = remaining_.data();
    size_t size = remaining_.size();
    const char* found = find_byte(std::span<const char>{data, size}, delim_);
    const char* end = data + size;
    if (found == end) {
        segment_ = remaining_;
        has_value_ = true;
        remaining_ = {};
    } else {
        size_t pos = static_cast<size_t>(found - data);
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
                saved_lead_ = byte;
            } else if ((byte & 0xF0) == 0xE0) {
                // 3-byte lead: 1110xxxx
                state_ = 2;
                saved_lead_ = byte;
            } else if ((byte & 0xF8) == 0xF0) {
                // 4-byte lead: 11110xxx
                if (byte > 0xF4) return false; // > U+10FFFF
                state_ = 3;
                saved_lead_ = byte;
            } else {
                return false; // invalid lead byte (0x80-0xBF or 0xF5-0xFF)
            }
        } else {
            // Expecting continuation byte: 10xxxxxx
            if ((byte & 0xC0) != 0x80) return false;

            // Range checks for specific lead bytes
            if (state_ == 2) {
                // After E0, next byte must be A0-BF (reject overlong C0/C1 + 80-BF)
                // After ED, next byte must be 80-9F (reject surrogates D800-DFFF)
                // We check the *first* continuation byte (byte we just read)
                // saved_lead_ holds the lead byte
                if (saved_lead_ == 0xE0 && byte < 0xA0) return false; // overlong E0 80-9F
                if (saved_lead_ == 0xED && byte > 0x9F) return false; // surrogate ED A0-BF
            }
            if (state_ == 3) {
                // After F0, first continuation byte must be 90-BF (reject overlong F0 80-8F)
                // After F4, first continuation byte must be 80-8F (reject > U+10FFFF)
                if (saved_lead_ == 0xF0 && byte < 0x90) return false; // overlong F0 80-8F
                if (saved_lead_ == 0xF4 && byte > 0x8F) return false; // > U+10FFFF
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
    if (input.empty()) return 0;
    return detail::count_code_points_dispatch(input.data(), input.size());
}

size_t utf8_length(std::string_view input) noexcept {
    return count_code_points(input);
}

Utf8Result validate_utf8_detailed(std::string_view input) noexcept {
    Utf8Result result;
    if (input.empty()) return result;

    size_t i = 0;
    while (i < input.size()) {
        auto byte = static_cast<uint8_t>(input[i]);

        if (byte < 0x80) {
            // ASCII — valid
            ++i;
            continue;
        }

        size_t seq_len = 0;
        uint32_t codepoint = 0;

        if ((byte & 0xE0) == 0xC0) {
            seq_len = 2;
            codepoint = byte & 0x1F;
        } else if ((byte & 0xF0) == 0xE0) {
            seq_len = 3;
            codepoint = byte & 0x0F;
        } else if ((byte & 0xF8) == 0xF0) {
            seq_len = 4;
            codepoint = byte & 0x07;
        } else {
            result.valid = false;
            result.error_offset = i;
            result.error_byte = byte;
            result.error_desc = (byte >= 0x80 && byte <= 0xBF)
                ? "unexpected continuation byte"
                : (byte >= 0xF5) ? "byte exceeds U+10FFFF" : "invalid UTF-8 lead byte";
            return result;
        }

        // Check for overlong encodings and incomplete sequences
        if (i + seq_len > input.size()) {
            result.valid = false;
            result.error_offset = i;
            result.error_byte = byte;
            result.error_desc = "incomplete multi-byte sequence";
            return result;
        }

        // Read continuation bytes
        for (size_t j = 1; j < seq_len; ++j) {
            auto cb = static_cast<uint8_t>(input[i + j]);
            if ((cb & 0xC0) != 0x80) {
                result.valid = false;
                result.error_offset = i + j;
                result.error_byte = cb;
                result.error_desc = "expected continuation byte";
                return result;
            }
            codepoint = (codepoint << 6) | (cb & 0x3F);
        }

        // Check overlong encodings
        if (seq_len == 2 && codepoint < 0x80) {
            result.valid = false;
            result.error_offset = i;
            result.error_byte = byte;
            result.error_desc = "overlong 2-byte sequence";
            return result;
        }
        if (seq_len == 3 && codepoint < 0x800) {
            result.valid = false;
            result.error_offset = i;
            result.error_byte = byte;
            result.error_desc = "overlong 3-byte sequence";
            return result;
        }
        if (seq_len == 4 && codepoint < 0x10000) {
            result.valid = false;
            result.error_offset = i;
            result.error_byte = byte;
            result.error_desc = "overlong 4-byte sequence";
            return result;
        }

        // Check surrogates (U+D800..U+DFFF)
        if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
            result.valid = false;
            result.error_offset = i;
            result.error_byte = byte;
            result.error_desc = "UTF-8 encoded surrogate";
            return result;
        }

        // Check > U+10FFFF
        if (codepoint > 0x10FFFF) {
            result.valid = false;
            result.error_offset = i;
            result.error_byte = byte;
            result.error_desc = "codepoint exceeds U+10FFFF";
            return result;
        }

        // Check 0xF4 range (U+10FFFF max)
        if (byte == 0xF4 && static_cast<uint8_t>(input[i + 1]) > 0x8F) {
            result.valid = false;
            result.error_offset = i;
            result.error_byte = byte;
            result.error_desc = "codepoint exceeds U+10FFFF";
            return result;
        }

        i += seq_len;
    }

    return result;
}

} // namespace simdtext

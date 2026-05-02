#include "simdtext/simdtext.hpp"
#include <cstring>

namespace simdtext {

// ── Scanning ───────────────────────────────────────────────

size_t count_byte(std::span<const char> input, char byte) {
    size_t count = 0;
    const char* ptr = input.data();
    const char* end = ptr + input.size();

    // TODO: SIMD path for count_byte
    for (; ptr < end; ++ptr) {
        if (*ptr == byte) ++count;
    }
    return count;
}

size_t count_newlines(std::span<const char> input) {
    return count_byte(input, '\n');
}

bool contains(std::string_view input, std::string_view needle) {
    if (needle.empty()) return true;
    if (needle.size() > input.size()) return false;
    // TODO: SIMD acceleration for single-byte needle
    return input.find(needle) != std::string_view::npos;
}

const char* find_byte(const char* begin, const char* end, char byte) {
    // TODO: SIMD path
    for (const char* p = begin; p < end; ++p) {
        if (*p == byte) return p;
    }
    return end;
}

// ── ASCII ──────────────────────────────────────────────────

bool is_ascii(std::span<const char> input) {
    const char* ptr = input.data();
    const char* end = ptr + input.size();

    // TODO: SIMD path — test 16/32 bytes at once
    for (; ptr < end; ++ptr) {
        if (static_cast<unsigned char>(*ptr) > 0x7F) return false;
    }
    return true;
}

void lowercase_ascii_inplace(std::span<char> input) {
    char* ptr = input.data();
    char* end = ptr + input.size();

    // TODO: SIMD — OR with 0x20 for 16/32 bytes at once
    for (; ptr < end; ++ptr) {
        if (*ptr >= 'A' && *ptr <= 'Z') {
            *ptr = *ptr | 0x20;
        }
    }
}

void uppercase_ascii_inplace(std::span<char> input) {
    char* ptr = input.data();
    char* end = ptr + input.size();

    // TODO: SIMD — AND with ~0x20 for 16/32 bytes at once
    for (; ptr < end; ++ptr) {
        if (*ptr >= 'a' && *ptr <= 'z') {
            *ptr = *ptr & ~0x20;
        }
    }
}

std::string_view trim_ascii(std::string_view input) {
    while (!input.empty() && (input.front() == ' '  ||
                               input.front() == '\t' ||
                               input.front() == '\r' ||
                               input.front() == '\n')) {
        input.remove_prefix(1);
    }
    while (!input.empty() && (input.back() == ' '  ||
                               input.back() == '\t' ||
                               input.back() == '\r' ||
                               input.back() == '\n')) {
        input.remove_suffix(1);
    }
    return input;
}

// ── Lines & Splitting ──────────────────────────────────────

LineView::Iterator::Iterator(std::string_view remaining)
    : remaining_(remaining) {
    // Find first newline
    auto pos = remaining_.find('\n');
    if (pos == std::string_view::npos) {
        line_ = remaining_;
        remaining_ = {};
    } else {
        line_ = remaining_.substr(0, pos);
        remaining_ = remaining_.substr(pos + 1);
    }
}

LineView::Iterator& LineView::Iterator::operator++() {
    auto pos = remaining_.find('\n');
    if (pos == std::string_view::npos) {
        line_ = remaining_;
        remaining_ = {};
    } else {
        line_ = remaining_.substr(0, pos);
        remaining_ = remaining_.substr(pos + 1);
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
    : remaining_(remaining), delim_(delim) {
    auto pos = remaining_.find(delim_);
    if (pos == std::string_view::npos) {
        segment_ = remaining_;
        remaining_ = {};
    } else {
        segment_ = remaining_.substr(0, pos);
        remaining_ = remaining_.substr(pos + 1);
    }
}

SplitView::Iterator& SplitView::Iterator::operator++() {
    auto pos = remaining_.find(delim_);
    if (pos == std::string_view::npos) {
        segment_ = remaining_;
        remaining_ = {};
    } else {
        segment_ = remaining_.substr(0, pos);
        remaining_ = remaining_.substr(pos + 1);
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
    const uint8_t* p = reinterpret_cast<const uint8_t*>(input.data());
    const uint8_t* end = p + input.size();

    while (p < end) {
        uint8_t byte = *p++;

        if (byte <= 0x7F) {
            // 1-byte: 0xxxxxxx
            continue;
        } else if ((byte & 0xE0) == 0xC0) {
            // 2-byte: 110xxxxx 10xxxxxx
            if (p >= end || (*p & 0xC0) != 0x80) return false;
            if (byte < 0xC2) return false; // overlong
            ++p;
        } else if ((byte & 0xF0) == 0xE0) {
            // 3-byte: 1110xxxx 10xxxxxx 10xxxxxx
            if (p + 1 >= end || (*p & 0xC0) != 0x80 || (*(p+1) & 0xC0) != 0x80) return false;
            if (byte == 0xE0 && *p < 0xA0) return false; // overlong
            if (byte == 0xED && *p > 0x9F) return false; // surrogate
            p += 2;
        } else if ((byte & 0xF8) == 0xF0) {
            // 4-byte: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            if (p + 2 >= end || (*p & 0xC0) != 0x80 || (*(p+1) & 0xC0) != 0x80 || (*(p+2) & 0xC0) != 0x80) return false;
            if (byte == 0xF0 && *p < 0x90) return false; // overlong
            if (byte > 0xF4) return false; // > U+10FFFF
            p += 3;
        } else {
            return false;
        }
    }
    return true;
}

} // namespace simdtext

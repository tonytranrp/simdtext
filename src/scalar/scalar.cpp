#include "simdtext/simdtext.hpp"
#include <cstring>
#include <utility>

namespace simdtext {

// ── Scanning (scalar fallback, used when Highway is disabled) ──

size_t count_byte(std::span<const char> input, char byte) {
    size_t count = 0;
    for (auto c : input) {
        if (c == byte) ++count;
    }
    return count;
}

size_t count_newlines(std::span<const char> input) {
    return count_byte(input, '\n');
}

bool is_ascii(std::span<const char> input) {
    for (auto c : input) {
        if (static_cast<unsigned char>(c) >= 0x80) return false;
    }
    return true;
}

void lowercase_ascii_inplace(std::span<char> input) {
    for (auto& c : input) {
        if (c >= 'A' && c <= 'Z') c |= 0x20;
    }
}

void uppercase_ascii_inplace(std::span<char> input) {
    for (auto& c : input) {
        if (c >= 'a' && c <= 'z') c &= ~0x20;
    }
}

const char* find_byte(const char* begin, const char* end, char byte) {
    for (const char* p = begin; p != end; ++p) {
        if (*p == byte) return p;
    }
    return end;
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

namespace {

void advance_split(std::string_view& remaining, std::string_view& segment, char delim) {
    const auto pos = remaining.find(delim);
    if (pos == std::string_view::npos) {
        segment = remaining;
        remaining = {};
    } else {
        segment = remaining.substr(0, pos);
        remaining = remaining.substr(pos + 1);
    }
}

} // anonymous namespace

LineView::Iterator::Iterator(std::string_view remaining)
    : remaining_(remaining) {
    advance_split(remaining_, line_, '\n');
}

LineView::Iterator& LineView::Iterator::operator++() {
    advance_split(remaining_, line_, '\n');
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
    advance_split(remaining_, segment_, delim_);
}

SplitView::Iterator& SplitView::Iterator::operator++() {
    advance_split(remaining_, segment_, delim_);
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

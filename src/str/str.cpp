#include "simdtext/str.hpp"
#include "simdtext/scan.hpp"
#include "simdtext/ascii.hpp"
#include <cstring>

namespace simdtext {

// ── Trim ────────────────────────────────────────────────────

static inline bool is_whitespace(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

std::string_view trim_left(std::string_view s) noexcept {
    size_t i = 0;
    while (i < s.size() && is_whitespace(s[i])) ++i;
    return s.substr(i);
}

std::string_view trim_right(std::string_view s) noexcept {
    size_t i = s.size();
    while (i > 0 && is_whitespace(s[i - 1])) --i;
    return s.substr(0, i);
}

std::string_view trim(std::string_view s) noexcept {
    return trim_left(trim_right(s));
}

// ── Replace ─────────────────────────────────────────────────

std::string replace_all(std::string_view input, char needle, char replacement) {
    std::string result(input);
    const char* end = find_byte(std::span<const char>{result.data(), result.size()}, needle);
    // Use SIMD find_byte to skip to each occurrence
    for (size_t i = 0; i < result.size(); ) {
        const char* found = find_byte(std::span<const char>{result.data() + i, result.size() - i}, needle);
        if (found == result.data() + result.size()) break;
        const size_t pos = static_cast<size_t>(found - result.data());
        result[pos] = replacement;
        i = pos + 1;
    }
    return result;
}

std::string replace_all(std::string_view input, std::string_view needle, std::string_view replacement) {
    if (needle.empty()) return std::string(input);

    std::string result;
    result.reserve(input.size());

    size_t pos = 0;
    while (pos < input.size()) {
        // Find next occurrence of needle
        const char* search_start = input.data() + pos;
        const char* search_end = input.data() + input.size();

        // For single-char needles, use SIMD find_byte
        if (needle.size() == 1) {
            const char* found = find_byte(std::span<const char>{search_start, static_cast<size_t>(search_end - search_start)}, needle[0]);
            size_t match_pos = static_cast<size_t>(found - input.data());
            if (found == search_end) {
                result.append(input.substr(pos));
                break;
            }
            result.append(input.substr(pos, match_pos - pos));
            result.append(replacement);
            pos = match_pos + needle.size();
        } else {
            // Multi-char: use memchr to find first byte, then compare
            const void* found = memchr(search_start, static_cast<unsigned char>(needle[0]),
                                       static_cast<size_t>(search_end - search_start));
            if (!found) {
                result.append(input.substr(pos));
                break;
            }
            size_t match_pos = static_cast<size_t>(static_cast<const char*>(found) - input.data());
            // Check if full needle matches at this position
            if (match_pos + needle.size() <= input.size() &&
                std::memcmp(input.data() + match_pos, needle.data(), needle.size()) == 0) {
                result.append(input.substr(pos, match_pos - pos));
                result.append(replacement);
                pos = match_pos + needle.size();
            } else {
                result.append(input.substr(pos, match_pos - pos + 1));
                pos = match_pos + 1;
            }
        }
    }
    return result;
}

// ── Field splitting ──────────────────────────────────────────

std::vector<std::string_view> fields(std::string_view input) {
    std::vector<std::string_view> result;
    size_t i = 0;
    while (i < input.size()) {
        // Skip whitespace
        while (i < input.size() && is_whitespace(input[i])) ++i;
        if (i >= input.size()) break;
        // Find end of field
        size_t start = i;
        while (i < input.size() && !is_whitespace(input[i])) ++i;
        result.push_back(input.substr(start, i - start));
    }
    return result;
}

std::vector<std::string_view> split_vec(std::string_view input, char delimiter) {
    std::vector<std::string_view> result;
    size_t pos = 0;
    while (pos <= input.size()) {
        const char* found = find_byte(std::span<const char>{input.data() + pos, input.size() - pos}, delimiter);
        size_t end = static_cast<size_t>(found - input.data());
        if (found == input.data() + input.size()) {
            result.push_back(input.substr(pos));
            break;
        }
        result.push_back(input.substr(pos, end - pos));
        pos = end + 1;
    }
    return result;
}

size_t split_into(std::string_view input, char delimiter,
    std::string_view* out, size_t capacity) noexcept {
    size_t count = 0;
    size_t pos = 0;
    while (pos <= input.size() && count < capacity) {
        const char* found = find_byte(std::span<const char>{input.data() + pos, input.size() - pos}, delimiter);
        size_t end = static_cast<size_t>(found - input.data());
        if (found == input.data() + input.size()) {
            if (count < capacity) out[count++] = input.substr(pos);
            break;
        }
        if (count < capacity) out[count++] = input.substr(pos, end - pos);
        pos = end + 1;
    }
    return count;
}

// ── Starts/Ends With ────────────────────────────────────────

bool starts_with(std::string_view input, std::string_view prefix) noexcept {
    if (prefix.size() > input.size()) return false;
    return std::memcmp(input.data(), prefix.data(), prefix.size()) == 0;
}

bool ends_with(std::string_view input, std::string_view suffix) noexcept {
    if (suffix.size() > input.size()) return false;
    return std::memcmp(input.data() + input.size() - suffix.size(),
                       suffix.data(), suffix.size()) == 0;
}

// ── Contains ────────────────────────────────────────────────

bool contains_char(std::string_view input, char needle) noexcept {
    return find_byte(std::span<const char>{input.data(), input.size()}, needle) != input.data() + input.size();
}

} // namespace simdtext

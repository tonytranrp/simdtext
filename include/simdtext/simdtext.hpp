#pragma once

/// simdtext — High-performance C++ text utilities for large buffers
///
/// Provides fast zero-allocation operations for scanning, splitting,
/// trimming, encoding, decoding, and validating text.
///
/// Designed for logs, network protocols, data files, config parsers,
/// game engines, and backend services.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <iterator>

namespace simdtext {

// ── Error handling ─────────────────────────────────────────

enum class ErrorCode {
    Ok = 0,
    InvalidChar,
    InvalidLength,
    OutputTooSmall,
};

struct DecodeResult {
    size_t bytes_written = 0;
    size_t error_offset = 0;
    ErrorCode error = ErrorCode::Ok;

    bool ok() const { return error == ErrorCode::Ok; }
};

// ── Scanning ───────────────────────────────────────────────

/// Count occurrences of a byte in the input buffer.
size_t count_byte(std::span<const char> input, char byte);

/// Count newline characters ('\n') in the input buffer.
size_t count_newlines(std::span<const char> input);

/// Check if the input contains the given needle.
bool contains(std::string_view input, std::string_view needle);

/// Find the first occurrence of a byte in [begin, end). Returns end if not found.
const char* find_byte(const char* begin, const char* end, char byte);

// ── ASCII ──────────────────────────────────────────────────

/// Check if all bytes in the input are ASCII (0x00–0x7F).
bool is_ascii(std::span<const char> input);

/// Lowercase ASCII bytes in-place (A–Z → a–z). Non-ASCII bytes unchanged.
void lowercase_ascii_inplace(std::span<char> input);

/// Uppercase ASCII bytes in-place (a–z → A–Z). Non-ASCII bytes unchanged.
void uppercase_ascii_inplace(std::span<char> input);

/// Trim leading and trailing ASCII whitespace (space, tab, CR, LF).
std::string_view trim_ascii(std::string_view input);

// ── Lines & Splitting ──────────────────────────────────────

/// Iterable view over lines in a text buffer (split by '\n').
/// Each line is a string_view — no allocation.
class LineView {
public:
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::string_view;
        using difference_type = std::ptrdiff_t;
        using pointer = const std::string_view*;
        using reference = const std::string_view&;

        Iterator() = default;
        Iterator(std::string_view remaining);

        reference operator*() const { return line_; }
        pointer operator->() const { return &line_; }

        Iterator& operator++();
        Iterator operator++(int);

        bool operator==(const Iterator& other) const {
            return remaining_.data() == other.remaining_.data() &&
                   remaining_.size() == other.remaining_.size();
        }
        bool operator!=(const Iterator& other) const { return !(*this == other); }

    private:
        std::string_view remaining_;
        std::string_view line_;
    };

    explicit LineView(std::string_view input) : input_(input) {}

    Iterator begin() const { return Iterator(input_); }
    Iterator end() const { return Iterator(); }

private:
    std::string_view input_;
};

/// Convenience: create a LineView from a string_view.
LineView lines(std::string_view input);

/// Iterable view over segments split by a delimiter.
class SplitView {
public:
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::string_view;
        using difference_type = std::ptrdiff_t;
        using pointer = const std::string_view*;
        using reference = const std::string_view&;

        Iterator() = default;
        Iterator(std::string_view remaining, char delim);

        reference operator*() const { return segment_; }
        pointer operator->() const { return &segment_; }

        Iterator& operator++();
        Iterator operator++(int);

        bool operator==(const Iterator& other) const {
            return remaining_.data() == other.remaining_.data() &&
                   remaining_.size() == other.remaining_.size();
        }
        bool operator!=(const Iterator& other) const { return !(*this == other); }

    private:
        std::string_view remaining_;
        std::string_view segment_;
        char delim_;
    };

    SplitView(std::string_view input, char delim) : input_(input), delim_(delim) {}

    Iterator begin() const { return Iterator(input_, delim_); }
    Iterator end() const { return Iterator(); }

private:
    std::string_view input_;
    char delim_;
};

/// Split a string_view by a delimiter into an iterable view.
SplitView split(std::string_view input, char delimiter);

// ── Hex Encode/Decode ──────────────────────────────────────

/// Encode bytes to hexadecimal. Returns bytes written, 0 on error.
size_t hex_encode_to(std::span<const std::byte> input, std::span<char> output);

/// Encode bytes to hexadecimal string.
std::string hex_encode(std::span<const std::byte> input);

/// Decode hexadecimal to bytes.
DecodeResult hex_decode_to(std::string_view input, std::span<std::byte> output);

/// Decode hexadecimal to char buffer.
DecodeResult hex_decode_to(std::string_view input, std::span<char> output);

/// Decode hexadecimal string to byte vector.
std::vector<std::byte> hex_decode(std::string_view input);

// ── Base64 Encode/Decode ───────────────────────────────────

/// Encode bytes to Base64. Returns bytes written, 0 on error.
size_t base64_encode_to(std::span<const std::byte> input, std::span<char> output);

/// Encode bytes to Base64 string.
std::string base64_encode(std::span<const std::byte> input);

/// Decode Base64 to bytes.
DecodeResult base64_decode_to(std::string_view input, std::span<std::byte> output);

/// Decode Base64 to char buffer.
DecodeResult base64_decode_to(std::string_view input, std::span<char> output);

/// Decode Base64 string to byte vector.
std::vector<std::byte> base64_decode(std::string_view input);

// ── URL Encode/Decode ──────────────────────────────────────

/// URL-encode a string. Returns bytes written, 0 on error.
size_t url_encode_to(std::string_view input, std::span<char> output);

/// URL-encode a string.
std::string url_encode(std::string_view input);

/// URL-decode a string. Returns bytes written, 0 on error.
size_t url_decode_to(std::string_view input, std::span<char> output);

/// URL-decode a string.
std::string url_decode(std::string_view input);

/// Parse a query string into key-value pairs.
/// e.g. "name=tony&age=18" → {"name": "tony", "age": "18"}
std::unordered_map<std::string, std::string> parse_query(std::string_view query);

// ── UTF-8 ──────────────────────────────────────────────────

/// Validate UTF-8 encoding.
bool valid_utf8(std::span<const char> input);

// ── File I/O ───────────────────────────────────────────────

/// Memory-mapped file (zero-copy read access).
class MappedFile {
public:
    MappedFile();
    explicit MappedFile(const char* path);
    ~MappedFile();

    MappedFile(MappedFile&&) noexcept;
    MappedFile& operator=(MappedFile&&) noexcept;
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    bool open(const char* path);
    std::string_view view() const;
    size_t size() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// High-level file scanner for line-by-line processing.
class FileScanner {
public:
    explicit FileScanner(const char* path);
    explicit FileScanner(const std::string& path);

    bool is_open() const;

    /// Iterate over each line in the file.
    void each_line(std::function<void(std::string_view)> callback) const;

    /// Iterate over lines containing the needle.
    void each_line_containing(std::string_view needle,
                               std::function<void(std::string_view)> callback) const;

    /// Count total lines in the file.
    size_t count_lines() const;

    /// Count lines containing the needle.
    size_t count_matching(std::string_view needle) const;

private:
    MappedFile file_;
};

// ── Internal ───────────────────────────────────────────────

/// Hex digit value (internal helper).
int hex_val(char c);

} // namespace simdtext

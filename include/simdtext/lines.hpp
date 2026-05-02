#pragma once

/// @file lines.hpp
/// @brief Zero-allocation line iteration and string splitting.

#include "export.hpp"
#include <cstddef>
#include <iterator>
#include <string_view>

namespace simdtext {

/// Iterable view over lines in a text buffer (split by '\n').
/// Each line is a string_view — no allocation.
class SIMDTEXT_API LineView {
public:
    class SIMDTEXT_API Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::string_view;
        using difference_type = std::ptrdiff_t;
        using pointer = const std::string_view*;
        using reference = const std::string_view&;

        /// End sentinel constructor
        Iterator() : line_(), remaining_(), has_value_(false) {}

        Iterator(std::string_view remaining);

        SIMDTEXT_NODISCARD reference operator*() const { return line_; }
        SIMDTEXT_NODISCARD pointer operator->() const { return &line_; }

        Iterator& operator++();
        Iterator operator++(int);

        SIMDTEXT_NODISCARD bool operator==(const Iterator& other) const {
            return has_value_ == other.has_value_ && (!has_value_ || line_.data() == other.line_.data());
        }
        SIMDTEXT_NODISCARD bool operator!=(const Iterator& other) const { return !(*this == other); }

    private:
        std::string_view line_;
        std::string_view remaining_;
        bool has_value_;  // true = we have a line to yield, false = end sentinel
        void advance();
    };

    explicit LineView(std::string_view input) : input_(input) {}

    Iterator begin() const { return Iterator(input_); }
    Iterator end() const { return Iterator(); }

private:
    std::string_view input_;
};

/// Convenience: create a LineView from a string_view.
SIMDTEXT_NODISCARD SIMDTEXT_API LineView lines(std::string_view input);

/// Iterable view over segments split by a delimiter.
class SIMDTEXT_API SplitView {
public:
    class SIMDTEXT_API Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::string_view;
        using difference_type = std::ptrdiff_t;
        using pointer = const std::string_view*;
        using reference = const std::string_view&;

        /// End sentinel constructor
        Iterator() : segment_(), remaining_(), delim_('\0'), has_value_(false) {}

        Iterator(std::string_view remaining, char delim);

        SIMDTEXT_NODISCARD reference operator*() const { return segment_; }
        SIMDTEXT_NODISCARD pointer operator->() const { return &segment_; }

        Iterator& operator++();
        Iterator operator++(int);

        SIMDTEXT_NODISCARD bool operator==(const Iterator& other) const {
            return has_value_ == other.has_value_ && (!has_value_ || segment_.data() == other.segment_.data());
        }
        SIMDTEXT_NODISCARD bool operator!=(const Iterator& other) const { return !(*this == other); }

    private:
        std::string_view segment_;
        std::string_view remaining_;
        char delim_;
        bool has_value_;  // true = we have a segment to yield, false = end sentinel
        void advance();
    };

    SplitView(std::string_view input, char delim) : input_(input), delim_(delim) {}

    Iterator begin() const { return Iterator(input_, delim_); }
    Iterator end() const { return Iterator(); }

private:
    std::string_view input_;
    char delim_;
};

/// Split a string_view by a delimiter into an iterable view.
SIMDTEXT_NODISCARD SIMDTEXT_API SplitView split(std::string_view input, char delimiter);

} // namespace simdtext

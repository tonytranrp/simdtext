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

        Iterator() = default;
        Iterator(std::string_view remaining);

        SIMDTEXT_NODISCARD reference operator*() const { return line_; }
        SIMDTEXT_NODISCARD pointer operator->() const { return &line_; }

        Iterator& operator++();
        Iterator operator++(int);

        SIMDTEXT_NODISCARD bool operator==(const Iterator& other) const {
            return remaining_.data() == other.remaining_.data() &&
                   remaining_.size() == other.remaining_.size() &&
                   line_.data() == other.line_.data() &&
                   line_.size() == other.line_.size();
        }
        SIMDTEXT_NODISCARD bool operator!=(const Iterator& other) const { return !(*this == other); }

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

        Iterator() = default;
        Iterator(std::string_view remaining, char delim);

        SIMDTEXT_NODISCARD reference operator*() const { return segment_; }
        SIMDTEXT_NODISCARD pointer operator->() const { return &segment_; }

        Iterator& operator++();
        Iterator operator++(int);

        SIMDTEXT_NODISCARD bool operator==(const Iterator& other) const {
            return remaining_.data() == other.remaining_.data() &&
                   remaining_.size() == other.remaining_.size() &&
                   segment_.data() == other.segment_.data() &&
                   segment_.size() == other.segment_.size();
        }
        SIMDTEXT_NODISCARD bool operator!=(const Iterator& other) const { return !(*this == other); }

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
SIMDTEXT_NODISCARD SIMDTEXT_API SplitView split(std::string_view input, char delimiter);

} // namespace simdtext

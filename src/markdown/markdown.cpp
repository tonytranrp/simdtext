#include "simdtext/markdown.hpp"
#include <cstring>

namespace simdtext {

MarkdownTokenizer::MarkdownTokenizer(std::string_view input) noexcept
    : input_(input), pos_(0), token_(), in_code_block_(false) {}

void MarkdownTokenizer::skip_whitespace() noexcept {
    while (pos_ < input_.size() && (input_[pos_] == ' ' || input_[pos_] == '\t')) ++pos_;
}

const MdToken& MarkdownTokenizer::next() noexcept {
    if (pos_ >= input_.size()) {
        token_ = {MdType::End, {}, {}, 0, pos_};
        return token_;
    }

    // Skip leading whitespace but preserve position for offset
    size_t line_start = pos_;
    skip_whitespace();

    // Handle code block state
    if (in_code_block_) {
        // Check for closing ```
        if (pos_ + 2 < input_.size() && input_[pos_] == '`' && input_[pos_+1] == '`' && input_[pos_+2] == '`') {
            // Skip to end of line
            while (pos_ < input_.size() && input_[pos_] != '\n') ++pos_;
            if (pos_ < input_.size()) ++pos_;
            in_code_block_ = false;
            token_ = {MdType::CodeBlock, input_.substr(line_start, pos_ - line_start), {}, 0, line_start};
            return token_;
        }
        // Code block content
        size_t content_start = pos_;
        while (pos_ < input_.size() && input_[pos_] != '\n') ++pos_;
        if (pos_ < input_.size()) ++pos_;
        token_ = {MdType::CodeBlock, input_.substr(line_start, pos_ - line_start),
                  input_.substr(content_start, pos_ - content_start - (pos_ > content_start && input_[pos_-1] == '\n' ? 1 : 0)),
                  0, line_start};
        return token_;
    }

    const size_t start = pos_;

    // Heading: # ## ### etc.
    if (input_[pos_] == '#') {
        int level = 0;
        while (pos_ < input_.size() && input_[pos_] == '#') { ++pos_; ++level; }
        level = (level > 6) ? 6 : level;
        // Skip space after #
        while (pos_ < input_.size() && input_[pos_] == ' ') ++pos_;
        size_t content_start = pos_;
        while (pos_ < input_.size() && input_[pos_] != '\n') ++pos_;
        std::string_view content = input_.substr(content_start, pos_ - content_start);
        if (pos_ < input_.size()) ++pos_;
        token_ = {MdType::Heading, input_.substr(line_start, pos_ - line_start), content, level, line_start};
        return token_;
    }

    // Code block: ```
    if (pos_ + 2 < input_.size() && input_[pos_] == '`' && input_[pos_+1] == '`' && input_[pos_+2] == '`') {
        pos_ += 3;
        // Skip optional language tag
        size_t lang_start = pos_;
        while (pos_ < input_.size() && input_[pos_] != '\n') ++pos_;
        std::string_view lang = input_.substr(lang_start, pos_ - lang_start);
        if (pos_ < input_.size()) ++pos_;
        in_code_block_ = true;
        token_ = {MdType::CodeBlock, input_.substr(line_start, pos_ - line_start), lang, 0, line_start};
        return token_;
    }

    // Horizontal rule: --- or *** or ___
    if (pos_ + 2 < input_.size()) {
        char c = input_[pos_];
        if ((c == '-' || c == '*' || c == '_') && input_[pos_+1] == c && input_[pos_+2] == c) {
            // Check it's really an HR (all same char, at least 3)
            size_t hr_len = 0;
            while (pos_ < input_.size() && input_[pos_] == c) { ++pos_; ++hr_len; }
            while (pos_ < input_.size() && input_[pos_] == ' ') ++pos_;
            if (pos_ >= input_.size() || input_[pos_] == '\n') {
                if (pos_ < input_.size()) ++pos_;
                token_ = {MdType::HorizontalRule, input_.substr(line_start, pos_ - line_start), {}, 0, line_start};
                return token_;
            }
            // Not an HR, reset
            pos_ = start;
        }
    }

    // Blockquote: >
    if (input_[pos_] == '>') {
        ++pos_;
        if (pos_ < input_.size() && input_[pos_] == ' ') ++pos_;
        size_t content_start = pos_;
        while (pos_ < input_.size() && input_[pos_] != '\n') ++pos_;
        std::string_view content = input_.substr(content_start, pos_ - content_start);
        if (pos_ < input_.size()) ++pos_;
        token_ = {MdType::Blockquote, input_.substr(line_start, pos_ - line_start), content, 0, line_start};
        return token_;
    }

    // List item: - or * or 1.
    if ((input_[pos_] == '-' || input_[pos_] == '*') && pos_ + 1 < input_.size() && input_[pos_+1] == ' ') {
        pos_ += 2;
        size_t content_start = pos_;
        while (pos_ < input_.size() && input_[pos_] != '\n') ++pos_;
        std::string_view content = input_.substr(content_start, pos_ - content_start);
        if (pos_ < input_.size()) ++pos_;
        token_ = {MdType::ListItem, input_.substr(line_start, pos_ - line_start), content, 0, line_start};
        return token_;
    }
    if (input_[pos_] >= '0' && input_[pos_] <= '9' && pos_ + 1 < input_.size() && input_[pos_+1] == '.') {
        pos_ += 2;
        size_t content_start = pos_;
        while (pos_ < input_.size() && input_[pos_] != '\n') ++pos_;
        std::string_view content = input_.substr(content_start, pos_ - content_start);
        if (pos_ < input_.size()) ++pos_;
        token_ = {MdType::ListItem, input_.substr(line_start, pos_ - line_start), content, 1, line_start};
        return token_;
    }

    // Paragraph (default)
    size_t content_start = pos_;
    while (pos_ < input_.size() && input_[pos_] != '\n') ++pos_;
    std::string_view content = input_.substr(content_start, pos_ - content_start);
    if (pos_ < input_.size()) ++pos_;
    token_ = {MdType::Paragraph, input_.substr(line_start, pos_ - line_start), content, 0, line_start};
    return token_;
}

void MarkdownTokenizer::reset(std::string_view input) noexcept {
    input_ = input;
    pos_ = 0;
    token_ = {};
    in_code_block_ = false;
}

} // namespace simdtext

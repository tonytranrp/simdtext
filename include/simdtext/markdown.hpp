#pragma once

/// @file markdown.hpp
/// @brief Zero-allocation Markdown tokenization.

#include "export.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace simdtext {

/// Markdown token type.
enum class MdType : uint8_t {
    Heading,        // # ## ### etc.
    Paragraph,      // Regular text
    Bold,           // **text** or __text__
    Italic,         // *text* or _text_
    Code,           // `code`
    CodeBlock,      // ```lang ... ```
    Link,           // [text](url)
    Image,          // ![alt](url)
    ListItem,       // - item or * item or 1. item
    Blockquote,     // > quote
    HorizontalRule, // --- or *** or ___
    HtmlTag,        // <tag>...</tag>
    Error,
    End,
};

/// A Markdown token.
struct MdToken {
    MdType type = MdType::Error;
    std::string_view text;     // Full token text
    std::string_view content;  // Inner content (without delimiters)
    int level = 0;             // Heading level (1-6), list indent level
    size_t offset = 0;
};

/// Zero-allocation Markdown tokenizer (line-by-line).
class SIMDTEXT_API MarkdownTokenizer {
public:
    explicit MarkdownTokenizer(std::string_view input) noexcept;

    /// Advance to the next token. Returns the token.
    SIMDTEXT_NODISCARD const MdToken& next() noexcept;

    /// Get the current token.
    SIMDTEXT_NODISCARD const MdToken& token() const noexcept { return token_; }

    /// Reset the tokenizer.
    void reset(std::string_view input) noexcept;

private:
    std::string_view input_;
    size_t pos_;
    MdToken token_;
    bool in_code_block_;

    void skip_whitespace() noexcept;
};

} // namespace simdtext

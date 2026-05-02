#pragma once

/// @file xml.hpp
/// @brief Zero-allocation XML tokenization.

#include "export.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace simdtext {

/// XML token type.
enum class XmlType : uint8_t {
    OpenTag,       // <tag>
    CloseTag,      // </tag>
    SelfClose,     // <tag/>
    Text,          // Text content
    Attribute,     // name="value" inside a tag
    Comment,       // <!-- ... -->
    CData,         // <![CDATA[ ... ]]>
    Declaration,   // <?xml ... ?>
    ProcessingInstruction, // <?target ... ?>
    Error,
    End,
};

/// An XML token.
struct XmlToken {
    XmlType type = XmlType::Error;
    std::string_view value;     // Tag name, text content, or attribute name
    std::string_view attr_value; // Attribute value (if type == Attribute)
    size_t offset = 0;
};

/// Zero-allocation XML tokenizer.
class SIMDTEXT_API XmlTokenizer {
public:
    explicit XmlTokenizer(std::string_view input) noexcept;

    /// Advance to the next token. Returns the token.
    SIMDTEXT_NODISCARD const XmlToken& next() noexcept;

    /// Get the current token.
    SIMDTEXT_NODISCARD const XmlToken& token() const noexcept { return token_; }

    /// Check if there was an error.
    SIMDTEXT_NODISCARD bool has_error() const noexcept { return token_.type == XmlType::Error; }

    /// Reset the tokenizer.
    void reset(std::string_view input) noexcept;

private:
    std::string_view input_;
    size_t pos_;
    XmlToken token_;

    void skip_whitespace() noexcept;
    char peek() const noexcept;
};

/// Quick check if a string looks like XML (starts with <).
SIMDTEXT_NODISCARD SIMDTEXT_API bool looks_like_xml(std::string_view input) noexcept;

/// Escape XML special characters in-place. Returns the length of the escaped string.
SIMDTEXT_API size_t xml_escape_inplace(char* data, size_t len, size_t capacity) noexcept;

} // namespace simdtext

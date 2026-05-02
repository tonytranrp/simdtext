// test_markdown.cpp — Tests for Markdown tokenizer
#include "test_framework.hpp"
#include <simdtext/markdown.hpp>

using namespace simdtext;

void test_markdown() {
    std::printf("[markdown]\n");

    // Heading
    {
        MarkdownTokenizer tok("# Hello World\n");
        auto& t = tok.next();
        CHECK_EQ(t.type, MdType::Heading);
        CHECK_EQ(t.level, 1);
        CHECK_EQ(t.content, std::string_view("Hello World"));
    }

    // Heading levels
    {
        MarkdownTokenizer tok("### H3\n");
        CHECK_EQ(tok.next().level, 3);
    }

    // Code block
    {
        MarkdownTokenizer tok("```python\nprint('hi')\n```\n");
        CHECK_EQ(tok.next().type, MdType::CodeBlock); // opening
        CHECK_EQ(tok.next().type, MdType::CodeBlock); // content
        CHECK_EQ(tok.next().type, MdType::CodeBlock); // closing
        CHECK_EQ(tok.next().type, MdType::End);
    }

    // Blockquote
    {
        MarkdownTokenizer tok("> This is a quote\n");
        auto& t = tok.next();
        CHECK_EQ(t.type, MdType::Blockquote);
        CHECK_EQ(t.content, std::string_view("This is a quote"));
    }

    // Unordered list
    {
        MarkdownTokenizer tok("- item one\n- item two\n");
        CHECK_EQ(tok.next().type, MdType::ListItem);
        CHECK_EQ(tok.next().type, MdType::ListItem);
    }

    // Ordered list
    {
        MarkdownTokenizer tok("1. first\n2. second\n");
        auto& t1 = tok.next();
        CHECK_EQ(t1.type, MdType::ListItem);
        CHECK_EQ(t1.level, 1); // ordered
    }

    // Horizontal rule
    {
        MarkdownTokenizer tok("---\n");
        CHECK_EQ(tok.next().type, MdType::HorizontalRule);
    }

    // Paragraph
    {
        MarkdownTokenizer tok("Just some text\n");
        CHECK_EQ(tok.next().type, MdType::Paragraph);
    }

    // Mixed
    {
        MarkdownTokenizer tok("# Title\n\nSome text\n\n- item\n");
        CHECK_EQ(tok.next().type, MdType::Heading);
        CHECK_EQ(tok.next().type, MdType::Paragraph); // empty line = paragraph
        CHECK_EQ(tok.next().type, MdType::Paragraph);
        CHECK_EQ(tok.next().type, MdType::Paragraph);
        CHECK_EQ(tok.next().type, MdType::ListItem);
    }

    // Empty
    {
        MarkdownTokenizer tok("");
        CHECK_EQ(tok.next().type, MdType::End);
    }
}

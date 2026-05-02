// test_xml.cpp — Tests for XML tokenizer
#include "test_framework.hpp"
#include <simdtext/xml.hpp>
#include <string>

using namespace simdtext;

void test_xml() {
    std::printf("[xml]\n");

    // Simple element
    {
        XmlTokenizer tok("<hello>world</hello>");
        CHECK_EQ(tok.next().type, XmlType::OpenTag);
        CHECK_EQ(tok.token().value, std::string_view("hello"));
        CHECK_EQ(tok.next().type, XmlType::Text);
        CHECK_EQ(tok.token().value, std::string_view("world"));
        CHECK_EQ(tok.next().type, XmlType::CloseTag);
        CHECK_EQ(tok.token().value, std::string_view("hello"));
        CHECK_EQ(tok.next().type, XmlType::End);
    }

    // Self-closing tag
    {
        XmlTokenizer tok("<br/>");
        CHECK_EQ(tok.next().type, XmlType::SelfClose);
        CHECK_EQ(tok.token().value, std::string_view("br"));
    }

    // Comment
    {
        XmlTokenizer tok("<!-- hello world -->");
        CHECK_EQ(tok.next().type, XmlType::Comment);
        CHECK_EQ(tok.token().value, std::string_view(" hello world "));
    }

    // CDATA
    {
        XmlTokenizer tok("<![CDATA[<not xml>]]>");
        CHECK_EQ(tok.next().type, XmlType::CData);
        CHECK_EQ(tok.token().value, std::string_view("<not xml>"));
    }

    // XML declaration
    {
        XmlTokenizer tok(R"(<?xml version="1.0"?>)");
        CHECK_EQ(tok.next().type, XmlType::Declaration);
        CHECK_EQ(tok.token().value, std::string_view("xml"));
    }

    // Nested elements
    {
        XmlTokenizer tok("<div><p>text</p></div>");
        CHECK_EQ(tok.next().type, XmlType::OpenTag);
        CHECK_EQ(tok.token().value, std::string_view("div"));
        CHECK_EQ(tok.next().type, XmlType::OpenTag);
        CHECK_EQ(tok.token().value, std::string_view("p"));
        CHECK_EQ(tok.next().type, XmlType::Text);
        CHECK_EQ(tok.next().type, XmlType::CloseTag);
        CHECK_EQ(tok.token().value, std::string_view("p"));
        CHECK_EQ(tok.next().type, XmlType::CloseTag);
        CHECK_EQ(tok.token().value, std::string_view("div"));
    }

    // looks_like_xml
    {
        CHECK(looks_like_xml("<root/>"));
        CHECK(looks_like_xml("  <root/>"));
        CHECK(!looks_like_xml("hello"));
    }

    // xml_escape_inplace
    {
        char buf[100] = "a<b>c&d";
        size_t len = xml_escape_inplace(buf, 7, 100);
        CHECK_EQ(len, 17u);  // a&lt;b&gt;c&amp;d
        CHECK_EQ(std::string(buf, len), std::string("a&lt;b&gt;c&amp;d"));
    }

    // Empty
    {
        XmlTokenizer tok("");
        CHECK_EQ(tok.next().type, XmlType::End);
    }
}

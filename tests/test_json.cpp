// test_json.cpp — Tests for JSON tokenizer
#include "test_framework.hpp"
#include <simdtext/json.hpp>
#include <string>

using namespace simdtext;

void test_json() {
    std::printf("[json]\n");

    // Simple object
    {
        JsonTokenizer tok(R"({"key": "value", "num": 42, "flag": true, "empty": null})");
        CHECK_EQ(tok.next().type, JsonType::ObjectOpen);
        CHECK_EQ(tok.next().type, JsonType::String);
        CHECK_EQ(tok.next().type, JsonType::Colon);
        CHECK_EQ(tok.next().type, JsonType::String);
        CHECK_EQ(tok.next().type, JsonType::Comma);
        CHECK_EQ(tok.next().type, JsonType::String);
        CHECK_EQ(tok.next().type, JsonType::Colon);
        CHECK_EQ(tok.next().type, JsonType::Number);
        CHECK_EQ(tok.next().type, JsonType::Comma);
        CHECK_EQ(tok.next().type, JsonType::String);
        CHECK_EQ(tok.next().type, JsonType::Colon);
        CHECK_EQ(tok.next().type, JsonType::True);
        CHECK_EQ(tok.next().type, JsonType::Comma);
        CHECK_EQ(tok.next().type, JsonType::String);
        CHECK_EQ(tok.next().type, JsonType::Colon);
        CHECK_EQ(tok.next().type, JsonType::Null);
        CHECK_EQ(tok.next().type, JsonType::ObjectClose);
        CHECK_EQ(tok.next().type, JsonType::End);
    }

    // Array
    {
        JsonTokenizer tok(R"([1, 2.5, -3, 1e10])");
        CHECK_EQ(tok.next().type, JsonType::ArrayOpen);
        CHECK_EQ(tok.next().type, JsonType::Number);
        CHECK_EQ(tok.next().type, JsonType::Comma);
        CHECK_EQ(tok.next().type, JsonType::Number);
        CHECK_EQ(tok.next().type, JsonType::Comma);
        CHECK_EQ(tok.next().type, JsonType::Number);
        CHECK_EQ(tok.next().type, JsonType::Comma);
        CHECK_EQ(tok.next().type, JsonType::Number);
        CHECK_EQ(tok.next().type, JsonType::ArrayClose);
        CHECK_EQ(tok.next().type, JsonType::End);
    }

    // String with escapes
    {
        JsonTokenizer tok(R"({"msg": "hello\nworld"})");
        CHECK_EQ(tok.next().type, JsonType::ObjectOpen);
        auto key = tok.next();
        CHECK_EQ(key.type, JsonType::String);
        CHECK_EQ(tok.next().type, JsonType::Colon);
        auto val = tok.next();
        CHECK_EQ(val.type, JsonType::String);
        CHECK_EQ(tok.next().type, JsonType::ObjectClose);
    }

    // Nested
    {
        JsonTokenizer tok(R"({"a": [1, {"b": 2}]})");
        CHECK_EQ(tok.next().type, JsonType::ObjectOpen);
        CHECK_EQ(tok.next().type, JsonType::String);
        CHECK_EQ(tok.next().type, JsonType::Colon);
        CHECK_EQ(tok.next().type, JsonType::ArrayOpen);
        CHECK_EQ(tok.next().type, JsonType::Number);
        CHECK_EQ(tok.next().type, JsonType::Comma);
        CHECK_EQ(tok.next().type, JsonType::ObjectOpen);
        CHECK_EQ(tok.next().type, JsonType::String);
        CHECK_EQ(tok.next().type, JsonType::Colon);
        CHECK_EQ(tok.next().type, JsonType::Number);
        CHECK_EQ(tok.next().type, JsonType::ObjectClose);
        CHECK_EQ(tok.next().type, JsonType::ArrayClose);
        CHECK_EQ(tok.next().type, JsonType::ObjectClose);
        CHECK_EQ(tok.next().type, JsonType::End);
    }

    // Token values
    {
        JsonTokenizer tok(R"({"name": "Alice"})");
        CHECK_EQ(tok.next().type, JsonType::ObjectOpen);
        auto key = tok.next();
        CHECK_EQ(key.value, std::string_view("\"name\""));
        tok.next(); // colon
        auto val = tok.next();
        CHECK_EQ(val.value, std::string_view("\"Alice\""));
    }

    // Number values
    {
        JsonTokenizer tok(R"([42, -3.14, 1.5e+10])");
        tok.next(); // [
        CHECK_EQ(tok.next().value, std::string_view("42"));
        tok.next(); // ,
        CHECK_EQ(tok.next().value, std::string_view("-3.14"));
        tok.next(); // ,
        CHECK_EQ(tok.next().value, std::string_view("1.5e+10"));
    }

    // False
    {
        JsonTokenizer tok(R"([false])");
        tok.next(); // [
        CHECK_EQ(tok.next().type, JsonType::False);
    }

    // Whitespace handling
    {
        JsonTokenizer tok("  {  }  ");
        CHECK_EQ(tok.next().type, JsonType::ObjectOpen);
        CHECK_EQ(tok.next().type, JsonType::ObjectClose);
        CHECK_EQ(tok.next().type, JsonType::End);
    }

    // looks_like_json
    {
        CHECK(looks_like_json(R"({"a": 1})"));
        CHECK(looks_like_json("  [1, 2]"));
        CHECK(!looks_like_json("hello"));
        CHECK(!looks_like_json("42"));
    }

    // is_json_number
    {
        CHECK(is_json_number("42"));
        CHECK(is_json_number("-3.14"));
        CHECK(is_json_number("1e10"));
        CHECK(is_json_number("1.5e+10"));
        CHECK(!is_json_number("abc"));
        CHECK(!is_json_number(""));
        CHECK(!is_json_number("-"));
        CHECK(!is_json_number("1."));
    }

    // json_unescape_inplace
    {
        char buf[] = "hello\\nworld";
        size_t len = json_unescape_inplace(buf, 12);
        CHECK_EQ(len, 11u);
        CHECK_EQ(std::string(buf, len), std::string("hello\nworld"));
    }

    // Error: unterminated string
    {
        JsonTokenizer tok(R"({"key": "unterminated)");
        CHECK_EQ(tok.next().type, JsonType::ObjectOpen);
        CHECK_EQ(tok.next().type, JsonType::String);
        CHECK_EQ(tok.next().type, JsonType::Colon);
        CHECK_EQ(tok.next().type, JsonType::Error);
    }

    // Empty input
    {
        JsonTokenizer tok("");
        CHECK_EQ(tok.next().type, JsonType::End);
    }
}

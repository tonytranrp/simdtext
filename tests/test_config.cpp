// test_config.cpp — Tests for INI config parser
#include "test_framework.hpp"
#include <simdtext/config.hpp>

using namespace simdtext;

void test_config() {
    std::printf("[config]\n");

    // Basic INI
    {
        const char* ini = R"(
[database]
host = localhost
port = 5432

[server]
debug = true
)";
        ConfigParser parser(ini);
        CHECK_EQ(parser.parse(), 3u);
        CHECK_EQ(parser.get("database", "host"), std::string_view("localhost"));
        CHECK_EQ(parser.get("database", "port"), std::string_view("5432"));
        CHECK_EQ(parser.get("server", "debug"), std::string_view("true"));
    }

    // Comments
    {
        const char* ini = "# comment\n; also comment\nkey = val\n";
        ConfigParser parser(ini);
        CHECK_EQ(parser.parse(), 1u);
        CHECK_EQ(parser.get("", "key"), std::string_view("val"));
    }

    // Quoted values
    {
        const char* ini = "name = \"John Doe\"\npath = '/tmp/test'\n";
        ConfigParser parser(ini);
        CHECK_EQ(parser.parse(), 2u);
        CHECK_EQ(parser.get("", "name"), std::string_view("John Doe"));
        CHECK_EQ(parser.get("", "path"), std::string_view("/tmp/test"));
    }

    // Default value
    {
        const char* ini = "[app]\n";
        ConfigParser parser(ini);
        parser.parse();
        CHECK_EQ(parser.get("app", "missing", "default"), std::string_view("default"));
    }

    // Empty input
    {
        ConfigParser parser("");
        CHECK_EQ(parser.parse(), 0u);
    }

    // Entry access
    {
        const char* ini = "[s]\nk=v\n";
        ConfigParser parser(ini);
        parser.parse();
        CHECK_EQ(parser.entry(0).section, std::string_view("s"));
        CHECK_EQ(parser.entry(0).key, std::string_view("k"));
        CHECK_EQ(parser.entry(0).value, std::string_view("v"));
        CHECK_EQ(parser.entry(0).line, 2u);
    }
}

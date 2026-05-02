#include <simdtext/simdtext.hpp>
#include <cassert>
#include <print>
#include <vector>
#include <fstream>

#define TEST(name) std::print("  {}...", name);
#define PASS() std::print(" OK\n")

int main() {
    std::print("=== simdtext tests ===\n\n");

    // ── Scanning ───────────────────────────────────
    std::print("[Scanning]\n");

    TEST("count_byte");
    assert(simdtext::count_byte("hello\nworld\nfoo\n", '\n') == 3);
    assert(simdtext::count_byte("abc", 'z') == 0);
    assert(simdtext::count_byte("aaa", 'a') == 3);
    PASS();

    TEST("count_newlines");
    assert(simdtext::count_newlines("a\nb\nc\n") == 3);
    assert(simdtext::count_newlines("no newlines") == 0);
    PASS();

    TEST("contains");
    assert(simdtext::contains("hello world", "world") == true);
    assert(simdtext::contains("hello world", "xyz") == false);
    assert(simdtext::contains("abc", "") == true);
    PASS();

    TEST("find_byte");
    {
        const char* data = "hello";
        assert(simdtext::find_byte(data, data + 5, 'e') == data + 1);
        assert(simdtext::find_byte(data, data + 5, 'z') == data + 5);
    }
    PASS();

    // ── ASCII ──────────────────────────────────────
    std::print("\n[ASCII]\n");

    TEST("is_ascii");
    assert(simdtext::is_ascii("hello world") == true);
    {
        std::string non_ascii = "hello\xff";
        assert(simdtext::is_ascii(non_ascii) == false);
    }
    PASS();

    TEST("lowercase_ascii_inplace");
    {
        std::string s = "HELLO World 123";
        simdtext::lowercase_ascii_inplace(s);
        assert(s == "hello world 123");
    }
    {
        std::string s = "CONTENT-TYPE: APPLICATION/JSON";
        simdtext::lowercase_ascii_inplace(s);
        assert(s == "content-type: application/json");
    }
    PASS();

    TEST("uppercase_ascii_inplace");
    {
        std::string s = "hello World 123";
        simdtext::uppercase_ascii_inplace(s);
        assert(s == "HELLO WORLD 123");
    }
    PASS();

    TEST("trim_ascii");
    assert(simdtext::trim_ascii("  hello  ") == "hello");
    assert(simdtext::trim_ascii("\t\nhello\r\n") == "hello");
    assert(simdtext::trim_ascii("hello") == "hello");
    assert(simdtext::trim_ascii("   ") == "");
    PASS();

    // ── Lines & Splitting ──────────────────────────
    std::print("\n[Lines & Splitting]\n");

    TEST("lines");
    {
        std::string text = "line1\nline2\nline3";
        std::vector<std::string_view> result;
        for (const auto line : simdtext::lines(text)) result.push_back(line);
        assert(result.size() == 3);
        assert(result[0] == "line1");
        assert(result[1] == "line2");
        assert(result[2] == "line3");
    }
    {
        std::string text = "single";
        std::vector<std::string_view> result;
        for (const auto line : simdtext::lines(text)) result.push_back(line);
        assert(result.size() == 1);
        assert(result[0] == "single");
    }
    PASS();

    TEST("split");
    {
        std::string csv = "a,b,c,d";
        std::vector<std::string_view> parts;
        for (const auto part : simdtext::split(csv, ',')) parts.push_back(part);
        assert(parts.size() == 4);
        assert(parts[0] == "a");
        assert(parts[3] == "d");
    }
    PASS();

    // ── Hex ────────────────────────────────────────
    std::print("\n[Hex]\n");

    TEST("hex_encode");
    {
        const std::byte data[] = {std::byte(0xDE), std::byte(0xAD), std::byte(0xBE), std::byte(0xEF)};
        const auto result = simdtext::hex_encode(data);
        assert(result == "deadbeef");
    }
    {
        const std::byte data[] = {std::byte(0x48), std::byte(0x65), std::byte(0x6c), std::byte(0x6c), std::byte(0x6f)};
        const auto result = simdtext::hex_encode(data);
        assert(result == "48656c6c6f");
    }
    PASS();

    TEST("hex_decode");
    {
        const auto result = simdtext::hex_decode("48656c6c6f");
        assert(result.size() == 5);
        assert(reinterpret_cast<const char*>(result.data()) == std::string("Hello"));
    }
    {
        const auto result = simdtext::hex_decode("deadbeef");
        assert(result.size() == 4);
        assert(result[0] == std::byte(0xDE));
        assert(result[1] == std::byte(0xAD));
        assert(result[2] == std::byte(0xBE));
        assert(result[3] == std::byte(0xEF));
    }
    PASS();

    // ── Base64 ─────────────────────────────────────
    std::print("\n[Base64]\n");

    TEST("base64_encode");
    {
        const auto bytes = std::string("Hello");
        const auto encoded = simdtext::base64_encode(
            std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));
        assert(encoded == "SGVsbG8=");
    }
    {
        const auto bytes = std::string("Man");
        const auto encoded = simdtext::base64_encode(
            std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));
        assert(encoded == "TWFu");
    }
    PASS();

    TEST("base64_decode");
    {
        const auto result = simdtext::base64_decode("SGVsbG8=");
        const std::string str(reinterpret_cast<const char*>(result.data()), result.size());
        assert(str == "Hello");
    }
    {
        const auto result = simdtext::base64_decode("TWFu");
        assert(result.size() == 3);
        const std::string str(reinterpret_cast<const char*>(result.data()), result.size());
        assert(str == "Man");
    }
    PASS();

    // ── URL ────────────────────────────────────────
    std::print("\n[URL]\n");

    TEST("url_encode");
    assert(simdtext::url_encode("hello world") == "hello%20world");
    assert(simdtext::url_encode("a=b&c=d") == "a%3Db%26c%3Dd");
    PASS();

    TEST("url_decode");
    assert(simdtext::url_decode("hello%20world") == "hello world");
    assert(simdtext::url_decode("a%3Db%26c%3Dd") == "a=b&c=d");
    assert(simdtext::url_decode("hello+world") == "hello world");
    PASS();

    TEST("parse_query");
    {
        const auto params = simdtext::parse_query("name=tony&age=18");
        assert(params.at("name") == "tony");
        assert(params.at("age") == "18");
    }
    {
        const auto params = simdtext::parse_query("q=hello%20world&lang=c%2B%2B");
        assert(params.at("q") == "hello world");
        assert(params.at("lang") == "c++");
    }
    PASS();

    // ── UTF-8 ──────────────────────────────────────
    std::print("\n[UTF-8]\n");

    TEST("valid_utf8");
    assert(simdtext::valid_utf8("hello") == true);
    assert(simdtext::valid_utf8("caf\xc3\xa9") == true);
    assert(simdtext::valid_utf8("\xff") == false);
    PASS();

    // ── File I/O ───────────────────────────────────
    std::print("\n[File I/O]\n");

    TEST("MappedFile + FileScanner");
    {
        const char* path = "/tmp/simdtext_test.log";
        std::ofstream f(path, std::ios::binary);
        f << "INFO started\nERROR timeout\nWARNING memory\nERROR disk\n";
        f.close();

        const simdtext::FileScanner scanner(path);
        assert(scanner.is_open());

        assert(scanner.count_lines() == 4);
        assert(scanner.count_matching("ERROR") == 2);

        std::vector<std::string_view> error_lines;
        scanner.each_line_containing("ERROR", [&](std::string_view line) {
            error_lines.push_back(line);
        });
        assert(error_lines.size() == 2);
    }
    PASS();

    std::print("\n=== All tests passed! ===\n");
    return 0;
}

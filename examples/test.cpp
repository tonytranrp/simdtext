#include <simdtext/simdtext.hpp>
#include <cassert>
#include <iostream>
#include <vector>
#include <fstream>

#define TEST(name) std::cout << "  " << name << "..." << std::flush;
#define PASS() std::cout << " OK\n"

int main() {
    std::cout << "=== simdtext tests ===\n\n";

    // ── Scanning ───────────────────────────────────
    std::cout << "[Scanning]\n";

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
    std::cout << "\n[ASCII]\n";

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
    std::cout << "\n[Lines & Splitting]\n";

    TEST("lines");
    {
        std::string text = "line1\nline2\nline3";
        std::vector<std::string_view> result;
        for (auto line : simdtext::lines(text)) result.push_back(line);
        assert(result.size() == 3);
        assert(result[0] == "line1");
        assert(result[1] == "line2");
        assert(result[2] == "line3");
    }
    {
        std::string text = "single";
        std::vector<std::string_view> result;
        for (auto line : simdtext::lines(text)) result.push_back(line);
        assert(result.size() == 1);
        assert(result[0] == "single");
    }
    PASS();

    TEST("split");
    {
        std::string csv = "a,b,c,d";
        std::vector<std::string_view> parts;
        for (auto part : simdtext::split(csv, ',')) parts.push_back(part);
        assert(parts.size() == 4);
        assert(parts[0] == "a");
        assert(parts[3] == "d");
    }
    PASS();

    // ── Hex ────────────────────────────────────────
    std::cout << "\n[Hex]\n";

    TEST("hex_encode");
    {
        std::byte data[] = {std::byte(0xDE), std::byte(0xAD), std::byte(0xBE), std::byte(0xEF)};
        auto result = simdtext::hex_encode(data);
        assert(result == "deadbeef");
    }
    {
        std::byte data[] = {std::byte(0x48), std::byte(0x65), std::byte(0x6c), std::byte(0x6c), std::byte(0x6f)};
        auto result = simdtext::hex_encode(data);
        assert(result == "48656c6c6f");
    }
    PASS();

    TEST("hex_decode");
    {
        auto result = simdtext::hex_decode("48656c6c6f");
        assert(result.size() == 5);
        assert(reinterpret_cast<const char*>(result.data()) == std::string("Hello"));
    }
    {
        auto result = simdtext::hex_decode("deadbeef");
        assert(result.size() == 4);
        assert(result[0] == std::byte(0xDE));
        assert(result[1] == std::byte(0xAD));
        assert(result[2] == std::byte(0xBE));
        assert(result[3] == std::byte(0xEF));
    }
    PASS();

    // ── Base64 ─────────────────────────────────────
    std::cout << "\n[Base64]\n";

    TEST("base64_encode");
    {
        auto bytes = std::string("Hello");
        auto encoded = simdtext::base64_encode(
            std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));
        assert(encoded == "SGVsbG8=");
    }
    {
        auto bytes = std::string("Man");
        auto encoded = simdtext::base64_encode(
            std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));
        assert(encoded == "TWFu");
    }
    PASS();

    TEST("base64_decode");
    {
        auto result = simdtext::base64_decode("SGVsbG8=");
        std::string str(reinterpret_cast<const char*>(result.data()), result.size());
        assert(str == "Hello");
    }
    {
        auto result = simdtext::base64_decode("TWFu");
        assert(result.size() == 3);
        std::string str(reinterpret_cast<const char*>(result.data()), result.size());
        assert(str == "Man");
    }
    PASS();

    // ── URL ────────────────────────────────────────
    std::cout << "\n[URL]\n";

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
        auto params = simdtext::parse_query("name=tony&age=18");
        assert(params["name"] == "tony");
        assert(params["age"] == "18");
    }
    {
        auto params = simdtext::parse_query("q=hello%20world&lang=c%2B%2B");
        assert(params["q"] == "hello world");
        assert(params["lang"] == "c++");
    }
    PASS();

    // ── UTF-8 ──────────────────────────────────────
    std::cout << "\n[UTF-8]\n";

    TEST("valid_utf8");
    assert(simdtext::valid_utf8("hello") == true);
    assert(simdtext::valid_utf8("caf\xc3\xa9") == true);
    assert(simdtext::valid_utf8("\xff") == false);
    PASS();

    // ── File I/O ───────────────────────────────────
    std::cout << "\n[File I/O]\n";

    TEST("MappedFile + FileScanner");
    {
        // Create a test file
        const char* path = "/tmp/simdtext_test.log";
        std::ofstream f(path, std::ios::binary);
        f << "INFO started\nERROR timeout\nWARNING memory\nERROR disk\n";
        f.close();

        simdtext::FileScanner scanner(path);
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

    std::cout << "\n=== All tests passed! ===\n";
    return 0;
}

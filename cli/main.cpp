#include <simdtext/simdtext.hpp>
#include <format>
#include <iostream>
#include <string>
#include <chrono>
#include <filesystem>
#include <array>

static void print_usage() {
    std::cout << std::format(R"(simdtext — high-performance text utilities

Usage:
  simdtext stats <file>              Show file statistics
  simdtext grep <file> <pattern>     Find lines containing pattern
  simdtext count <file> <byte>       Count occurrences of a byte
  simdtext lower <file>              Lowercase file to stdout
  simdtext upper <file>              Uppercase file to stdout
  simdtext hex-encode <file>         Hex-encode file to stdout
  simdtext base64-encode <file>      Base64-encode file to stdout
  simdtext url-decode <string>       URL-decode a string
  simdtext url-encode <string>       URL-encode a string
  simdtext validate-utf8 <file>      Validate UTF-8 encoding
)");
}

static std::string format_size(size_t bytes) {
    constexpr std::array units = {"B", "KB", "MB", "GB", "TB"};
    int u = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024 && u < 4) { size /= 1024; ++u; }
    return std::format("{:.1f} {}", size, units[u]);
}

static std::string format_rate(size_t bytes, double seconds) {
    if (seconds <= 0) return "inf GB/s";
    const double gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    return std::format("{:.1f} GB/s", gb / seconds);
}

int cmd_stats(const char* path) {
    const auto start = std::chrono::high_resolution_clock::now();

    const simdtext::FileScanner scanner(path);
    if (!scanner.is_open()) {
        std::cerr << std::format("Cannot open: {}\n", path);
        return 1;
    }

    const simdtext::MappedFile mf(path);
    const auto view = mf.view();

    const size_t line_count = scanner.count_lines();
    const bool ascii = simdtext::is_ascii(view);
    const bool utf8 = simdtext::valid_utf8(view);

    const auto end = std::chrono::high_resolution_clock::now();
    const double elapsed = std::chrono::duration<double>(end - start).count();

    std::cout << std::format("File: {}\n", path);
    std::cout << std::format("Size: {}\n", format_size(view.size()));
    std::cout << std::format("Lines: {}\n", line_count);
    std::cout << std::format("ASCII: {}\n", ascii ? "yes" : "no");
    std::cout << std::format("UTF-8 valid: {}\n", utf8 ? "yes" : "no");
    std::cout << std::format("Scan speed: {}\n", format_rate(view.size(), elapsed));

    return 0;
}

int cmd_grep(const char* path, const char* pattern) {
    const simdtext::FileScanner scanner(path);
    if (!scanner.is_open()) {
        std::cerr << std::format("Cannot open: {}\n", path);
        return 1;
    }

    scanner.each_line_containing(pattern, [](std::string_view line) {
        std::cout << std::format("{}\n", line);
    });

    return 0;
}

int cmd_count(const char* path, const char byte_char) {
    const simdtext::MappedFile mf(path);
    if (mf.size() == 0 && mf.view().data() == nullptr) {
        std::cerr << std::format("Cannot open: {}\n", path);
        return 1;
    }

    const size_t count = simdtext::count_byte(mf.view(), byte_char);
    std::cout << std::format("{}\n", count);
    return 0;
}

int cmd_lower(const char* path) {
    const simdtext::MappedFile mf(path);
    if (mf.size() == 0 && mf.view().data() == nullptr) {
        std::cerr << std::format("Cannot open: {}\n", path);
        return 1;
    }

    std::string data(mf.view());
    simdtext::lowercase_ascii_inplace(data);
    std::cout << data;
    return 0;
}

int cmd_upper(const char* path) {
    const simdtext::MappedFile mf(path);
    if (mf.size() == 0 && mf.view().data() == nullptr) {
        std::cerr << std::format("Cannot open: {}\n", path);
        return 1;
    }

    std::string data(mf.view());
    simdtext::uppercase_ascii_inplace(data);
    std::cout << data;
    return 0;
}

int cmd_hex_encode(const char* path) {
    const simdtext::MappedFile mf(path);
    if (mf.size() == 0 && mf.view().data() == nullptr) {
        std::cerr << std::format("Cannot open: {}\n", path);
        return 1;
    }

    const auto encoded = simdtext::hex_encode(
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(mf.view().data()), mf.view().size()));
    std::cout << std::format("{}\n", encoded);
    return 0;
}

int cmd_base64_encode(const char* path) {
    const simdtext::MappedFile mf(path);
    if (mf.size() == 0 && mf.view().data() == nullptr) {
        std::cerr << std::format("Cannot open: {}\n", path);
        return 1;
    }

    const auto encoded = simdtext::base64_encode(
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(mf.view().data()), mf.view().size()));
    std::cout << std::format("{}\n", encoded);
    return 0;
}

int cmd_url_decode(const char* str) {
    std::cout << std::format("{}\n", simdtext::url_decode(str));
    return 0;
}

int cmd_url_encode(const char* str) {
    std::cout << std::format("{}\n", simdtext::url_encode(str));
    return 0;
}

int cmd_validate_utf8(const char* path) {
    const simdtext::MappedFile mf(path);
    if (mf.size() == 0 && mf.view().data() == nullptr) {
        std::cerr << std::format("Cannot open: {}\n", path);
        return 1;
    }

    const bool valid = simdtext::valid_utf8(mf.view());
    std::cout << std::format("{}\n", valid ? "valid" : "invalid");
    return valid ? 0 : 1;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    const std::string_view cmd = argv[1];

    if (cmd == "stats" && argc >= 3) return cmd_stats(argv[2]);
    if (cmd == "grep" && argc >= 4) return cmd_grep(argv[2], argv[3]);
    if (cmd == "count" && argc >= 4) return cmd_count(argv[2], argv[3][0]);
    if (cmd == "lower" && argc >= 3) return cmd_lower(argv[2]);
    if (cmd == "upper" && argc >= 3) return cmd_upper(argv[2]);
    if (cmd == "hex-encode" && argc >= 3) return cmd_hex_encode(argv[2]);
    if (cmd == "base64-encode" && argc >= 3) return cmd_base64_encode(argv[2]);
    if (cmd == "url-decode" && argc >= 3) return cmd_url_decode(argv[2]);
    if (cmd == "url-encode" && argc >= 3) return cmd_url_encode(argv[2]);
    if (cmd == "validate-utf8" && argc >= 3) return cmd_validate_utf8(argv[2]);

    std::cerr << std::format("Unknown command or missing arguments: {}\n", cmd);
    print_usage();
    return 1;
}

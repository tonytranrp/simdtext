#include <simdtext/simdtext.hpp>
#include <iostream>
#include <string>
#include <chrono>

static void print_usage() {
    std::cout << R"(simdtext — high-performance text utilities

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
)" << std::flush;
}

static std::string format_size(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int u = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024 && u < 4) { size /= 1024; u++; }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.1f %s", size, units[u]);
    return buf;
}

static std::string format_rate(size_t bytes, double seconds) {
    if (seconds <= 0) return "inf GB/s";
    double gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    char buf[64];
    snprintf(buf, sizeof(buf), "%.1f GB/s", gb / seconds);
    return buf;
}

int cmd_stats(const char* path) {
    auto start = std::chrono::high_resolution_clock::now();

    simdtext::FileScanner scanner(path);
    if (!scanner.is_open()) {
        std::cerr << "Cannot open: " << path << "\n";
        return 1;
    }

    simdtext::MappedFile mf(path);
    auto view = mf.view();

    size_t lines = scanner.count_lines();
    bool ascii = simdtext::is_ascii(view);
    bool utf8 = simdtext::valid_utf8(view);

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();

    std::cout << "File: " << path << "\n";
    std::cout << "Size: " << format_size(view.size()) << "\n";
    std::cout << "Lines: " << lines << "\n";
    std::cout << "ASCII: " << (ascii ? "yes" : "no") << "\n";
    std::cout << "UTF-8 valid: " << (utf8 ? "yes" : "no") << "\n";
    std::cout << "Scan speed: " << format_rate(view.size(), elapsed) << "\n";

    return 0;
}

int cmd_grep(const char* path, const char* pattern) {
    simdtext::FileScanner scanner(path);
    if (!scanner.is_open()) {
        std::cerr << "Cannot open: " << path << "\n";
        return 1;
    }

    scanner.each_line_containing(pattern, [](std::string_view line) {
        std::cout << line << "\n";
    });

    return 0;
}

int cmd_count(const char* path, const char byte_char) {
    simdtext::MappedFile mf(path);
    if (mf.size() == 0 && mf.view().data() == nullptr) {
        std::cerr << "Cannot open: " << path << "\n";
        return 1;
    }

    size_t count = simdtext::count_byte(mf.view(), byte_char);
    std::cout << count << "\n";
    return 0;
}

int cmd_lower(const char* path) {
    simdtext::MappedFile mf(path);
    if (mf.size() == 0 && mf.view().data() == nullptr) {
        std::cerr << "Cannot open: " << path << "\n";
        return 1;
    }

    // Need mutable copy since mmap is read-only
    std::string data(mf.view());
    simdtext::lowercase_ascii_inplace(data);
    std::cout << data;
    return 0;
}

int cmd_upper(const char* path) {
    simdtext::MappedFile mf(path);
    if (mf.size() == 0 && mf.view().data() == nullptr) {
        std::cerr << "Cannot open: " << path << "\n";
        return 1;
    }

    std::string data(mf.view());
    simdtext::uppercase_ascii_inplace(data);
    std::cout << data;
    return 0;
}

int cmd_hex_encode(const char* path) {
    simdtext::MappedFile mf(path);
    if (mf.size() == 0 && mf.view().data() == nullptr) {
        std::cerr << "Cannot open: " << path << "\n";
        return 1;
    }

    auto encoded = simdtext::hex_encode(
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(mf.view().data()), mf.view().size()));
    std::cout << encoded << "\n";
    return 0;
}

int cmd_base64_encode(const char* path) {
    simdtext::MappedFile mf(path);
    if (mf.size() == 0 && mf.view().data() == nullptr) {
        std::cerr << "Cannot open: " << path << "\n";
        return 1;
    }

    auto encoded = simdtext::base64_encode(
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(mf.view().data()), mf.view().size()));
    std::cout << encoded << "\n";
    return 0;
}

int cmd_url_decode(const char* str) {
    std::cout << simdtext::url_decode(str) << "\n";
    return 0;
}

int cmd_url_encode(const char* str) {
    std::cout << simdtext::url_encode(str) << "\n";
    return 0;
}

int cmd_validate_utf8(const char* path) {
    simdtext::MappedFile mf(path);
    if (mf.size() == 0 && mf.view().data() == nullptr) {
        std::cerr << "Cannot open: " << path << "\n";
        return 1;
    }

    bool valid = simdtext::valid_utf8(mf.view());
    std::cout << (valid ? "valid" : "invalid") << "\n";
    return valid ? 0 : 1;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string cmd = argv[1];

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

    std::cerr << "Unknown command or missing arguments: " << cmd << "\n";
    print_usage();
    return 1;
}

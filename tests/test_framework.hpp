#pragma once
#include <iostream>
#include <string>
#include <source_location>
#include <cstdio>

namespace test {

struct Stats {
    int passed = 0;
    int failed = 0;
};

inline Stats g_stats;

inline void check(bool condition, const std::string& expr, const std::source_location& loc = std::source_location::current()) {
    if (condition) {
        g_stats.passed++;
    } else {
        g_stats.failed++;
        std::fprintf(stderr, "  FAIL %s:%d: %s\n", loc.file_name(), loc.line(), expr.c_str());
    }
}

#define CHECK(expr) test::check((expr), #expr)
#define CHECK_EQ(a, b) test::check((a) == (b), #a " == " #b)
#define CHECK_NE(a, b) test::check((a) != (b), #a " != " #b)

} // namespace test

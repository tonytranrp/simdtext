#include "test_framework.hpp"

void test_scan();
void test_ascii();
void test_lines();
void test_encode();
void test_url();
void test_utf8();
void test_file();

int main() {
    test_scan();
    test_ascii();
    test_lines();
    test_encode();
    test_url();
    test_utf8();
    test_file();

    std::printf("\n%d passed, %d failed\n", test::g_stats.passed, test::g_stats.failed);
    return test::g_stats.failed > 0 ? 1 : 0;
}

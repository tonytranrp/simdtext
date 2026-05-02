#include "test_framework.hpp"

void test_scan();
void test_ascii();
void test_lines();
void test_encode();
void test_url();
void test_utf8();
void test_file();
void test_pattern();
void test_parallel();
void test_str();
void test_hash();
void test_json();
void test_csv();
void test_diff();
void test_log();
void test_xml();
void test_markdown();

int main() {
    test_scan();
    test_ascii();
    test_lines();
    test_encode();
    test_url();
    test_utf8();
    test_file();
    test_pattern();
    test_parallel();
    test_str();
    test_hash();
    test_json();
    test_csv();
    test_diff();
    test_log();
    test_xml();
    test_markdown();

    std::printf("\n%d passed, %d failed\n", test::g_stats.passed, test::g_stats.failed);
    return test::g_stats.failed > 0 ? 1 : 0;
}

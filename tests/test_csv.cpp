// test_csv.cpp — Tests for CSV parser
#include "test_framework.hpp"
#include <simdtext/csv.hpp>
#include <string>
#include <vector>

using namespace simdtext;

void test_csv() {
    std::printf("[csv]\n");

    // Simple CSV
    {
        CsvParser parser("name,age,city\nAlice,30,NYC\nBob,25,LA\n");
        CHECK(parser.next_row());
        CHECK_EQ(parser.field_count(), 3u);
        CHECK_EQ(parser.field(0), std::string_view("name"));
        CHECK_EQ(parser.field(1), std::string_view("age"));
        CHECK_EQ(parser.field(2), std::string_view("city"));

        CHECK(parser.next_row());
        CHECK_EQ(parser.field(0), std::string_view("Alice"));
        CHECK_EQ(parser.field(1), std::string_view("30"));
        CHECK_EQ(parser.field(2), std::string_view("NYC"));

        CHECK(parser.next_row());
        CHECK_EQ(parser.field(0), std::string_view("Bob"));
        CHECK_EQ(parser.row_number(), 3u);

        CHECK(!parser.next_row());
    }

    // Quoted fields
    {
        CsvParser parser("\"hello, world\",42\n");
        CHECK(parser.next_row());
        CHECK_EQ(parser.field_count(), 2u);
        CHECK_EQ(parser.field(0), std::string_view("hello, world"));
        CHECK_EQ(parser.field(1), std::string_view("42"));
    }

    // Escaped quotes (stored as-is in zero-allocation mode)
    {
        CsvParser parser("\"say \"\"hi\"\"\",x\n");
        CHECK(parser.next_row());
        CHECK_EQ(parser.field_count(), 2u);
        CHECK_EQ(parser.field(0), std::string_view("say \"\"hi\"\""));  // double-quote preserved
    }

    // Empty fields
    {
        CsvParser parser("a,,c\n");
        CHECK(parser.next_row());
        CHECK_EQ(parser.field_count(), 3u);
        CHECK_EQ(parser.field(0), std::string_view("a"));
        CHECK_EQ(parser.field(1), std::string_view(""));
        CHECK_EQ(parser.field(2), std::string_view("c"));
    }

    // CRLF line endings
    {
        CsvParser parser("a,b\r\nc,d\r\n");
        CHECK(parser.next_row());
        CHECK_EQ(parser.field(0), std::string_view("a"));
        CHECK(parser.next_row());
        CHECK_EQ(parser.field(0), std::string_view("c"));
        CHECK(!parser.next_row());
    }

    // Single row, no trailing newline
    {
        CsvParser parser("1,2,3");
        CHECK(parser.next_row());
        CHECK_EQ(parser.field_count(), 3u);
        CHECK_EQ(parser.field(2), std::string_view("3"));
        CHECK(!parser.next_row());
    }

    // Custom delimiter
    {
        CsvOptions opts;
        opts.delimiter = '|';
        CsvParser parser("a|b|c\n", opts);
        CHECK(parser.next_row());
        CHECK_EQ(parser.field_count(), 3u);
        CHECK_EQ(parser.field(1), std::string_view("b"));
    }

    // parse_csv_row
    {
        std::vector<std::string_view> fields;
        size_t n = parse_csv_row("x,y,z", fields);
        CHECK_EQ(n, 3u);
        CHECK_EQ(fields[0], std::string_view("x"));
    }

    // Empty input
    {
        CsvParser parser("");
        CHECK(!parser.next_row());
    }

    // Row numbers
    {
        CsvParser parser("a\nb\nc\n");
        CHECK(parser.next_row());
        CHECK_EQ(parser.row_number(), 1u);
        CHECK(parser.next_row());
        CHECK_EQ(parser.row_number(), 2u);
        CHECK(parser.next_row());
        CHECK_EQ(parser.row_number(), 3u);
    }
}

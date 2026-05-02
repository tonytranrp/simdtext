// test_log.cpp — Tests for log parser
#include "test_framework.hpp"
#include <simdtext/log.hpp>

using namespace simdtext;

void test_log() {
    std::printf("[log]\n");

    // parse_log_level
    {
        CHECK_EQ(parse_log_level("INFO"), LogLevel::Info);
        CHECK_EQ(parse_log_level("WARN"), LogLevel::Warn);
        CHECK_EQ(parse_log_level("ERROR"), LogLevel::Error);
        CHECK_EQ(parse_log_level("FATAL"), LogLevel::Fatal);
        CHECK_EQ(parse_log_level("DEBUG"), LogLevel::Debug);
        CHECK_EQ(parse_log_level("TRACE"), LogLevel::Trace);
        CHECK_EQ(parse_log_level("unknown"), LogLevel::Unknown);
    }

    // parse_log_line — simple format
    {
        auto entry = parse_log_line("2024-01-15 03:21:44 ERROR connection timeout");
        CHECK_EQ(entry.level, std::string_view("ERROR"));
        CHECK_EQ(entry.message, std::string_view("connection timeout"));
    }

    // parse_log_line — bracket timestamp
    {
        auto entry = parse_log_line("[2024-01-15T03:21:44] INFO server started");
        CHECK_EQ(entry.timestamp, std::string_view("2024-01-15T03:21:44"));
        CHECK_EQ(entry.level, std::string_view("INFO"));
        CHECK_EQ(entry.message, std::string_view("server started"));
    }

    // parse_log_line — WARN
    {
        auto entry = parse_log_line("2024-01-15 WARN low memory");
        CHECK_EQ(parse_log_level(entry.level), LogLevel::Warn);
    }

    // parse_log_line — line numbers
    {
        auto entry = parse_log_line("INFO test", 42, 1000);
        CHECK_EQ(entry.line_number, 42u);
        CHECK_EQ(entry.byte_offset, 1000u);
    }

    // count_log_levels
    {
        const char* log = 
            "2024-01-15 INFO server started\n"
            "2024-01-15 ERROR disk full\n"
            "2024-01-15 WARN low memory\n"
            "2024-01-15 INFO request handled\n"
            "2024-01-15 FATAL system crash\n";
        auto counts = count_log_levels(log);
        CHECK_EQ(counts.info, 2u);
        CHECK_EQ(counts.error, 1u);
        CHECK_EQ(counts.warn, 1u);
        CHECK_EQ(counts.fatal, 1u);
        CHECK_EQ(counts.total, 5u);
    }

    // filter_log_lines
    {
        const char* log = 
            "2024-01-15 INFO server started\n"
            "2024-01-15 ERROR disk full\n"
            "2024-01-15 WARN low memory\n";
        auto errors = filter_log_lines(log, LogLevel::Error);
        CHECK_EQ(errors.size(), 1u);  // only ERROR line
    }

    // Empty log
    {
        auto counts = count_log_levels("");
        CHECK_EQ(counts.total, 0u);
    }
}

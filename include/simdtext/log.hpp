#pragma once

/// @file log.hpp
/// @brief Fast log file parsing and analysis.

#include "export.hpp"
#include <cstdint>
#include <string_view>
#include <vector>

namespace simdtext {

/// Log level filter.
enum class LogLevel : uint8_t {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
    Unknown,
};

/// A parsed log entry.
struct LogEntry {
    std::string_view timestamp;   // Timestamp portion
    std::string_view level;       // Log level (INFO, WARN, ERROR, etc.)
    std::string_view message;     // Message body
    size_t line_number;           // 1-based line number
    size_t byte_offset;           // Byte offset in original input
};

/// Parse a log level string into a LogLevel enum.
SIMDTEXT_NODISCARD SIMDTEXT_API LogLevel parse_log_level(std::string_view level) noexcept;

/// Parse a single log line into components.
/// Supports common formats: "TIMESTAMP LEVEL MESSAGE"
SIMDTEXT_NODISCARD SIMDTEXT_API LogEntry parse_log_line(std::string_view line, size_t line_number = 0, size_t byte_offset = 0) noexcept;

/// Count log entries by level.
struct SIMDTEXT_API LogCounts {
    size_t trace = 0;
    size_t debug = 0;
    size_t info = 0;
    size_t warn = 0;
    size_t error = 0;
    size_t fatal = 0;
    size_t unknown = 0;
    size_t total = 0;
};

/// Count log entries by level in a multi-line text.
SIMDTEXT_NODISCARD SIMDTEXT_API LogCounts count_log_levels(std::string_view text);

/// Extract lines matching a log level or higher.
/// Returns string_views into the original text.
SIMDTEXT_NODISCARD SIMDTEXT_API std::vector<std::string_view> filter_log_lines(
    std::string_view text, LogLevel min_level);

} // namespace simdtext

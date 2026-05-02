#include "simdtext/log.hpp"
#include "simdtext/lines.hpp"
#include "simdtext/str.hpp"

namespace simdtext {

LogLevel parse_log_level(std::string_view level) noexcept {
    if (level == "TRACE" || level == "trace") return LogLevel::Trace;
    if (level == "DEBUG" || level == "debug") return LogLevel::Debug;
    if (level == "INFO"  || level == "info")  return LogLevel::Info;
    if (level == "WARN"  || level == "warn"  || level == "WARNING" || level == "warning") return LogLevel::Warn;
    if (level == "ERROR" || level == "error") return LogLevel::Error;
    if (level == "FATAL" || level == "fatal") return LogLevel::Fatal;
    return LogLevel::Unknown;
}

LogEntry parse_log_line(std::string_view line, size_t line_number, size_t byte_offset) noexcept {
    LogEntry entry;
    entry.line_number = line_number;
    entry.byte_offset = byte_offset;

    if (line.empty()) return entry;

    size_t pos = 0;

    // Skip leading whitespace
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) ++pos;
    if (pos >= line.size()) { entry.message = line; return entry; }

    // Optional bracket: [timestamp]
    if (line[pos] == '[') {
        size_t end = pos + 1;
        while (end < line.size() && line[end] != ']') ++end;
        if (end < line.size()) {
            entry.timestamp = line.substr(pos + 1, end - pos - 1);
            pos = end + 1;
        } else {
            // No closing bracket — treat rest as message
            entry.message = line;
            return entry;
        }
    } else {
        // Timestamp: scan until we find a log level keyword
        // Try to find a known level word
        static constexpr std::string_view levels[] = {"TRACE", "DEBUG", "INFO", "WARN", "WARNING", "ERROR", "FATAL",
                                                       "trace", "debug", "info", "warn", "warning", "error", "fatal"};
        size_t found_pos = line.size();
        size_t found_len = 0; (void)found_len;
        for (auto lv : levels) {
            size_t idx = line.find(lv, pos);
            if (idx != std::string_view::npos && idx < found_pos) {
                // Verify it's a standalone word
                bool word_start = (idx == 0 || line[idx-1] == ' ' || line[idx-1] == '\t');
                bool word_end = (idx + lv.size() >= line.size() || line[idx + lv.size()] == ' ' || line[idx + lv.size()] == ':');
                if (word_start && word_end) {
                    found_pos = idx;
                    found_len = lv.size();
                }
            }
        }
        if (found_pos < line.size()) {
            entry.timestamp = line.substr(pos, found_pos - pos);
            // Trim trailing space from timestamp
            while (!entry.timestamp.empty() && (entry.timestamp.back() == ' ' || entry.timestamp.back() == '\t'))
                entry.timestamp = entry.timestamp.substr(0, entry.timestamp.size() - 1);
            pos = found_pos;
        } else {
            // Can't find level — return whole line as message
            entry.message = line;
            return entry;
        }
    }

    // Skip whitespace before level
    while (pos < line.size() && line[pos] == ' ') ++pos;

    // Parse level
    size_t level_start = pos;
    while (pos < line.size() && line[pos] != ' ' && line[pos] != ':') ++pos;
    entry.level = line.substr(level_start, pos - level_start);

    // Skip whitespace/colon after level
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == ':' || line[pos] == '\t')) ++pos;

    // Rest is message
    entry.message = line.substr(pos);
    return entry;
}

LogCounts count_log_levels(std::string_view text) {
    LogCounts counts;
    for (auto line : LineView(text)) {
        auto entry = parse_log_line(line);
        switch (parse_log_level(entry.level)) {
            case LogLevel::Trace:  ++counts.trace;  break;
            case LogLevel::Debug:  ++counts.debug;  break;
            case LogLevel::Info:   ++counts.info;   break;
            case LogLevel::Warn:   ++counts.warn;   break;
            case LogLevel::Error:  ++counts.error;  break;
            case LogLevel::Fatal:  ++counts.fatal;  break;
            case LogLevel::Unknown: ++counts.unknown; break;
        }
        ++counts.total;
    }
    return counts;
}

std::vector<std::string_view> filter_log_lines(std::string_view text, LogLevel min_level) {
    std::vector<std::string_view> result;
    static const LogLevel levels[] __attribute__((unused)) = {
        LogLevel::Trace, LogLevel::Debug, LogLevel::Info,
        LogLevel::Warn, LogLevel::Error, LogLevel::Fatal
    };
    for (auto line : LineView(text)) {
        auto entry = parse_log_line(line);
        LogLevel level = parse_log_level(entry.level);
        if (level >= min_level) {
            result.push_back(line);
        }
    }
    return result;
}

} // namespace simdtext

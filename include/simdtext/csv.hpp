#pragma once

/// @file csv.hpp
/// @brief Zero-allocation CSV parsing.

#include "export.hpp"
#include <cstddef>
#include <string_view>
#include <vector>

namespace simdtext {

/// CSV parsing options.
struct CsvOptions {
    char delimiter = ',';
    char quote = '"';
    bool strict = true;  // Reject malformed quoting
};

/// Zero-allocation CSV row iterator.
/// Usage:
///   CsvParser parser(csv_data);
///   while (parser.next_row()) {
///       for (size_t i = 0; i < parser.field_count(); i++) {
///           auto field = parser.field(i);
///       }
///   }
class SIMDTEXT_API CsvParser {
public:
    explicit CsvParser(std::string_view input, CsvOptions opts = {}) noexcept;

    /// Advance to the next row. Returns false at end of input.
    SIMDTEXT_NODISCARD bool next_row() noexcept;

    /// Number of fields in the current row.
    SIMDTEXT_NODISCARD size_t field_count() const noexcept { return fields_.size(); }

    /// Get a field by index.
    SIMDTEXT_NODISCARD std::string_view field(size_t index) const noexcept;

    /// Get all fields in the current row.
    SIMDTEXT_NODISCARD const std::vector<std::string_view>& fields() const noexcept { return fields_; }

    /// Current row number (1-based).
    SIMDTEXT_NODISCARD size_t row_number() const noexcept { return row_; }

    /// Reset the parser.
    void reset(std::string_view input) noexcept;

private:
    std::string_view input_;
    size_t pos_;
    CsvOptions opts_;
    std::vector<std::string_view> fields_;
    size_t row_;

    void skip_whitespace() noexcept;
};

/// Parse a single CSV row into fields. Returns the number of fields parsed.
SIMDTEXT_NODISCARD SIMDTEXT_API size_t parse_csv_row(
    std::string_view line, std::vector<std::string_view>& fields,
    char delimiter = ',', char quote = '"');

} // namespace simdtext

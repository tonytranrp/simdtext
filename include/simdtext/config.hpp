#pragma once

/// @file config.hpp
/// @brief INI-style config file parsing.

#include "export.hpp"
#include <cstddef>
#include <string_view>
#include <vector>

namespace simdtext {

/// A config entry (key-value pair).
struct ConfigEntry {
    std::string_view section;   // [section] name (empty for global)
    std::string_view key;       // key name
    std::string_view value;     // value (unquoted if quoted)
    size_t line;                // 1-based line number
};

/// Zero-allocation INI-style config parser.
class SIMDTEXT_API ConfigParser {
public:
    explicit ConfigParser(std::string_view input) noexcept;

    /// Parse all entries. Returns the number of entries found.
    SIMDTEXT_NODISCARD size_t parse() noexcept;

    /// Number of parsed entries.
    SIMDTEXT_NODISCARD size_t size() const noexcept { return entries_.size(); }

    /// Get an entry by index.
    SIMDTEXT_NODISCARD const ConfigEntry& entry(size_t index) const noexcept;

    /// Get all entries.
    SIMDTEXT_NODISCARD const std::vector<ConfigEntry>& entries() const noexcept { return entries_; }

    /// Find the first entry matching section and key.
    SIMDTEXT_NODISCARD std::string_view get(std::string_view section, std::string_view key,
                                            std::string_view default_val = {}) const noexcept;

    /// Reset the parser.
    void reset(std::string_view input) noexcept;

private:
    std::string_view input_;
    std::vector<ConfigEntry> entries_;
    std::string_view current_section_;
};

} // namespace simdtext

#include "simdtext/config.hpp"
#include "simdtext/lines.hpp"
#include "simdtext/str.hpp"

namespace simdtext {

ConfigParser::ConfigParser(std::string_view input) noexcept
    : input_(input), entries_(), current_section_() {}

size_t ConfigParser::parse() noexcept {
    entries_.clear();
    current_section_ = {};
    size_t line_num = 0;

    for (auto line_view : LineView(input_)) {
        ++line_num;
        auto line = trim(line_view);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        // Section: [name]
        if (line.size() >= 2 && line[0] == '[' && line.back() == ']') {
            current_section_ = line.substr(1, line.size() - 2);
            continue;
        }

        // Key = value
        size_t eq = line.find('=');
        if (eq == std::string_view::npos) continue;

        auto key = trim(line.substr(0, eq));
        auto val = trim(line.substr(eq + 1));

        // Strip quotes from value
        if (val.size() >= 2 && ((val[0] == '"' && val.back() == '"') ||
                                 (val[0] == '\'' && val.back() == '\''))) {
            val = val.substr(1, val.size() - 2);
        }

        entries_.push_back({current_section_, key, val, line_num});
    }

    return entries_.size();
}

const ConfigEntry& ConfigParser::entry(size_t index) const noexcept {
    static const ConfigEntry empty = {};
    return index < entries_.size() ? entries_[index] : empty;
}

std::string_view ConfigParser::get(std::string_view section, std::string_view key,
                                    std::string_view default_val) const noexcept {
    for (const auto& e : entries_) {
        if (e.section == section && e.key == key) return e.value;
    }
    return default_val;
}

void ConfigParser::reset(std::string_view input) noexcept {
    input_ = input;
    entries_.clear();
    current_section_ = {};
}

} // namespace simdtext

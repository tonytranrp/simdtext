#include "simdtext/csv.hpp"

namespace simdtext {

CsvParser::CsvParser(std::string_view input, CsvOptions opts) noexcept
    : input_(input), pos_(0), opts_(opts), fields_(), row_(0) {}

bool CsvParser::next_row() noexcept {
    if (pos_ >= input_.size()) return false;

    fields_.clear();
    ++row_;
    size_t field_start = pos_;

    while (pos_ < input_.size()) {
        char c = input_[pos_];

        if (c == opts_.quote) {
            // Quoted field — scan to closing quote, handling escaped quotes
            ++pos_; // skip opening quote
            field_start = pos_;
            while (pos_ < input_.size()) {
                if (input_[pos_] == opts_.quote) {
                    // Check for escaped quote (double quote)
                    if (pos_ + 1 < input_.size() && input_[pos_ + 1] == opts_.quote) {
                        pos_ += 2;
                        continue;
                    }
                    // End of quoted field
                    std::string_view field_val = input_.substr(field_start, pos_ - field_start);
                    fields_.push_back(field_val);
                    ++pos_; // skip closing quote
                    // Next char should be delimiter, newline, or end
                    if (pos_ < input_.size()) {
                        char next = input_[pos_];
                        if (next == opts_.delimiter) {
                            ++pos_;
                            field_start = pos_;
                        } else if (next == '\n') {
                            ++pos_;
                            return true;
                        } else if (next == '\r' && pos_ + 1 < input_.size() && input_[pos_+1] == '\n') {
                            pos_ += 2;
                            return true;
                        }
                    }
                    break;
                }
                ++pos_;
            }
        } else if (c == opts_.delimiter) {
            // End of unquoted field
            fields_.push_back(input_.substr(field_start, pos_ - field_start));
            ++pos_;
            field_start = pos_;
        } else if (c == '\n') {
            // End of row
            fields_.push_back(input_.substr(field_start, pos_ - field_start));
            ++pos_;
            return true;
        } else if (c == '\r' && pos_ + 1 < input_.size() && input_[pos_+1] == '\n') {
            fields_.push_back(input_.substr(field_start, pos_ - field_start));
            pos_ += 2;
            return true;
        } else {
            ++pos_;
        }
    }

    // Last field (no trailing newline)
    if (field_start <= pos_) {
        fields_.push_back(input_.substr(field_start, pos_ - field_start));
    }
    return !fields_.empty();
}

std::string_view CsvParser::field(size_t index) const noexcept {
    return index < fields_.size() ? fields_[index] : std::string_view();
}

void CsvParser::reset(std::string_view input) noexcept {
    input_ = input;
    pos_ = 0;
    fields_.clear();
    row_ = 0;
}

size_t parse_csv_row(std::string_view line, std::vector<std::string_view>& fields,
    char delimiter, char quote) {
    CsvParser parser(line, {delimiter, quote, true});
    (void)parser.next_row();
    fields = parser.fields();
    return fields.size();
}

} // namespace simdtext

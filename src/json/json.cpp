#include "simdtext/json.hpp"
#include <cstring>

namespace simdtext {

JsonTokenizer::JsonTokenizer(std::string_view input) noexcept
    : input_(input), pos_(0), token_() {}

void JsonTokenizer::skip_whitespace() noexcept {
    while (pos_ < input_.size()) {
        char c = input_[pos_];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            ++pos_;
        } else {
            break;
        }
    }
}

char JsonTokenizer::peek() const noexcept {
    return pos_ < input_.size() ? input_[pos_] : '\0';
}

char JsonTokenizer::advance() noexcept {
    return pos_ < input_.size() ? input_[pos_++] : '\0';
}

const JsonToken& JsonTokenizer::next() noexcept {
    skip_whitespace();
    if (pos_ >= input_.size()) {
        token_ = {JsonType::End, {}, pos_};
        return token_;
    }

    const size_t start = pos_;
    char c = advance();

    switch (c) {
        case '{':
            token_ = {JsonType::ObjectOpen, input_.substr(start, 1), start};
            return token_;
        case '}':
            token_ = {JsonType::ObjectClose, input_.substr(start, 1), start};
            return token_;
        case '[':
            token_ = {JsonType::ArrayOpen, input_.substr(start, 1), start};
            return token_;
        case ']':
            token_ = {JsonType::ArrayClose, input_.substr(start, 1), start};
            return token_;
        case ':':
            token_ = {JsonType::Colon, input_.substr(start, 1), start};
            return token_;
        case ',':
            token_ = {JsonType::Comma, input_.substr(start, 1), start};
            return token_;

        case '"': {
            // Scan string, handling escapes
            size_t str_start = start; // include opening quote
            while (pos_ < input_.size()) {
                char sc = input_[pos_++];
                if (sc == '\\') {
                    if (pos_ < input_.size()) ++pos_; // skip escaped char
                } else if (sc == '"') {
                    token_ = {JsonType::String, input_.substr(str_start, pos_ - str_start), str_start};
                    return token_;
                }
            }
            // Unterminated string
            token_ = {JsonType::Error, input_.substr(str_start), str_start};
            return token_;
        }

        case 't': {
            // true
            if (pos_ + 2 < input_.size() && input_[pos_] == 'r' && input_[pos_+1] == 'u' && input_[pos_+2] == 'e') {
                pos_ += 3;
                token_ = {JsonType::True, input_.substr(start, 4), start};
                return token_;
            }
            token_ = {JsonType::Error, input_.substr(start), start};
            return token_;
        }

        case 'f': {
            // false
            if (pos_ + 3 < input_.size() && input_[pos_] == 'a' && input_[pos_+1] == 'l' && input_[pos_+2] == 's' && input_[pos_+3] == 'e') {
                pos_ += 4;
                token_ = {JsonType::False, input_.substr(start, 5), start};
                return token_;
            }
            token_ = {JsonType::Error, input_.substr(start), start};
            return token_;
        }

        case 'n': {
            // null
            if (pos_ + 2 < input_.size() && input_[pos_] == 'u' && input_[pos_+1] == 'l' && input_[pos_+2] == 'l') {
                pos_ += 3;
                token_ = {JsonType::Null, input_.substr(start, 4), start};
                return token_;
            }
            token_ = {JsonType::Error, input_.substr(start), start};
            return token_;
        }

        default: {
            // Number: -?[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?
            if (c == '-' || (c >= '0' && c <= '9')) {
                if (c == '-' && (pos_ >= input_.size() || input_[pos_] < '0' || input_[pos_] > '9')) {
                    token_ = {JsonType::Error, input_.substr(start), start};
                    return token_;
                }
                // Integer part
                while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') ++pos_;
                // Fractional part
                if (pos_ < input_.size() && input_[pos_] == '.') {
                    ++pos_;
                    if (pos_ >= input_.size() || input_[pos_] < '0' || input_[pos_] > '9') {
                        token_ = {JsonType::Error, input_.substr(start), start};
                        return token_;
                    }
                    while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') ++pos_;
                }
                // Exponent
                if (pos_ < input_.size() && (input_[pos_] == 'e' || input_[pos_] == 'E')) {
                    ++pos_;
                    if (pos_ < input_.size() && (input_[pos_] == '+' || input_[pos_] == '-')) ++pos_;
                    if (pos_ >= input_.size() || input_[pos_] < '0' || input_[pos_] > '9') {
                        token_ = {JsonType::Error, input_.substr(start), start};
                        return token_;
                    }
                    while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') ++pos_;
                }
                token_ = {JsonType::Number, input_.substr(start, pos_ - start), start};
                return token_;
            }
            // Unknown character
            token_ = {JsonType::Error, input_.substr(start, 1), start};
            return token_;
        }
    }
}

void JsonTokenizer::reset(std::string_view input) noexcept {
    input_ = input;
    pos_ = 0;
    token_ = {};
}

std::string_view JsonTokenizer::remaining() const noexcept {
    return pos_ < input_.size() ? input_.substr(pos_) : std::string_view();
}

bool looks_like_json(std::string_view input) noexcept {
    for (char c : input) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
        return c == '{' || c == '[';
    }
    return false;
}

bool is_json_number(std::string_view input) noexcept {
    if (input.empty()) return false;
    size_t i = 0;
    if (input[0] == '-') {
        if (input.size() == 1) return false;
        ++i;
    }
    if (input[i] < '0' || input[i] > '9') return false;
    while (i < input.size() && input[i] >= '0' && input[i] <= '9') ++i;
    if (i < input.size() && input[i] == '.') {
        ++i;
        if (i >= input.size() || input[i] < '0' || input[i] > '9') return false;
        while (i < input.size() && input[i] >= '0' && input[i] <= '9') ++i;
    }
    if (i < input.size() && (input[i] == 'e' || input[i] == 'E')) {
        ++i;
        if (i < input.size() && (input[i] == '+' || input[i] == '-')) ++i;
        if (i >= input.size() || input[i] < '0' || input[i] > '9') return false;
        while (i < input.size() && input[i] >= '0' && input[i] <= '9') ++i;
    }
    return i == input.size();
}

size_t json_unescape_inplace(char* data, size_t len) noexcept {
    size_t j = 0;
    for (size_t i = 0; i < len; ) {
        if (data[i] == '\\' && i + 1 < len) {
            ++i;
            switch (data[i]) {
                case '"':  data[j++] = '"';  ++i; break;
                case '\\': data[j++] = '\\'; ++i; break;
                case '/':  data[j++] = '/';  ++i; break;
                case 'b':  data[j++] = '\b'; ++i; break;
                case 'f':  data[j++] = '\f'; ++i; break;
                case 'n':  data[j++] = '\n'; ++i; break;
                case 'r':  data[j++] = '\r'; ++i; break;
                case 't':  data[j++] = '\t'; ++i; break;
                case 'u': {
                    // \uXXXX — write as-is for simplicity (full Unicode decode needs more logic)
                    data[j++] = '\\';
                    // Copy uXXXX
                    for (size_t k = 0; k < 5 && i + k < len; k++) data[j++] = data[i + k];
                    i += 5;
                    break;
                }
                default:
                    data[j++] = data[i++];
                    break;
            }
        } else {
            data[j++] = data[i++];
        }
    }
    return j;
}

} // namespace simdtext

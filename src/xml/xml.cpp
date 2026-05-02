#include "simdtext/xml.hpp"
#include <cstring>

namespace simdtext {

XmlTokenizer::XmlTokenizer(std::string_view input) noexcept
    : input_(input), pos_(0), token_() {}

void XmlTokenizer::skip_whitespace() noexcept {
    while (pos_ < input_.size()) {
        char c = input_[pos_];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos_;
        else break;
    }
}

char XmlTokenizer::peek() const noexcept {
    return pos_ < input_.size() ? input_[pos_] : '\0';
}

const XmlToken& XmlTokenizer::next() noexcept {
    skip_whitespace();
    if (pos_ >= input_.size()) {
        token_ = {XmlType::End, {}, {}, pos_};
        return token_;
    }

    const size_t start = pos_;

    if (input_[pos_] == '<') {
        ++pos_; // skip <

        if (pos_ >= input_.size()) {
            token_ = {XmlType::Error, input_.substr(start), {}, start};
            return token_;
        }

        // Comment: <!-- ... -->
        if (pos_ + 2 < input_.size() && input_[pos_] == '!' && input_[pos_+1] == '-' && input_[pos_+2] == '-') {
            pos_ += 3;
            size_t comment_start = pos_;
            while (pos_ + 2 < input_.size() && !(input_[pos_] == '-' && input_[pos_+1] == '-' && input_[pos_+2] == '>')) ++pos_;
            std::string_view comment_val = input_.substr(comment_start, pos_ - comment_start);
            pos_ += 3; // skip -->
            token_ = {XmlType::Comment, comment_val, {}, start};
            return token_;
        }

        // CDATA: <![CDATA[ ... ]]>
        if (pos_ + 6 < input_.size() && input_[pos_] == '!' && input_.substr(pos_, 8) == "![CDATA[") {
            pos_ += 8;
            size_t cdata_start = pos_;
            while (pos_ + 2 < input_.size() && !(input_[pos_] == ']' && input_[pos_+1] == ']' && input_[pos_+2] == '>')) ++pos_;
            std::string_view cdata_val = input_.substr(cdata_start, pos_ - cdata_start);
            pos_ += 3; // skip ]]>
            token_ = {XmlType::CData, cdata_val, {}, start};
            return token_;
        }

        // Declaration: <?xml ... ?>
        if (input_[pos_] == '?') {
            ++pos_;
            // Scan to ?>
            size_t name_start = pos_;
            while (pos_ < input_.size() && input_[pos_] != ' ' && input_[pos_] != '?') ++pos_;
            std::string_view name = input_.substr(name_start, pos_ - name_start);
            while (pos_ + 1 < input_.size() && !(input_[pos_] == '?' && input_[pos_+1] == '>')) ++pos_;
            pos_ += 2; // skip ?>
            token_ = {XmlType::Declaration, name, {}, start};
            return token_;
        }

        // Close tag: </tag>
        if (input_[pos_] == '/') {
            ++pos_;
            size_t name_start = pos_;
            while (pos_ < input_.size() && input_[pos_] != '>' && input_[pos_] != ' ') ++pos_;
            std::string_view name = input_.substr(name_start, pos_ - name_start);
            while (pos_ < input_.size() && input_[pos_] != '>') ++pos_;
            ++pos_; // skip >
            token_ = {XmlType::CloseTag, name, {}, start};
            return token_;
        }

        // Open tag or self-closing: <tag ...> or <tag ... />
        size_t name_start = pos_;
        while (pos_ < input_.size() && input_[pos_] != '>' && input_[pos_] != ' ' && input_[pos_] != '/' && input_[pos_] != '\t' && input_[pos_] != '\n') ++pos_;
        std::string_view name = input_.substr(name_start, pos_ - name_start);

        // Check for self-closing
        bool self_close = false;
        // Scan attributes and find > or />
        while (pos_ < input_.size()) {
            if (input_[pos_] == '>') {
                ++pos_;
                break;
            }
            if (input_[pos_] == '/' && pos_ + 1 < input_.size() && input_[pos_+1] == '>') {
                self_close = true;
                pos_ += 2;
                break;
            }
            ++pos_;
        }

        token_ = {self_close ? XmlType::SelfClose : XmlType::OpenTag, name, {}, start};
        return token_;
    }

    // Text content
    size_t text_start = pos_;
    while (pos_ < input_.size() && input_[pos_] != '<') ++pos_;
    token_ = {XmlType::Text, input_.substr(text_start, pos_ - text_start), {}, text_start};
    return token_;
}

void XmlTokenizer::reset(std::string_view input) noexcept {
    input_ = input;
    pos_ = 0;
    token_ = {};
}

bool looks_like_xml(std::string_view input) noexcept {
    for (char c : input) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
        return c == '<';
    }
    return false;
}

size_t xml_escape_inplace(char* data, size_t len, size_t capacity) noexcept {
    // First pass: count special chars
    size_t extra = 0;
    for (size_t i = 0; i < len; ++i) {
        switch (data[i]) {
            case '&': extra += 4; break;  // &amp;
            case '<': extra += 3; break;  // &lt;
            case '>': extra += 3; break;  // &gt;
            case '"': extra += 5; break;  // &quot;
            case '\'': extra += 5; break; // &apos;
        }
    }
    if (extra == 0) return len;
    if (len + extra > capacity) return 0; // Not enough space

    // Second pass: escape from end to beginning (in-place)
    size_t j = len + extra;
    for (size_t i = len; i > 0; --i) {
        switch (data[i-1]) {
            case '&':  j -= 5; memcpy(data + j, "&amp;", 5); break;
            case '<':  j -= 4; memcpy(data + j, "&lt;", 4); break;
            case '>':  j -= 4; memcpy(data + j, "&gt;", 4); break;
            case '"':  j -= 6; memcpy(data + j, "&quot;", 6); break;
            case '\'': j -= 6; memcpy(data + j, "&apos;", 6); break;
            default:   --j; data[j] = data[i-1]; break;
        }
    }
    return len + extra;
}

} // namespace simdtext

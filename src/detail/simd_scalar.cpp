#include <cstddef>

namespace simdtext::detail::scalar {

size_t count_byte(const char* data, size_t size, char byte) {
    size_t count = 0;
    for (size_t i = 0; i < size; ++i)
        if (data[i] == byte) ++count;
    return count;
}

bool is_ascii(const char* data, size_t size) {
    for (size_t i = 0; i < size; ++i)
        if (static_cast<unsigned char>(data[i]) > 0x7F) return false;
    return true;
}

void lowercase_ascii(char* data, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        auto c = static_cast<unsigned char>(data[i]);
        if (c >= 'A' && c <= 'Z') data[i] = static_cast<char>(c + 32);
    }
}

void uppercase_ascii(char* data, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        auto c = static_cast<unsigned char>(data[i]);
        if (c >= 'a' && c <= 'z') data[i] = static_cast<char>(c - 32);
    }
}

const char* find_byte(const char* data, size_t size, char byte) {
    for (size_t i = 0; i < size; ++i)
        if (data[i] == byte) return data + i;
    return data + size;
}

} // namespace simdtext::detail::scalar

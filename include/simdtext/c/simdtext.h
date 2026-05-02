#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle for mapped file
typedef struct simdtext_file* simdtext_file_t;

// Version
int simdtext_version_major(void);
int simdtext_version_minor(void);
int simdtext_version_patch(void);

// Scanning
size_t simdtext_count_byte(const char* data, size_t len, char byte);
size_t simdtext_count_newlines(const char* data, size_t len);
int simdtext_contains(const char* haystack, size_t haystack_len, const char* needle, size_t needle_len);

// ASCII
int simdtext_is_ascii(const char* data, size_t len);
void simdtext_lowercase_ascii(char* data, size_t len);
void simdtext_uppercase_ascii(char* data, size_t len);

// UTF-8
int simdtext_valid_utf8(const char* data, size_t len);

// Encoding
// Returns allocated string, caller must free with simdtext_free()
char* simdtext_hex_encode(const char* data, size_t len);
char* simdtext_base64_encode(const char* data, size_t len);

// URL
char* simdtext_url_encode(const char* data, size_t len);
char* simdtext_url_decode(const char* data, size_t len);

// File I/O
simdtext_file_t simdtext_file_open(const char* path);
void simdtext_file_close(simdtext_file_t file);
const char* simdtext_file_data(simdtext_file_t file);
size_t simdtext_file_size(simdtext_file_t file);
size_t simdtext_file_count_lines(simdtext_file_t file);

// Memory management
void simdtext_free(void* ptr);

#ifdef __cplusplus
}
#endif

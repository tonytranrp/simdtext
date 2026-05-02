#pragma once

#include <stddef.h>
#include <stdint.h>

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
typedef struct simdtext_utf8_result {
    int valid;           ///< 1 if valid UTF-8
    size_t error_offset; ///< Byte offset of first error (0 if valid)
    uint8_t error_byte;  ///< The invalid byte at error_offset
} simdtext_utf8_result_t;

int simdtext_valid_utf8(const char* data, size_t len);
simdtext_utf8_result_t simdtext_validate_utf8_detailed(const char* data, size_t len);

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

// Pattern scanning
const uint8_t* simdtext_find_pattern(const uint8_t* data, size_t length, const char* hex_pattern);
int simdtext_byte_pattern_parse(const char* hex_pattern, uint8_t* out_bytes, uint8_t* out_masks, size_t* out_length, size_t capacity);

// Parallel processing
size_t simdtext_parallel_count_byte(const char* data, size_t len, char byte, unsigned int num_threads);

// Memory management
void simdtext_free(void* ptr);

#ifdef __cplusplus
}
#endif

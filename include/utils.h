#ifndef UTILS_H
#define UTILS_H

#include <windows.h>
#include <stdbool.h>

// Time utilities
void get_timestamp_string(char* buffer, size_t size);
DWORD get_time_ms(void);

// String utilities
bool str_ends_with(const char* str, const char* suffix);
void str_trim(char* str);
size_t str_copy_safe(char* dest, const char* src, size_t size);

// File utilities
bool file_exists(const char* path);
bool create_directory_if_needed(const char* path);
size_t get_file_size(const char* path);
bool is_path_valid(const char* path);
DWORD get_last_error_string(char* buffer, size_t size);
bool ensure_path_exists(const char* path);

// Debug utilities
void debug_print(const char* format, ...);

#endif // UTILS_H
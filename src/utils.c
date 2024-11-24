#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <ctype.h>

void get_timestamp_string(char* buffer, size_t size) {
    if (!buffer || size < 20) return;  // Add safety check
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(buffer, size, "%04d%02d%02d_%02d%02d%02d",
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond);
}

DWORD get_time_ms(void) {
    return GetTickCount();
}

bool str_ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) return false;
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return false;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

void str_trim(char* str) {
    if (!str || !*str) return;  // Check for empty string
    
    char* start = str;
    char* end = str + strlen(str) - 1;
    
    while (*start && isspace((unsigned char)*start)) start++;
    while (end > start && isspace((unsigned char)*end)) end--;
    
    size_t len = (end >= start) ? (end - start + 1) : 0;
    if (start != str) memmove(str, start, len);
    str[len] = '\0';
}

size_t str_copy_safe(char* dest, const char* src, size_t size) {
    if (!dest || !src || size == 0) return 0;
    size_t len = strlen(src);
    if (len >= size) len = size - 1;
    memcpy(dest, src, len);
    dest[len] = '\0';
    return len;
}

bool file_exists(const char* path) {
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES;
}

bool create_directory_if_needed(const char* path) {
    if (!path) return false;
    
    if (CreateDirectoryA(path, NULL)) return true;
    
    DWORD error = GetLastError();
    if (error == ERROR_ALREADY_EXISTS) return true;
    
    debug_print("Failed to create directory: %lu", error);
    return false;
}

size_t get_file_size(const char* path) {
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &attrs)) {
        return 0;
    }
    return (size_t)attrs.nFileSizeLow;
}

bool is_path_valid(const char* path) {
    if (!path) return false;
    // Check for invalid characters and path length
    for (const char* p = path; *p; p++) {
        if (*p < 32 || *p == '<' || *p == '>' || *p == '"' || 
            *p == '|' || *p == '?' || *p == '*') {
            return false;
        }
    }
    return strlen(path) < MAX_PATH;
}

DWORD get_last_error_string(char* buffer, size_t size) {
    if (!buffer || size == 0) return 0;
    DWORD error = GetLastError();
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        error,
        0,
        buffer,
        size,
        NULL
    );
    return error;
}

bool ensure_path_exists(const char* path) {
    if (!path || !is_path_valid(path)) return false;
    
    char temp[MAX_PATH];
    char* p = temp;
    strncpy(temp, path, MAX_PATH - 1);
    temp[MAX_PATH - 1] = '\0';
    
    while ((p = strchr(p, '\\'))) {
        *p = '\0';
        if (!create_directory_if_needed(temp)) return false;
        *p = '\\';
        p++;
    }
    return create_directory_if_needed(temp);
}

void debug_print(const char* format, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
#endif
}
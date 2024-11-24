#ifndef LOGGER_H
#define LOGGER_H

#include <stdbool.h>
#include <windows.h>

// Logger configuration
#define LOG_MAX_PATH 260
#define LOG_TIMESTAMP_SIZE 32
#define LOG_BUFFER_SIZE 4096
#define LOG_MAX_FILE_SIZE (100 * 1024 * 1024)  // 100MB max file size
#define LOG_MAX_WRITE_RETRIES 3

// Logger error codes
#define LOG_ERROR_NONE      0
#define LOG_ERROR_INIT      1
#define LOG_ERROR_FILE      2
#define LOG_ERROR_WRITE     3
#define LOG_ERROR_INVALID   4
#define LOG_ERROR_MEMORY    5
#define LOG_ERROR_SIZE      6

// Debug logging
#ifdef DEBUG
    #define LOG_DEBUG(msg, ...) fprintf(stderr, "[Logger] " msg "\n", ##__VA_ARGS__)
#else
    #define LOG_DEBUG(msg, ...)
#endif

/**
 * Logger structure containing all logger-related data and state
 */
typedef struct {
    HANDLE file_handle;         // File handle for the log file
    char filepath[LOG_MAX_PATH];// Path to the log file
    volatile bool initialized;   // Initialization flag
    CRITICAL_SECTION lock;      // Thread safety
    volatile DWORD last_error;  // Last error code
    volatile size_t current_file_size;  // Current file size
    struct {
        volatile size_t total_writes;    // Total number of writes
        volatile size_t failed_writes;   // Number of failed writes
        volatile size_t bytes_written;   // Total bytes written
        volatile size_t retry_count;     // Number of write retries
    } stats;
} Logger;

// Core functions
bool init_logger(const char* filepath);
void cleanup_logger(void);
bool write_to_log(const char* data, size_t size);
bool flush_log(void);

// Utility functions
bool is_logger_initialized(void);
DWORD get_logger_last_error(void);
size_t get_logger_bytes_written(void);
void reset_logger_stats(void);
bool check_logger_health(void);
size_t get_current_file_size(void);

#endif // LOGGER_H
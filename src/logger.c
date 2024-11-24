#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

// Global logger instance
static Logger logger = {0};

// Forward declarations of internal functions
static void set_logger_error_internal(DWORD error_code);
static bool format_timestamp(char* buffer, size_t size);
static bool write_with_timestamp(const char* data, size_t size);
static bool validate_logger_state(void);
static bool CreateDirectoryIfNotExists(const char* filepath);
static bool write_with_retry(HANDLE handle, const void* data, 
                           DWORD size, DWORD* written);
static bool check_file_size(size_t additional_bytes);

// Initialize logger
bool init_logger(const char* filepath) {
    if (!filepath || strlen(filepath) >= LOG_MAX_PATH) {
        set_logger_error_internal(LOG_ERROR_INVALID);
        LOG_DEBUG("Invalid filepath provided");
        return false;
    }

    if (logger.initialized) {
        set_logger_error_internal(LOG_ERROR_INIT);
        LOG_DEBUG("Logger already initialized");
        return false;
    }

    // Create directory if needed
    if (!CreateDirectoryIfNotExists(filepath)) {
        set_logger_error_internal(LOG_ERROR_FILE);
        LOG_DEBUG("Failed to create log directory");
        return false;
    }

    // Initialize critical section
    __try {
        InitializeCriticalSection(&logger.lock);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        set_logger_error_internal(LOG_ERROR_INIT);
        LOG_DEBUG("Failed to initialize critical section");
        return false;
    }

    EnterCriticalSection(&logger.lock);
    bool init_success = false;

    __try {
        // Create or open log file
        logger.file_handle = CreateFileA(
            filepath,
            FILE_APPEND_DATA,
            FILE_SHARE_READ,
            NULL,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (logger.file_handle == INVALID_HANDLE_VALUE) {
            set_logger_error_internal(LOG_ERROR_FILE);
            LOG_DEBUG("Failed to open log file: %s (Error: %lu)", 
                     filepath, GetLastError());
            __leave;
        }

        // Get initial file size
        LARGE_INTEGER file_size;
        if (!GetFileSizeEx(logger.file_handle, &file_size)) {
            set_logger_error_internal(LOG_ERROR_FILE);
            LOG_DEBUG("Failed to get file size (Error: %lu)", GetLastError());
            __leave;
        }
        logger.current_file_size = (size_t)file_size.QuadPart;

        // Initialize logger properties
        strncpy(logger.filepath, filepath, LOG_MAX_PATH - 1);
        logger.filepath[LOG_MAX_PATH - 1] = '\0';
        logger.initialized = true;
        logger.last_error = LOG_ERROR_NONE;
        memset(&logger.stats, 0, sizeof(logger.stats));

        LOG_DEBUG("Logger initialized with file: %s (Size: %zu)", 
                 filepath, logger.current_file_size);
        init_success = true;
    }
    __finally {
        LeaveCriticalSection(&logger.lock);
        if (!init_success) {
            if (logger.file_handle != INVALID_HANDLE_VALUE) {
                CloseHandle(logger.file_handle);
                logger.file_handle = INVALID_HANDLE_VALUE;
            }
            DeleteCriticalSection(&logger.lock);
        }
    }

    return init_success;
}

// Clean up logger resources
void cleanup_logger(void) {
    if (!logger.initialized) {
        return;
    }

    EnterCriticalSection(&logger.lock);
    __try {
        if (logger.file_handle != INVALID_HANDLE_VALUE) {
            FlushFileBuffers(logger.file_handle);
            CloseHandle(logger.file_handle);
            logger.file_handle = INVALID_HANDLE_VALUE;
        }
        logger.initialized = false;
        
        LOG_DEBUG("Logger cleanup complete. Stats: Writes: %zu, Failed: %zu, "
                 "Bytes: %zu, Retries: %zu",
                 logger.stats.total_writes,
                 logger.stats.failed_writes,
                 logger.stats.bytes_written,
                 logger.stats.retry_count);
    }
    __finally {
        LeaveCriticalSection(&logger.lock);
        DeleteCriticalSection(&logger.lock);
    }
}

// Write data to log file
bool write_to_log(const char* data, size_t size) {
    if (!data || size == 0 || size > LOG_BUFFER_SIZE) {
        set_logger_error_internal(LOG_ERROR_INVALID);
        LOG_DEBUG("Invalid write attempt: size=%zu", size);
        return false;
    }

    if (!validate_logger_state()) {
        return false;
    }

    if (!check_file_size(size + LOG_TIMESTAMP_SIZE + 1)) {  // +1 for potential newline
        return false;
    }

    return write_with_timestamp(data, size);
}

// Format current timestamp
static bool format_timestamp(char* buffer, size_t size) {
    time_t now;
    struct tm timeinfo;

    time(&now);
    if (localtime_s(&timeinfo, &now) != 0) {
        LOG_DEBUG("Failed to get local time");
        return false;
    }

    int written = snprintf(buffer, size, "[%04d-%02d-%02d %02d:%02d:%02d] ",
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec);

    return (written > 0 && written < size);
}

// Write data with timestamp
static bool write_with_timestamp(const char* data, size_t size) {
    char timestamp[LOG_TIMESTAMP_SIZE];
    if (!format_timestamp(timestamp, sizeof(timestamp))) {
        set_logger_error_internal(LOG_ERROR_WRITE);
        return false;
    }

    EnterCriticalSection(&logger.lock);
    bool success = false;
    DWORD total_bytes = 0;
    DWORD written;

    __try {
        // Write timestamp
        if (!write_with_retry(logger.file_handle, timestamp, strlen(timestamp), &written)) {
            set_logger_error_internal(LOG_ERROR_WRITE);
            logger.stats.failed_writes++;
            __leave;
        }
        total_bytes += written;

        // Write data
        if (!write_with_retry(logger.file_handle, data, size, &written)) {
            set_logger_error_internal(LOG_ERROR_WRITE);
            logger.stats.failed_writes++;
            __leave;
        }
        total_bytes += written;

        // Write newline if needed
        if (data[size-1] != '\n') {
            const char newline = '\n';
            if (!write_with_retry(logger.file_handle, &newline, 1, &written)) {
                set_logger_error_internal(LOG_ERROR_WRITE);
                logger.stats.failed_writes++;
                __leave;
            }
            total_bytes += written;
        }

        logger.stats.total_writes++;
        logger.stats.bytes_written += total_bytes;
        logger.current_file_size += total_bytes;
        success = true;
    }
    __finally {
        LeaveCriticalSection(&logger.lock);
    }

    return success;
}

// Write with retry logic
static bool write_with_retry(HANDLE handle, const void* data, 
                           DWORD size, DWORD* written) {
    for (int retry = 0; retry < LOG_MAX_WRITE_RETRIES; retry++) {
        if (WriteFile(handle, data, size, written, NULL)) {
            return true;
        }
        logger.stats.retry_count++;
        Sleep(10);  // Short delay before retry
    }
    return false;
}

// Check if file size would exceed limit
static bool check_file_size(size_t additional_bytes) {
    if (logger.current_file_size + additional_bytes > LOG_MAX_FILE_SIZE) {
        set_logger_error_internal(LOG_ERROR_SIZE);
        LOG_DEBUG("File size limit reached: current=%zu, additional=%zu, max=%zu",
                 logger.current_file_size, additional_bytes, LOG_MAX_FILE_SIZE);
        return false;
    }
    return true;
}

// Create directory if it doesn't exist
static bool CreateDirectoryIfNotExists(const char* filepath) {
    char dir[LOG_MAX_PATH];
    strncpy(dir, filepath, LOG_MAX_PATH - 1);
    dir[LOG_MAX_PATH - 1] = '\0';

    char* last_slash = strrchr(dir, '\\');
    if (last_slash) {
        *last_slash = '\0';
        if (!CreateDirectoryA(dir, NULL) && 
            GetLastError() != ERROR_ALREADY_EXISTS) {
            return false;
        }
    }
    return true;
}

// Flush log file buffers
bool flush_log(void) {
    if (!validate_logger_state()) {
        return false;
    }

    EnterCriticalSection(&logger.lock);
    bool success = FlushFileBuffers(logger.file_handle);
    LeaveCriticalSection(&logger.lock);

    if (!success) {
        set_logger_error_internal(LOG_ERROR_WRITE);
        LOG_DEBUG("Failed to flush log file (Error: %lu)", GetLastError());
    }

    return success;
}

// Utility functions implementation...
[Previous utility functions remain the same]

// Internal helper functions
static void set_logger_error_internal(DWORD error_code) {
    logger.last_error = error_code;
    DWORD sys_error = GetLastError();
    LOG_DEBUG("Logger error set: %lu (System error: %lu)", 
              error_code, sys_error);
}

static bool validate_logger_state(void) {
    EnterCriticalSection(&logger.lock);
    bool valid = logger.initialized && 
                (logger.file_handle != INVALID_HANDLE_VALUE);
    if (!valid) {
        set_logger_error_internal(LOG_ERROR_INIT);
    }
    LeaveCriticalSection(&logger.lock);
    return valid;
}
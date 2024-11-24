#ifndef BUFFER_H
#define BUFFER_H

#include <stdbool.h>
#include <windows.h>

// Buffer configuration
#define BUFFER_SIZE 4096
#define BUFFER_FLUSH_THRESHOLD (BUFFER_SIZE * 0.75)
#define BUFFER_MAX_EVENT_SIZE 1024

// Safety checks
#if BUFFER_MAX_EVENT_SIZE >= BUFFER_SIZE
    #error "BUFFER_MAX_EVENT_SIZE must be smaller than BUFFER_SIZE"
#endif

#if BUFFER_FLUSH_THRESHOLD >= BUFFER_SIZE
    #error "BUFFER_FLUSH_THRESHOLD must be smaller than BUFFER_SIZE"
#endif

/**
 * Buffer error codes:
 * BUFFER_ERROR_NONE (0):    No error
 * BUFFER_ERROR_INIT (1):    Initialization failed
 * BUFFER_ERROR_FULL (2):    Buffer is full
 * BUFFER_ERROR_EMPTY (3):   Buffer is empty
 * BUFFER_ERROR_INVALID (4): Invalid parameter or state
 * BUFFER_ERROR_FLUSH (5):   Flush operation failed
 * BUFFER_ERROR_MEMORY (6):  Memory allocation failed
 */
#define BUFFER_ERROR_NONE       0
#define BUFFER_ERROR_INIT       1
#define BUFFER_ERROR_FULL       2
#define BUFFER_ERROR_EMPTY      3
#define BUFFER_ERROR_INVALID    4
#define BUFFER_ERROR_FLUSH      5
#define BUFFER_ERROR_MEMORY     6

// Debug logging
#ifdef DEBUG
    #define BUFFER_LOG(msg, ...) fprintf(stderr, "[Buffer] " msg "\n", ##__VA_ARGS__)
#else
    #define BUFFER_LOG(msg, ...)
#endif

/**
 * Buffer structure containing all buffer-related data and state
 */
typedef struct {
    char* data;                  // Buffer data
    volatile size_t size;        // Current size
    volatile size_t capacity;    // Maximum capacity
    CRITICAL_SECTION lock;       // Thread safety
    volatile bool initialized;   // Initialization flag
    volatile DWORD last_error;   // Last error code
    struct {
        volatile size_t total_flushes;   // Total number of flushes
        volatile size_t failed_flushes;  // Number of failed flushes
        volatile size_t total_writes;    // Total number of writes
        volatile size_t failed_writes;   // Number of failed writes
    } stats;
} Buffer;

// Core functions
bool init_buffer(void);
void cleanup_buffer(void);
bool add_to_buffer(const char* event_data, size_t data_size);
bool flush_buffer_if_needed(void);
bool force_flush_buffer(void);

// Utility functions
bool is_buffer_initialized(void);
size_t get_buffer_size(void);
size_t get_buffer_capacity(void);
DWORD get_buffer_last_error(void);
void clear_buffer(void);
bool check_buffer_health(void);

// Additional status functions
bool is_buffer_full(void);
bool is_buffer_empty(void);
float get_buffer_usage_percentage(void);
void reset_buffer_stats(void);

#endif // BUFFER_H
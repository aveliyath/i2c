#include "buffer.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Global buffer instance
static Buffer buffer = {0};

// Forward declarations of internal functions
static void set_buffer_error_internal(DWORD error_code);
static bool should_flush_buffer(void);
static bool validate_buffer_state(void);
static void reset_buffer_stats_internal(void);

// Initialize buffer
bool init_buffer(void) {
    // Check if already initialized
    bool was_initialized;
    EnterCriticalSection(&buffer.lock);
    was_initialized = buffer.initialized;
    LeaveCriticalSection(&buffer.lock);

    if (was_initialized) {
        set_buffer_error_internal(BUFFER_ERROR_INIT);
        return false;
    }

    // Initialize critical section
    __try {
        InitializeCriticalSection(&buffer.lock);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        set_buffer_error_internal(BUFFER_ERROR_INIT);
        return false;
    }

    EnterCriticalSection(&buffer.lock);
    bool init_success = false;
    
    __try {
        // Allocate and initialize buffer memory
        buffer.data = (char*)malloc(BUFFER_SIZE);
        if (!buffer.data) {
            set_buffer_error_internal(BUFFER_ERROR_MEMORY);
            __leave;
        }
        memset(buffer.data, 0, BUFFER_SIZE);

        // Initialize buffer properties
        buffer.capacity = BUFFER_SIZE;
        buffer.size = 0;
        buffer.initialized = true;
        buffer.last_error = BUFFER_ERROR_NONE;
        reset_buffer_stats_internal();

        BUFFER_LOG("Buffer initialized with capacity: %zu bytes", buffer.capacity);
        init_success = true;
    }
    __finally {
        LeaveCriticalSection(&buffer.lock);
        if (!init_success) {
            DeleteCriticalSection(&buffer.lock);
        }
    }

    return init_success;
}

// Clean up buffer resources
void cleanup_buffer(void) {
    bool was_initialized;
    EnterCriticalSection(&buffer.lock);
    was_initialized = buffer.initialized;
    LeaveCriticalSection(&buffer.lock);

    if (!was_initialized) {
        return;
    }

    EnterCriticalSection(&buffer.lock);
    __try {
        if (buffer.size > 0) {
            BUFFER_LOG("Flushing remaining %zu bytes during cleanup", buffer.size);
            if (buffer.data) {
                force_flush_buffer();
            }
        }

        if (buffer.data) {
            free(buffer.data);
            buffer.data = NULL;
        }
        buffer.size = 0;
        buffer.capacity = 0;
        buffer.initialized = false;
        
        BUFFER_LOG("Buffer cleanup complete. Stats: Flushes: %zu, Failed: %zu, Writes: %zu, Failed: %zu",
                  buffer.stats.total_flushes, buffer.stats.failed_flushes,
                  buffer.stats.total_writes, buffer.stats.failed_writes);
    }
    __finally {
        LeaveCriticalSection(&buffer.lock);
        DeleteCriticalSection(&buffer.lock);
    }
}

// Add data to buffer
bool add_to_buffer(const char* event_data, size_t data_size) {
    if (!event_data || data_size == 0 || data_size > BUFFER_MAX_EVENT_SIZE) {
        set_buffer_error_internal(BUFFER_ERROR_INVALID);
        BUFFER_LOG("Invalid buffer add attempt: size=%zu", data_size);
        return false;
    }

    // Check for integer overflow
    if (data_size > SIZE_MAX - buffer.size) {
        set_buffer_error_internal(BUFFER_ERROR_INVALID);
        BUFFER_LOG("Integer overflow detected in add_to_buffer");
        return false;
    }

    if (!validate_buffer_state()) {
        return false;
    }

    EnterCriticalSection(&buffer.lock);
    bool success = false;

    __try {
        // Check if there's enough space
        if (buffer.size + data_size > buffer.capacity) {
            BUFFER_LOG("Buffer full, attempting flush before add");
            if (!force_flush_buffer()) {
                set_buffer_error_internal(BUFFER_ERROR_FULL);
                buffer.stats.failed_writes++;
                __leave;
            }
        }

        // Double check after potential flush
        if (buffer.size + data_size <= buffer.capacity) {
            memcpy(buffer.data + buffer.size, event_data, data_size);
            buffer.size += data_size;
            buffer.stats.total_writes++;
            success = true;
            BUFFER_LOG("Added %zu bytes to buffer, total size: %zu", data_size, buffer.size);
        } else {
            set_buffer_error_internal(BUFFER_ERROR_FULL);
            buffer.stats.failed_writes++;
            BUFFER_LOG("Buffer full after flush attempt");
        }
    }
    __finally {
        LeaveCriticalSection(&buffer.lock);
    }

    return success;
}

// Check if buffer should be flushed
static bool should_flush_buffer(void) {
    return buffer.size >= BUFFER_FLUSH_THRESHOLD;
}

// Flush buffer if threshold is reached
bool flush_buffer_if_needed(void) {
    if (!validate_buffer_state()) {
        return false;
    }

    EnterCriticalSection(&buffer.lock);
    bool flushed = false;

    __try {
        if (should_flush_buffer()) {
            BUFFER_LOG("Threshold reached (%zu bytes), flushing buffer", buffer.size);
            flushed = force_flush_buffer();
        }
    }
    __finally {
        LeaveCriticalSection(&buffer.lock);
    }

    return flushed;
}

// Force buffer flush
bool force_flush_buffer(void) {
    if (!validate_buffer_state() || buffer.size == 0) {
        return false;
    }

    EnterCriticalSection(&buffer.lock);
    bool flush_success = false;

    __try {
        buffer.stats.total_flushes++;
        
        // Write buffer contents to log file
        if (write_to_log(buffer.data, buffer.size)) {
            buffer.size = 0;
            memset(buffer.data, 0, buffer.capacity);
            flush_success = true;
            BUFFER_LOG("Buffer flushed successfully");
        } else {
            buffer.stats.failed_flushes++;
            set_buffer_error_internal(BUFFER_ERROR_FLUSH);
            BUFFER_LOG("Buffer flush failed");
        }
    }
    __finally {
        LeaveCriticalSection(&buffer.lock);
    }

    return flush_success;
}

// Utility functions
bool is_buffer_initialized(void) {
    EnterCriticalSection(&buffer.lock);
    bool init = buffer.initialized;
    LeaveCriticalSection(&buffer.lock);
    return init;
}

size_t get_buffer_size(void) {
    if (!validate_buffer_state()) {
        return 0;
    }
    
    EnterCriticalSection(&buffer.lock);
    size_t size = buffer.size;
    LeaveCriticalSection(&buffer.lock);
    return size;
}

size_t get_buffer_capacity(void) {
    EnterCriticalSection(&buffer.lock);
    size_t cap = buffer.capacity;
    LeaveCriticalSection(&buffer.lock);
    return cap;
}

DWORD get_buffer_last_error(void) {
    EnterCriticalSection(&buffer.lock);
    DWORD error = buffer.last_error;
    LeaveCriticalSection(&buffer.lock);
    return error;
}

void clear_buffer(void) {
    if (!validate_buffer_state()) {
        return;
    }

    EnterCriticalSection(&buffer.lock);
    __try {
        buffer.size = 0;
        memset(buffer.data, 0, buffer.capacity);
        reset_buffer_stats_internal();
        BUFFER_LOG("Buffer cleared and stats reset");
    }
    __finally {
        LeaveCriticalSection(&buffer.lock);
    }
}

bool check_buffer_health(void) {
    if (!validate_buffer_state()) {
        return false;
    }
    
    EnterCriticalSection(&buffer.lock);
    bool healthy = (buffer.size <= buffer.capacity) && 
                  (buffer.data != NULL) &&
                  (buffer.initialized);
    LeaveCriticalSection(&buffer.lock);
    
    BUFFER_LOG("Buffer health check: %s", healthy ? "OK" : "FAILED");
    return healthy;
}

// Additional status functions
bool is_buffer_full(void) {
    if (!validate_buffer_state()) {
        return true;
    }
    
    EnterCriticalSection(&buffer.lock);
    bool full = buffer.size >= buffer.capacity;
    LeaveCriticalSection(&buffer.lock);
    return full;
}

bool is_buffer_empty(void) {
    if (!validate_buffer_state()) {
        return true;
    }
    
    EnterCriticalSection(&buffer.lock);
    bool empty = buffer.size == 0;
    LeaveCriticalSection(&buffer.lock);
    return empty;
}

float get_buffer_usage_percentage(void) {
    if (!validate_buffer_state()) {
        return 0.0f;
    }
    
    EnterCriticalSection(&buffer.lock);
    float usage = ((float)buffer.size / buffer.capacity) * 100.0f;
    LeaveCriticalSection(&buffer.lock);
    return usage;
}

void reset_buffer_stats(void) {
    if (!validate_buffer_state()) {
        return;
    }

    EnterCriticalSection(&buffer.lock);
    reset_buffer_stats_internal();
    LeaveCriticalSection(&buffer.lock);
}

// Internal helper functions
static void set_buffer_error_internal(DWORD error_code) {
    // Note: Should only be called when lock is already held or during initialization
    buffer.last_error = error_code;
    BUFFER_LOG("Buffer error set: %lu", error_code);
}

static bool validate_buffer_state(void) {
    EnterCriticalSection(&buffer.lock);
    bool valid = buffer.initialized && buffer.data;
    if (!valid) {
        set_buffer_error_internal(BUFFER_ERROR_INIT);
    }
    LeaveCriticalSection(&buffer.lock);
    return valid;
}

static void reset_buffer_stats_internal(void) {
    // Note: Should only be called when lock is already held
    assert(buffer.initialized);
    buffer.stats.total_flushes = 0;
    buffer.stats.failed_flushes = 0;
    buffer.stats.total_writes = 0;
    buffer.stats.failed_writes = 0;
}
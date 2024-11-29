#include "buffer.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

// Global buffer instance
static Buffer buffer = {0};

// Internal utility functions to manage errors, validate state, and reset stats
static void set_buffer_error_internal(DWORD error_code);
static bool should_flush_buffer(void);
static bool validate_buffer_state(void);
static void reset_buffer_stats_internal(void);

// Sets up the buffer for use, including memory allocation and critical section initialization
bool init_buffer(void) {
    // Initialize critical section for thread safety
    if (!InitializeCriticalSectionAndSpinCount(&buffer.lock, 0x00000400)) { 
        set_buffer_error_internal(BUFFER_ERROR_INIT);
        return false;
    }

    EnterCriticalSection(&buffer.lock);

    // Check if already initialized
    if (buffer.initialized) {
        set_buffer_error_internal(BUFFER_ERROR_INIT);
        LeaveCriticalSection(&buffer.lock);
        DeleteCriticalSection(&buffer.lock);
        return false;
    }

    // Allocate and initialize buffer memory
    buffer.data = (char *)malloc(BUFFER_SIZE);
    if (!buffer.data) {
        set_buffer_error_internal(BUFFER_ERROR_MEMORY);
        LeaveCriticalSection(&buffer.lock);
        DeleteCriticalSection(&buffer.lock);
        return false;
    }

    // Initialize buffer metadata
    memset(buffer.data, 0, BUFFER_SIZE);
    buffer.capacity = BUFFER_SIZE;
    buffer.size = 0;
    buffer.initialized = true;
    buffer.last_error = BUFFER_ERROR_NONE;
    reset_buffer_stats_internal();

    BUFFER_LOG("Buffer initialized with capacity: %zu bytes", buffer.capacity);
    LeaveCriticalSection(&buffer.lock);

    return true;
}

// Releases memory and critical section resources associated with the buffer
void cleanup_buffer(void) {
    EnterCriticalSection(&buffer.lock);
    if (!buffer.initialized) {
        LeaveCriticalSection(&buffer.lock);
        return;
    }

    // Flush remaining data in buffer
    if (buffer.size > 0) {
        BUFFER_LOG("Flushing remaining %zu bytes during cleanup", buffer.size);
        force_flush_buffer();
    }

    // Free allocated memory
    if (buffer.data) {
        free(buffer.data);
        buffer.data = NULL;
    }

    // Reset buffer metadata
    buffer.size = 0;
    buffer.capacity = 0;
    buffer.initialized = false;

    BUFFER_LOG("Buffer cleanup complete. Stats: Flushes: %zu, Failed: %zu, Writes: %zu, Failed: %zu",
               buffer.stats.total_flushes, buffer.stats.failed_flushes,
               buffer.stats.total_writes, buffer.stats.failed_writes);
    LeaveCriticalSection(&buffer.lock);

    // Delete critical section to prevent further access
    DeleteCriticalSection(&buffer.lock);
}


// Appends new data to the buffer, flushing it if necessary when full
bool add_to_buffer(const char *event_data, size_t data_size) {
    if (!event_data || data_size == 0 || data_size > BUFFER_MAX_EVENT_SIZE) {
        set_buffer_error_internal(BUFFER_ERROR_INVALID);
        BUFFER_LOG("Invalid buffer add attempt: size=%zu", data_size);
        return false;
    }

    if (data_size > SIZE_MAX - buffer.size) {
        set_buffer_error_internal(BUFFER_ERROR_INVALID);
        BUFFER_LOG("Integer overflow detected in add_to_buffer");
        return false;
    }

    if (!validate_buffer_state()) {
        return false;
    }

    EnterCriticalSection(&buffer.lock);

    // Check if the buffer has enough space and flush if necessary
    if (buffer.size + data_size > buffer.capacity) {
        BUFFER_LOG("Buffer full, attempting flush before add");
        if (!force_flush_buffer()) {
            set_buffer_error_internal(BUFFER_ERROR_FULL);
            buffer.stats.failed_writes++;
            LeaveCriticalSection(&buffer.lock);
            return false;
        }
    }

    // Add data to buffer
    if (buffer.size + data_size <= buffer.capacity) {
        memcpy(buffer.data + buffer.size, event_data, data_size);
        buffer.size += data_size;
        buffer.stats.total_writes++;
        BUFFER_LOG("Added %zu bytes to buffer, total size: %zu", data_size, buffer.size);
        LeaveCriticalSection(&buffer.lock);
        return true;
    } else {
        // If buffer is still full after flush, return error
        set_buffer_error_internal(BUFFER_ERROR_FULL);
        buffer.stats.failed_writes++;
        BUFFER_LOG("Buffer full after flush attempt");
    }

    LeaveCriticalSection(&buffer.lock);
    return false;
}


// Check if buffer should be flushed
static bool should_flush_buffer(void) {
    return buffer.size >= BUFFER_FLUSH_THRESHOLD;
}

// Writes buffer data to the log if the flush threshold is met
bool flush_buffer_if_needed(void) {
    if (!validate_buffer_state()) {
        return false;
    }

    EnterCriticalSection(&buffer.lock);
    if (should_flush_buffer()) {
        BUFFER_LOG("Threshold reached (%zu bytes), flushing buffer", buffer.size);
        bool flushed = force_flush_buffer();
        LeaveCriticalSection(&buffer.lock);
        return flushed;
    }
    LeaveCriticalSection(&buffer.lock);

    return false;
}


// Force buffer flush
bool force_flush_buffer(void) {
    if (!validate_buffer_state() || buffer.size == 0) {
        return false;
    }

    EnterCriticalSection(&buffer.lock);

    buffer.stats.total_flushes++;
    if (write_to_log(buffer.data, buffer.size)) {
        buffer.size = 0;
        memset(buffer.data, 0, buffer.capacity);
        BUFFER_LOG("Buffer flushed successfully");
        LeaveCriticalSection(&buffer.lock);
        return true;
    } else {
        buffer.stats.failed_flushes++;
        set_buffer_error_internal(BUFFER_ERROR_FLUSH);
        BUFFER_LOG("Buffer flush failed");
    }

    LeaveCriticalSection(&buffer.lock);
    return false;
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

    buffer.size = 0;
    memset(buffer.data, 0, buffer.capacity);
    reset_buffer_stats_internal();

    BUFFER_LOG("Buffer cleared and stats reset");
    LeaveCriticalSection(&buffer.lock);
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
    buffer.last_error = error_code;
    BUFFER_LOG("Buffer error set: %u", error_code);
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
    assert(buffer.initialized);
    buffer.stats.total_flushes = 0;
    buffer.stats.failed_flushes = 0;
    buffer.stats.total_writes = 0;
    buffer.stats.failed_writes = 0;
}
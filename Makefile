# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Werror -Iinclude
LDFLAGS = 

# OS-specific settings
ifeq ($(OS),Windows_NT)
    LDFLAGS += -luser32 -lpsapi
    TARGET_EXT = .exe
    RM = del /Q
    RMDIR = rmdir /S /Q
else
    LDFLAGS += -lX11
    TARGET_EXT =
    RM = rm -rf
    RMDIR = rm -rf
endif

# Directories
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
TEST_DIR = tests
LOG_DIR = logs

# Files
TARGET = keylogger$(TARGET_EXT)
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_OBJS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(OBJ_DIR)/%.o)
TEST_TARGET = run_tests$(TARGET_EXT)

# Targets
.PHONY: all clean test dirs

all: dirs $(TARGET)

dirs:
	@if not exist $(OBJ_DIR) mkdir $(OBJ_DIR)
	@if not exist $(LOG_DIR) mkdir $(LOG_DIR)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Test targets
test: dirs $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJS) $(filter-out $(OBJ_DIR)/main.o, $(OBJS))
	$(CC) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target
clean:
	@if exist $(OBJ_DIR) $(RMDIR) $(OBJ_DIR)
	@if exist $(LOG_DIR) $(RMDIR) $(LOG_DIR)
	@if exist $(TARGET) $(RM) $(TARGET)
	@if exist $(TEST_TARGET) $(RM) $(TEST_TARGET)

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: all
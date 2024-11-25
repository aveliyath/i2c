# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Werror -I$(shell cygpath -w ./include)
LDFLAGS = 

# OS-specific settings
ifeq ($(OS),Windows_NT)
    LDFLAGS += -luser32
    TARGET_EXT = .exe
else
    LDFLAGS += -lX11
    TARGET_EXT =
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
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(LOG_DIR)

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
	rm -rf $(OBJ_DIR) $(TARGET) $(TEST_TARGET) $(LOG_DIR)/*

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: all
# Compiler definitions
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -O2 -Iinclude
LDFLAGS = 

# Folders layout
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj

# List of source inputs
SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/trace.c $(SRC_DIR)/vm.c $(SRC_DIR)/stats.c $(SRC_DIR)/replacement.c $(SRC_DIR)/tlb.c
# Direct object files mapping to target folder
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# Cross-platform definitions
ifeq ($(OS),Windows_NT)
    TARGET = vmsim.exe
    ifeq ($(OS_SHELL),UNIX)
        CREATE_OBJ_DIR = mkdir -p $(OBJ_DIR)
        CLEAN_CMD = rm -rf $(OBJ_DIR) $(TARGET)
        SAN_CHECK := $(shell gcc -fsanitize=address,undefined -x c -o /dev/null - <<< "int main(){}" 2>/dev/null && echo yes || echo no)
    else
        CREATE_OBJ_DIR = if not exist $(OBJ_DIR) mkdir $(OBJ_DIR)
        CLEAN_CMD = if exist $(OBJ_DIR) rmdir /S /Q $(OBJ_DIR) & if exist $(TARGET) del /Q /F $(TARGET)
        SAN_CHECK := $(shell cmd /c "echo int main(){}>test_san.c && (gcc -fsanitize=address,undefined test_san.c -o test_san.exe 2>nul && echo yes || echo no) & del test_san.c test_san.exe 2>nul")
    endif
else
    TARGET = vmsim
    CREATE_OBJ_DIR = mkdir -p $(OBJ_DIR)
    CLEAN_CMD = rm -rf $(OBJ_DIR) $(TARGET)
    SAN_CHECK := $(shell gcc -fsanitize=address,undefined -x c -o /dev/null - <<< "int main(){}" 2>/dev/null && echo yes || echo no)
endif

# Top-level build target
all: $(TARGET)

# Linking step
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Compile step with order-only directory prerequisite
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create obj folder dynamically
$(OBJ_DIR):
	$(CREATE_OBJ_DIR)

# Debug build incorporating AddressSanitizer and UndefinedBehaviorSanitizer
debug: CFLAGS += -g -O0
ifeq ($(SAN_CHECK),yes)
debug: CFLAGS += -fsanitize=address,undefined
debug: LDFLAGS += -fsanitize=address,undefined
endif
debug: clean $(TARGET)

# Clean build structures
clean:
	@$(CLEAN_CMD)

.PHONY: all clean debug

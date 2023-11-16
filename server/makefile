# Default compiler
CC ?= gcc
# Compiler flags
CFLAGS ?= -Wall -Werror -Wextra -O2
# Source directory
SRC_DIR ?= src
# Object directory
OBJ_DIR ?= obj
# Output directory for the application
BIN_DIR ?= bin

# Application name
APP_NAME ?= aesdsocket

# Source and object files
SRCS ?= aesdsocket.c
OBJS ?= $(SRCS:%.c=$(OBJ_DIR)/%.o)

# If the CROSS_COMPILE variable is set, prepend it to the CC variable
ifdef CROSS_COMPILE
    CC ?= $(CROSS_COMPILE)$(CC)
endif

# Define default target
.PHONY: default
default: all

# Define all target
.PHONY: all
all: $(APP_NAME)

# Link the application
$(APP_NAME): $(OBJS)
	$(CC) $(OBJS) -o $@ -pthread

# Compile source files to object files
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target to remove the application and object files
.PHONY: clean
clean:
	rm -rf wr $(APP_NAME) iter $(OBJ_DIR)/*
	# rm valgrind-out.txt

# Include dependencies
-include $(OBJS:.o=.d)

# Rule to generate a dependency file by using the C preprocessor
# (see man gcc for details on the -MM and -MT options).
$(OBJ_DIR)/%.d: %.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -MM -MT '$(OBJ_DIR)/$*.o $@' $< > $@

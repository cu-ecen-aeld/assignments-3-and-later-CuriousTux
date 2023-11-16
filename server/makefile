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

LDFLAGS ?= -Wl,--hash-style=gnu

TARGET ?= aesdsocket

# Application name
APP_NAME ?= TARGET

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
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS) -lpthread -lrt # $@ # -pthread

# Compile source files to object files
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@ # -c $< -o $@ $(INCLUDES) $(LDFLAGS) #$^ -o $@ $(INCLUDES) $(LDFLAGS) #-c $< -o $@

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

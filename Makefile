CC = gcc
CFLAGS = -Wall -g

# List of source files
SRCS = main.c ./src/fs.c ./src/disk.c

# List of header files
HDRS = ./src/fs.h ./src/disk.h

# Output executable
TARGET = main

# Default target
all: $(TARGET)

# Rule to build the executable
$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

# Rule to clean the project
clean:
	rm -f $(TARGET) core

.PHONY: all clean
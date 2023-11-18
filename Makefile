CC = gcc
CFLAGS = -Wall -g

# List of source files
SRCS = main.c fs.c disk.c

# List of header files
HDRS = fs.h disk.h

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
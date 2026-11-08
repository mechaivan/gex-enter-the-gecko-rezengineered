# Makefile for DFX parser tests

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
INCLUDES = -I../include
LIBS =

# Source files
SRC = test_dfx.c
SRC += ../src/asset/dfx.c

# Object files
OBJ = $(SRC:.c=.o)

# Executable
TARGET = test_dfx

# Default target
all: $(TARGET)

# Build the test executable
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET) $(OBJ) $(LIBS)

# Compile source files to objects
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJ) $(TARGET)

# Run the tests
test: $(TARGET)
	./$(TARGET)

# Run tests with valgrind (if available)
valgrind: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --verbose ./$(TARGET)

.PHONY: all clean test valgrind
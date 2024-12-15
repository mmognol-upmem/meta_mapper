# Makefile for compiling app/mapper.cpp with sources in src directory
# The compiled binary will be placed in the build directory and then copied to the bin directory

# Compiler
CXX := g++

# Directories
SRC_DIR := src
APP_DIR := app
BUILD_DIR := build
BIN_DIR := bin

# Source files
SRC_FILES := $(wildcard $(SRC_DIR)/*.cpp)
APP_FILE := $(APP_DIR)/mapper.cpp

# Output binary
BUILD_OUTPUT := $(BUILD_DIR)/mapper
BIN_OUTPUT := $(BIN_DIR)/mapper

# Include directories
INCLUDE_DIRS := -I$(SRC_DIR)

# Compiler flags
CXXFLAGS := -Wall -Werror -std=c++20 $(INCLUDE_DIRS)

# Create build and bin directories if they don't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Build target
$(BUILD_OUTPUT): $(SRC_FILES) $(APP_FILE) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Copy target
$(BIN_OUTPUT): $(BUILD_OUTPUT) | $(BIN_DIR)
	cp $< $@

# Clean target
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Default target
.PHONY: all
all: $(BIN_OUTPUT)
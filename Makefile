# Compiler and flags
CC = g++
CFLAGS = -O2 -std=c++20
CFLAGS += -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-value -Wno-unused-local-typedefs
#add arch flags needed
CFLAGS += -mpopcnt -msse -msse2 -msse3 -msse4 -msse4.1 -msse4.2 -mavx -mavx2 -mbmi -mbmi2

# Directories
SRC_DIR = src
LIB_DIR = thirdparty
APP_DIR = app
BIN_DIR = bin
BUILD_DIR = build

CFLAGS += -I$(SRC_DIR) -I$(LIB_DIR)/cxxopts/

# Targets
TARGETS = mapper index

# Source files
MAPPER_SRC = $(wildcard $(SRC_DIR)/*.cpp $(APP_DIR)/mapper.cpp)
INDEX_SRC = $(wildcard $(SRC_DIR)/*.cpp $(APP_DIR)/index.cpp)

# Object files
MAPPER_OBJ = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(notdir $(MAPPER_SRC)))
INDEX_OBJ = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(notdir $(INDEX_SRC)))

# Ensure directories exist
$(BIN_DIR) $(BUILD_DIR):
	mkdir -p $@

# Default target
all: $(BIN_DIR) $(BUILD_DIR) $(addprefix $(BIN_DIR)/, $(TARGETS))

# Build mapper executable
$(BIN_DIR)/mapper: $(MAPPER_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

# Build index executable
$(BIN_DIR)/index: $(INDEX_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

# Compile source files into object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(APP_DIR)/%.cpp | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(BUILD_DIR)/*.o $(BIN_DIR)/*

.PHONY: all clean mapper index

mapper: $(BIN_DIR)/mapper

index: $(BIN_DIR)/index
# ATHENA Core - Makefile
# 
# CRITICAL: These flags are required for determinism.
# DO NOT modify without updating manifest documentation.
#
# SPDX-License-Identifier: Proprietary
# Copyright (c) 2026 ATHENA Project

CXX = g++
CXXFLAGS = -std=c++17 \
           -Wall -Wextra -Wpedantic -Werror \
           -Wno-unused-parameter \
           -fno-fast-math \
           -ffp-contract=off \
           -I include

# Debug build (default)
DEBUG_FLAGS = -g3 -O0 -DDEBUG

# Release build
RELEASE_FLAGS = -O2 -DNDEBUG -march=x86-64-v2

# Source files
SOURCES = src/types.cpp \
          src/rng.cpp \
          src/context.cpp \
          src/scheduler.cpp \
          src/manifest.cpp \
          src/json.cpp \
          src/scenario.cpp \
          src/terrain.cpp \
          src/environment.cpp \
          src/terrain_semantics.cpp \
          src/pathfinding.cpp \
          src/serialization.cpp \
          src/platform_loader.cpp \
          src/systems/movement.cpp \
          src/systems/combat.cpp \
          src/systems/logistics.cpp \
          src/systems/detection.cpp \
          src/systems/c2.cpp \
          src/analysis/montecarlo.cpp \
          src/analysis/sobol.cpp

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Output directories
BUILD_DIR = ../build
LIB_DIR = $(BUILD_DIR)/lib
BIN_DIR = $(BUILD_DIR)/bin

# Default target
all: debug

# Debug build
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: $(LIB_DIR)/libathena-core.a test

# Release build  
release: CXXFLAGS += $(RELEASE_FLAGS)
release: $(LIB_DIR)/libathena-core.a test

# Create library
$(LIB_DIR)/libathena-core.a: $(OBJECTS) | $(LIB_DIR)
	ar rcs $@ $(OBJECTS)

# Compile source files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create output directories
$(LIB_DIR):
	mkdir -p $(LIB_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# CLI binary

# Test binary
test: $(BIN_DIR)/test_determinism $(BIN_DIR)/test_scenario $(BIN_DIR)/test_integration $(BIN_DIR)/test_montecarlo $(BIN_DIR)/test_terrain $(BIN_DIR)/test_detection $(BIN_DIR)/test_c2 $(BIN_DIR)/test_pathfinding $(BIN_DIR)/test_serialization $(BIN_DIR)/test_sobol $(BIN_DIR)/test_platform_loader
	@echo "Running determinism tests..."
	@$(BIN_DIR)/test_determinism
	@echo ""
	@echo "Running scenario tests..."
	@$(BIN_DIR)/test_scenario
	@echo ""
	@echo "Running integration tests..."
	@$(BIN_DIR)/test_integration
	@echo ""
	@echo "Running Monte Carlo tests..."
	@$(BIN_DIR)/test_montecarlo
	@echo ""
	@echo "Running terrain tests..."
	@$(BIN_DIR)/test_terrain
	@echo ""
	@echo "Running detection tests..."
	@$(BIN_DIR)/test_detection
	@echo ""
	@echo "Running C2 tests..."
	@$(BIN_DIR)/test_c2
	@echo ""
	@echo "Running pathfinding tests..."
	@$(BIN_DIR)/test_pathfinding
	@echo ""
	@echo "Running serialization tests..."
	@$(BIN_DIR)/test_serialization
	@echo ""
	@echo "Running Sobol sensitivity tests..."
	@$(BIN_DIR)/test_sobol
	@echo ""
	@echo "Running Platform Loader tests..."
	@$(BIN_DIR)/test_platform_loader

$(BIN_DIR)/test_determinism: test/test_determinism.cpp $(LIB_DIR)/libathena-core.a | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -L$(LIB_DIR) -lathena-core -o $@

$(BIN_DIR)/test_scenario: test/test_scenario.cpp $(LIB_DIR)/libathena-core.a | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -L$(LIB_DIR) -lathena-core -o $@

$(BIN_DIR)/test_integration: test/test_integration.cpp $(LIB_DIR)/libathena-core.a | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -L$(LIB_DIR) -lathena-core -o $@

$(BIN_DIR)/test_montecarlo: test/test_montecarlo.cpp $(LIB_DIR)/libathena-core.a | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -L$(LIB_DIR) -lathena-core -o $@

$(BIN_DIR)/test_terrain: test/test_terrain.cpp $(LIB_DIR)/libathena-core.a | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -L$(LIB_DIR) -lathena-core -o $@

$(BIN_DIR)/test_detection: test/test_detection.cpp $(LIB_DIR)/libathena-core.a | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -L$(LIB_DIR) -lathena-core -o $@

$(BIN_DIR)/test_c2: test/test_c2.cpp $(LIB_DIR)/libathena-core.a | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -L$(LIB_DIR) -lathena-core -o $@

$(BIN_DIR)/test_pathfinding: test/test_pathfinding.cpp $(LIB_DIR)/libathena-core.a | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -L$(LIB_DIR) -lathena-core -o $@

$(BIN_DIR)/test_serialization: test/test_serialization.cpp $(LIB_DIR)/libathena-core.a | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -L$(LIB_DIR) -lathena-core -o $@

$(BIN_DIR)/test_sobol: test/test_sobol.cpp $(LIB_DIR)/libathena-core.a | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -L$(LIB_DIR) -lathena-core -o $@

$(BIN_DIR)/test_platform_loader: test/test_platform_loader.cpp $(LIB_DIR)/libathena-core.a | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -L$(LIB_DIR) -lathena-core -o $@

# Convenience target for platform loader test only
test-loader: $(BIN_DIR)/test_platform_loader
	@echo "Running platform loader tests..."
	@$(BIN_DIR)/test_platform_loader

# Clean
clean:
	rm -f $(OBJECTS)
	rm -rf $(BUILD_DIR)

# CLI binary
cli: $(BIN_DIR)/athena-cli
	@echo "CLI built: $(BIN_DIR)/athena-cli"

$(BIN_DIR)/athena-cli: src/main.cpp $(LIB_DIR)/libathena-core.a | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -L$(LIB_DIR) -lathena-core -o $@

# Print configuration (for audit)
info:
	@echo "=== ATHENA Build Configuration ==="
	@echo "Compiler: $(CXX)"
	@$(CXX) --version | head -1
	@echo "Flags: $(CXXFLAGS)"
	@echo "Fast-math: DISABLED"
	@echo "FP-contract: OFF"
	@echo "=================================="

.PHONY: all debug release test clean info

# Refactored Test Makefile
# Builds each src/*.cxx into its own binary in bin/

# ========= Project =========

BUILD_DIR := Object
BIN_DIR := bin

CXX := g++
CC  := gcc

# ========= Sources =========

SRCS := $(wildcard src/*.cxx)
BINS := $(patsubst src/%.cxx,$(BIN_DIR)/%,$(SRCS))
C_SRCS := ../headers/__wayland__/xdg-shell-protocol.c
C_OBJS := $(patsubst ../headers/__wayland__/%.c,$(BUILD_DIR)/%.o,$(C_SRCS))
ASM_SRCS := $(wildcard ../headers/__asm__/*.s)
ASM_OBJS := $(patsubst ../headers/__asm__/%.s,$(BUILD_DIR)/asm_%.o,$(ASM_SRCS))

# ========= Build Modes =========

MODE ?= release

# ========= Flags =========

COMMON_WARNINGS := -Wall -Wextra -Wpedantic -Winvalid-pch
COMMON_OPT := -ffunction-sections -fdata-sections -fmerge-all-constants -fomit-frame-pointer -fno-exceptions -fno-rtti -flto -pipe
COMMON_DEFINES := -DNDEBUG
COMMON_LINK := -Wl,--gc-sections -Wl,--as-needed -Wl,-O1 -flto -s

LIBS := -lglfw -lGL -lm -lvulkan -lwayland-client -lxkbcommon

ifeq ($(MODE),release)
    CXXFLAGS := -std=c++26 -Os -MMD $(COMMON_WARNINGS) $(COMMON_OPT) $(COMMON_DEFINES)
    CFLAGS := -Os -MMD $(COMMON_OPT) $(COMMON_DEFINES)
    LDFLAGS := $(COMMON_LINK)
else
    CXXFLAGS := -std=c++26 -g3 -O0 -MMD $(COMMON_WARNINGS)
    CFLAGS := -g3 -O0 -MMD
    LDFLAGS :=
endif

# ========= Rules =========

.PHONY: all clean compile_all

all: $(BINS)

compile_all: $(BINS)

# Rule for ASM objects
$(BUILD_DIR)/asm_%.o: ../headers/__asm__/%.s | $(BUILD_DIR)
	@echo -e "\x1b[34;1;3;4:2m[ASM]\x1b[0m $<"
	@$(CC) -c $< -o $@

# Rule for C objects (xdg-shell)
$(BUILD_DIR)/%.o: ../headers/__wayland__/%.c | $(BUILD_DIR)
	@echo -e "\x1b[1;3;4:2;92m[C]\x1b[0m $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Rule for each test binary
$(BIN_DIR)/%: src/%.cxx $(ASM_OBJS) $(C_OBJS) | $(BIN_DIR) $(BUILD_DIR)
	@echo -e "\x1b[1;3;4:2;92m[CXX]\x1b[0m $<"
	@$(CXX) $(CXXFLAGS) $< $(ASM_OBJS) $(C_OBJS) $(LDFLAGS) $(LIBS) -o $@

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

clean:
	@rm -rf $(BUILD_DIR) $(BIN_DIR)

# Helper to run a specific test: make run-WindowTest
run-%: $(BIN_DIR)/%
	@./$<

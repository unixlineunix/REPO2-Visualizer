
# I aint smart enough to make this shit though
# GPT make de makefile : works 4 me : file:::

# ========= Project =========

APP_NAME := run.xcbt
BUILD_DIR := Object
BIN_DIR := bin

CXX := g++
CC  := gcc

FILE ?= main

# ========= Sources =========

CXX_SRCS := $(wildcard src/*.cxx)
C_SRCS   := headers/__wayland__/xdg-shell-protocol.c
ASM_SRCS := $(wildcard headers/__asm__/*.s)

CXX_OBJS := $(patsubst src/%.cxx,$(BUILD_DIR)/%.o,$(CXX_SRCS))
C_OBJS   := $(patsubst headers/__wayland__/%.c,$(BUILD_DIR)/%.o,$(C_SRCS))
ASM_OBJS := $(patsubst headers/__asm__/%.s,$(BUILD_DIR)/asm_%.o,$(ASM_SRCS))

OBJS := $(CXX_OBJS) $(C_OBJS) $(ASM_OBJS)

DEPS := $(OBJS:.o=.d)

# ========= Build Modes =========

MODE ?= release

# ========= Common Flags =========

.PHONY: all compile run clean rebuild size debug release testr

all: compile

COMMON_WARNINGS := \
    -Wall \
    -Wextra \
    -Wpedantic \
    -fexceptions \
    -Winvalid-pch 

COMMON_OPT := \
    -ffunction-sections \
    -fdata-sections \
    -fmerge-all-constants \
    -fomit-frame-pointer \
    -fno-rtti \
    -fno-unwind-tables \
    -fno-asynchronous-unwind-tables \
    -flto \
    -pipe

#    -fno-exceptions 

COMMON_DEFINES := \
    -DNDEBUG

COMMON_LINK := \
    -Wl,--gc-sections \
    -Wl,--as-needed \
    -Wl,-O1 \
    -flto \
    -s

LIBS := \
    -lglfw \
    -lGL \
    -lm \
    -lvulkan \
    -lwayland-client \
    -lxkbcommon
    
# ========= Release =========

ifeq ($(MODE),release)

CXXFLAGS := \
    -std=c++26 \
    -Os \
    -MMD \
    -D_GLIBCXX_USE_FLOAT128=0 \
    $(COMMON_WARNINGS) \
    $(COMMON_OPT) \
    $(COMMON_DEFINES)

CFLAGS := \
    -Os \
    -MMD \
    $(COMMON_OPT) \
    $(COMMON_DEFINES)

LDFLAGS := \
    $(COMMON_LINK)

endif

# ========= Debug =========

ifeq ($(MODE),debug)

CXXFLAGS := \
    -std=c++26 \
    -g3 \
    -O0 \
    -MMD \
    -D_GLIBCXX_USE_FLOAT128=0 \
    $(COMMON_WARNINGS)

CFLAGS := \
    -g3 \
    -O0 \
    -MMD

LDFLAGS :=

endif

# ========= Directories =========

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# ========= Compile =========

$(BUILD_DIR)/%.o: src/%.cxx | $(BUILD_DIR)
	@echo -e "\x1b[1;3;4:2;92m[CXX]\x1b[0m $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: headers/__wayland__/%.c | $(BUILD_DIR)
	@echo -e "\x1b[1;3;4:2;92m[C]\x1b[0m $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/asm_%.o: headers/__asm__/%.s | $(BUILD_DIR)
	@echo -e "\x1b[34;1;3;4:2m[ASM]\x1b[0m $<"
	@$(CC) -c $< -o $@

# ========= Link =========

$(BIN_DIR)/$(APP_NAME): $(OBJS) | $(BIN_DIR)
	@echo -e "\x1b[93;1;3;4:2m[LINK]\x1b[0m"
	@$(CXX) $(OBJS) $(LDFLAGS) $(LIBS) -o $@

# ========= Targets =========



compile: $(BIN_DIR)/$(APP_NAME)

run: compile
	@./$(BIN_DIR)/$(APP_NAME)

erun:
	@./$(BIN_DIR)/$(APP_NAME)

debug:
	@$(MAKE) MODE=debug

release:
	@$(MAKE) MODE=release

getsize:
	@echo
	@size $(BIN_DIR)/$(APP_NAME)
	@echo
	@eza -lh $(BIN_DIR)/$(APP_NAME)

rebuild: clean all

clean:
	@rm -rf $(BUILD_DIR) $(BIN_DIR)

# ========= Tests =========

testr:
	@$(MAKE) -C test -f makefile all FILE=$(or $(FILE),types_test)
	@$(MAKE) -C test -f makefile testrun FILE=$(or $(FILE),types_test)

# ========= Dependencies =========

-include $(DEPS)

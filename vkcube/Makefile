CXX       := g++
CC        := gcc
CXXFLAGS  := -std=c++17 -O2 -g
CFLAGS    := -O2 -g
INCLUDES  := -IHellaAssVulkanProject.GodKillMeAlready.WhatYouWaitinFOr_/headers
LDFLAGS   := -lvulkan -lwayland-client -lxkbcommon -ldl -lpthread

PROJ_DIR  := HellaAssVulkanProject.GodKillMeAlready.WhatYouWaitinFOr_
ASM_DIR   := $(PROJ_DIR)/headers/__asm__
WL_DIR    := $(PROJ_DIR)/headers/__wayland__
OBJ_DIR   := Object

TARGET    := vkcube

ASM_SRCS  := $(wildcard $(ASM_DIR)/*.s)
ASM_OBJS  := $(patsubst $(ASM_DIR)/%.s,$(OBJ_DIR)/asm_%.o,$(ASM_SRCS))
C_SRCS    := $(WL_DIR)/xdg-shell-protocol.c
C_OBJS    := $(patsubst $(WL_DIR)/%.c,$(OBJ_DIR)/%.o,$(C_SRCS))

.PHONY: all clean run

all: $(TARGET)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/asm_%.o: $(ASM_DIR)/%.s | $(OBJ_DIR)
	$(CC) -c $< -o $@

$(OBJ_DIR)/%.o: $(WL_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

shaders/vert.spv: shaders/shader.vert
	glslc $< -o $@

shaders/frag.spv: shaders/shader.frag
	glslc $< -o $@

$(TARGET): main.cpp $(ASM_OBJS) $(C_OBJS) shaders/vert.spv shaders/frag.spv
	$(CXX) $(CXXFLAGS) $(INCLUDES) main.cpp $(ASM_OBJS) $(C_OBJS) -o $(TARGET) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(TARGET) shaders/*.spv $(OBJ_DIR)

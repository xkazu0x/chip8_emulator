CC := g++
CFLAGS := -O0 -g -Wextra -Wall -Wno-write-strings -Wno-unused-function -Wno-unused-variable
DEFINES := -DENABLE_DEBUG=1
LIBS := -luser32 -lgdi32

RES_DIR := res
SRC_DIR := src
BUILD_DIR := build

EXEC := chip8.exe

.PHONY: all
all: $(BUILD_DIR) $(BUILD_DIR)/$(EXEC)

$(BUILD_DIR):
	mkdir $@

$(BUILD_DIR)/$(EXEC): $(SRC_DIR)/chip8.cpp
	$(CC) $(CFLAGS) -o $@ $^ $(DEFINES) $(LIBS)

.PHONY: run
run: all
	$(BUILD_DIR)/$(EXEC) $(RES_DIR)/test_opcode.ch8

.PHONY: clean
clean:
	del /q $(BUILD_DIR)\*.*

CC := g++
CFLAGS := -Wextra -Wall -pedantic -Wno-write-strings

RES_DIR := res
SRC_DIR := src
BUILD_DIR := build

EXEC := chip8.exe

.PHONY: all
all: $(BUILD_DIR) $(BUILD_DIR)/$(EXEC)

$(BUILD_DIR):
	mkdir $@

$(BUILD_DIR)/$(EXEC): $(SRC_DIR)/chip8.cpp
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: run
run: all
	$(BUILD_DIR)/$(EXEC) $(RES_DIR)/ibm_logo.ch8

.PHONY: clean
clean:
	del /q $(BUILD_DIR)\*.*

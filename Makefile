CC = gcc
AR = ar

CFLAGS = -std=c11 -Wall -Wextra -Wpedantic
CPPFLAGS = -Iinclude -D_POSIX_C_SOURCE=200809L

LIB = libmonitor.a
BUILD_DIR = build

LIB_OBJS = $(BUILD_DIR)/database.o $(BUILD_DIR)/format.o
TEST_BIN = $(BUILD_DIR)/test_monitor

.PHONY: all test clean

all: $(LIB)

$(LIB): $(LIB_OBJS)
	$(AR) rcs $@ $^

$(BUILD_DIR)/%.o: src/%.c include/monitor.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TEST_BIN): tests/test_monitor.c $(LIB) include/monitor.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -L. -lmonitor -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(BUILD_DIR)/*.o $(TEST_BIN) $(LIB) server client

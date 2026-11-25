CC = gcc
AR = ar

CFLAGS = -std=c11 -Wall -Wextra -Wpedantic
CPPFLAGS = -Iinclude -D_POSIX_C_SOURCE=200809L

LIB = libmonitor.a
SERVER_BIN = server
CLIENT_BIN = client
BUILD_DIR = build

LIB_OBJS = $(BUILD_DIR)/database.o $(BUILD_DIR)/format.o
TEST_BIN = $(BUILD_DIR)/test_monitor
INTEGRATION_TEST_BIN = $(BUILD_DIR)/test_server
DEP_FILES = $(LIB_OBJS:.o=.d) $(BUILD_DIR)/test_monitor.d

.PHONY: all test test-integration clean run

all: $(LIB) $(SERVER_BIN) $(CLIENT_BIN)

$(LIB): $(LIB_OBJS)
	$(AR) rcs $@ $^

$(BUILD_DIR)/%.o: src/%.c include/monitor.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(SERVER_BIN): server.c $(LIB) include/monitor.h include/protocol.h
	$(CC) $(CPPFLAGS) $(CFLAGS) server.c $(LIB) -o $@

$(CLIENT_BIN): client.c include/protocol.h
	$(CC) $(CPPFLAGS) $(CFLAGS) client.c -o $@

$(TEST_BIN): tests/test_monitor.c $(LIB) include/monitor.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF $(BUILD_DIR)/test_monitor.d -MT $@ $< $(LIB) -o $@

$(INTEGRATION_TEST_BIN): tests/test_server.c include/protocol.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

test: all $(TEST_BIN) $(INTEGRATION_TEST_BIN)
	./$(TEST_BIN)
	./$(INTEGRATION_TEST_BIN)

test-integration: all $(INTEGRATION_TEST_BIN)
	./$(INTEGRATION_TEST_BIN)

run: $(SERVER_BIN)
	./$(SERVER_BIN)

clean:
	rm -f $(BUILD_DIR)/*.o $(BUILD_DIR)/*.d $(TEST_BIN) $(TEST_BIN).exe $(INTEGRATION_TEST_BIN) $(INTEGRATION_TEST_BIN).exe $(LIB) $(SERVER_BIN) $(SERVER_BIN).exe $(CLIENT_BIN) $(CLIENT_BIN).exe

-include $(DEP_FILES)

CC     = gcc
CFLAGS = -Wall -Werror -std=c11 -Isrc -Ithirdparty
LDLIBS ?= -lgnurx

SRC_DIR  = src
TEST_DIR = test
OBJ_DIR  = obj
BIN_DIR  = bin

EXEEXT =
ifeq ($(OS),Windows_NT)
EXEEXT = .exe
MKDIR = if not exist "$(subst /,\,$(1))" mkdir "$(subst /,\,$(1))"
RMDIR = if exist "$(subst /,\,$(1))" rmdir /S /Q "$(subst /,\,$(1))"
RUN = .\$(subst /,\,$(1))
else
MKDIR = mkdir -p $(1)
RMDIR = rm -rf $(1)
RUN = ./$(1)
endif

TARGET      = $(BIN_DIR)/chessviz$(EXEEXT)
TEST_TARGET = $(BIN_DIR)/chessviz-test$(EXEEXT)

APP_SRCS = \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/chess_table.c \
	$(SRC_DIR)/move_check.c \
	$(SRC_DIR)/move_logic.c \
	$(SRC_DIR)/parse_logic.c \
	$(SRC_DIR)/game_state.c

LIB_SRCS = \
	$(SRC_DIR)/chess_table.c \
	$(SRC_DIR)/move_check.c \
	$(SRC_DIR)/move_logic.c \
	$(SRC_DIR)/parse_logic.c \
	$(SRC_DIR)/game_state.c

TEST_SRCS = $(TEST_DIR)/unit_tests.c $(TEST_DIR)/unit_tests_custom.c $(LIB_SRCS)

APP_OBJS  = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/src/%.o,$(APP_SRCS))
TEST_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/src/%.o,$(LIB_SRCS)) \
            $(patsubst $(TEST_DIR)/%.c,$(OBJ_DIR)/test/%.o,$(filter $(TEST_DIR)/%.c,$(TEST_SRCS)))

DEPS = \
	$(SRC_DIR)/chess_table.h \
	$(SRC_DIR)/move_check.h \
	$(SRC_DIR)/move_logic.h \
	$(SRC_DIR)/parse_logic.h \
	$(SRC_DIR)/game_state.h

FILE ?=

.PHONY: all clean run test format-check

all: $(TARGET)

$(TARGET): $(APP_OBJS)
	$(call MKDIR,$(BIN_DIR))
	$(CC) $(CFLAGS) $(APP_OBJS) -o $(TARGET) $(LDLIBS)

$(TEST_TARGET): $(TEST_OBJS)
	$(call MKDIR,$(BIN_DIR))
	$(CC) $(CFLAGS) $(TEST_OBJS) -o $(TEST_TARGET) $(LDLIBS)

$(OBJ_DIR)/src/%.o: $(SRC_DIR)/%.c $(DEPS)
	$(call MKDIR,$(OBJ_DIR)/src)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/test/%.o: $(TEST_DIR)/%.c $(DEPS)
	$(call MKDIR,$(OBJ_DIR)/test)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	$(call RUN,$(TARGET)) $(FILE)

test: $(TEST_TARGET)
	$(call RUN,$(TEST_TARGET))

format-check:
ifeq ($(OS),Windows_NT)
	@where clang-format >nul 2>nul && clang-format --dry-run --Werror $(SRC_DIR)/*.c $(SRC_DIR)/*.h $(TEST_DIR)/unit_tests.c thirdparty/ctest.h || echo clang-format not found, skipping local format check
else
	@command -v clang-format >/dev/null 2>&1 || { echo "clang-format not found"; exit 1; }
	clang-format --dry-run --Werror $(SRC_DIR)/*.c $(SRC_DIR)/*.h $(TEST_DIR)/unit_tests.c thirdparty/ctest.h
endif

clean:
	$(call RMDIR,$(OBJ_DIR))
	$(call RMDIR,$(BIN_DIR))

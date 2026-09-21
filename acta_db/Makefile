CC      ?= cc
CFLAGS  ?= -Wall -Wextra -std=c11 -O2 -g
LDLIBS ?= -lsqlite3

# ---- cJSON (override CJSON_DIR / CJSON_LIB to point at a custom build) ----
CJSON_DIR ?=
CJSON_LIB ?= -lcjson
ifneq ($(CJSON_DIR),)
CFLAGS += -I$(CJSON_DIR)
endif
LDLIBS += $(CJSON_LIB)

INC_DIR  := include
SRC_DIR  := src
OBJ_DIR  := build/obj
TEST_DIR := tests
TEST_BIN_DIR := test

LIB_STATIC := libacta_db.a
LIB_SHARED := libacta_db.so

# ─── Library sources ───────────────────────────────────────────────────────────
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# ─── Test sources ──────────────────────────────────────────────────────────────
# All test files (excluding test_common.c and test_main.c which are helpers)
TEST_SRCS := $(wildcard $(TEST_DIR)/*.c)
TEST_MAIN := $(TEST_DIR)/test_main.c


# Module list: each maps to tests/test_<module>.c with run_<module>_tests()
TEST_MODULES := db execution execution_log integration \
                model model_folder model_revision \
                skill skill_folder skill_revision \
                context_deleted execution_deleted \
                light_queries

.PHONY: all clean test test-build test-%

# ─── Library ───────────────────────────────────────────────────────────────────
all: $(LIB_STATIC) $(LIB_SHARED)

$(LIB_STATIC): $(OBJS)
	ar rcs $@ $^

$(LIB_SHARED): $(OBJS)
	$(CC) -shared -o $@ $^ $(LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) -c $< -o $@

# ─── Run all tests (single binary) ────────────────────────────────────────────
test: $(LIB_STATIC) $(TEST_BIN_DIR)/run
	./$(TEST_BIN_DIR)/run

$(TEST_BIN_DIR)/run: $(TEST_SRCS) $(LIB_STATIC)
	@mkdir -p $(TEST_BIN_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ $(TEST_SRCS) -L. -lacta_db $(LDLIBS)

# ─── Run a single module's tests ──────────────────────────────────────────────
# Usage:
#   make test-db
#   make test-execution
#   make test-model_folder
#   make test-integration
#
# A tiny main() is generated on-the-fly so each test_<module>.c
# doesn't need to define its own main.

test-%: $(LIB_STATIC)
	@mkdir -p $(OBJ_DIR)
	@echo "#include <stdio.h>" > $(OBJ_DIR)/_gen_main_$*.c
	@echo "#include \"../tests/test_common.h\"" >> $(OBJ_DIR)/_gen_main_$*.c
	@echo "void run_$*_tests(void);" >> $(OBJ_DIR)/_gen_main_$*.c
	@echo "int main(void) {" >> $(OBJ_DIR)/_gen_main_$*.c
	@echo "    run_$*_tests();" >> $(OBJ_DIR)/_gen_main_$*.c
	@echo '    printf("\\n=== $* TESTS PASSED ===\\n");' >> $(OBJ_DIR)/_gen_main_$*.c
	@echo "    return 0;" >> $(OBJ_DIR)/_gen_main_$*.c
	@echo "}" >> $(OBJ_DIR)/_gen_main_$*.c
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $(TEST_BIN_DIR)/run_$* \
		$(TEST_DIR)/test_$*.c $(OBJ_DIR)/_gen_main_$*.c \
		-L. -lacta_db $(LDLIBS)
	rm -f $(OBJ_DIR)/_gen_main_$*.c
	./$(TEST_BIN_DIR)/run_$*

# ─── Build all test binaries without running ───────────────────────────────────
test-build: $(LIB_STATIC) $(TEST_BIN_DIR)/run
	@for m in $(TEST_MODULES); do \
		mkdir -p $(OBJ_DIR); \
		echo "#include <stdio.h>" > $(OBJ_DIR)/_gen_main_$$m.c; \
		echo "#include \"test_common.h\""" >> $(OBJ_DIR)/_gen_main_$$m.c; \
		echo "void run_$$m_tests(void);" >> $(OBJ_DIR)/_gen_main_$$m.c; \
		echo "int main(void) {" >> $(OBJ_DIR)/_gen_main_$$m.c; \
		echo "    run_$$m_tests();" >> $(OBJ_DIR)/_gen_main_$$m.c; \
		echo '    printf("\\n=== $$m TESTS PASSED ===\\n");' >> $(OBJ_DIR)/_gen_main_$$m.c; \
		echo "    return 0;" >> $(OBJ_DIR)/_gen_main_$$m.c; \
		echo "}" >> $(OBJ_DIR)/_gen_main_$$m.c; \
		$(CC) $(CFLAGS) -I$(INC_DIR) -o $(TEST_BIN_DIR)/run_$$m \
			$(TEST_DIR)/test_$$m.c $(OBJ_DIR)/_gen_main_$$m.c \
			-L. -lacta_db $(LDLIBS); \
		rm -f $(OBJ_DIR)/_gen_main_$$m.c; \
		echo "  built test/run_$$m"; \
	done

# ─── Clean ─────────────────────────────────────────────────────────────────────
clean:
	rm -rf $(OBJ_DIR) $(LIB_STATIC) $(LIB_SHARED) $(TEST_BIN_DIR)

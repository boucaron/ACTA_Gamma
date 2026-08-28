# actagamma_db — POC Makefile
#
#   make          → builds ./actagamma_db
#   make test     → builds + runs all test binaries
#   make clean    → removes objects + binaries
#
# Layout:
#   include/              headers
#   src/                  top-level .c  (argparse.c, json.c, …)
#   src/commands/         per-entity .c
#   tests/skill/          skill CLI tests  (also holds shared skill_test_helpers.*)
#   tests/skill_rev/      skill_revision CLI tests
#   tests/skill_folder/   skill_folder CLI tests
#   tests/model_folder/   model_folder CLI tests
#   tests/context/        context CLI tests
#   tests/db/             db (exec / version / help) CLI tests
#   tests/execution_log/  execution_log CLI tests
#   tests/exec/           execution CLI tests
#   ../acta_db/libacta_db.a
#
# cJSON:
#   System install (pkg-config or /usr/include/cjson/cJSON.h) works out
#   of the box.  Override the paths if it lives elsewhere:
#     make CJSON_DIR=/opt/cjson/include CJSON_LIB=/opt/cjson/lib/libcjson.a

CC      ?= cc
CFLAGS  += -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -g \
           -Iinclude -I../acta_db/include \
           -Itests/skill -Itests/skill_folder -Itests/model_folder \
           -Itests/context -Itests/db \
           -Itests/execution_log -Itests/exec

# ---- cJSON (override CJSON_DIR / CJSON_LIB to point at a custom build)
CJSON_DIR ?=
CJSON_LIB ?= -lcjson
ifneq ($(CJSON_DIR),)
CFLAGS   += -I$(CJSON_DIR)
endif

LDFLAGS += ../acta_db/libacta_db.a -lsqlite3 $(CJSON_LIB)

# ---- build metadata (auto-injected) ----
GIT_HASH    := $(shell git rev-parse --short=8 HEAD 2>/dev/null)
ifeq ($(GIT_HASH),)
  GIT_HASH  := unknown
else ifneq ($(shell git diff --quiet 2>/dev/null && echo y),y)
  GIT_HASH  := $(GIT_HASH)-dirty
endif
CFLAGS    += -DACTA_DB_GIT_HASH=\"$(GIT_HASH)\"

# ---- sources (app) ----
SRCS    := $(wildcard src/*.c)
SRCS   += $(wildcard src/commands/*.c)
OBJS    := $(patsubst %.c, %.o, $(SRCS))
APP_OBJS_NO_MAIN := $(filter-out src/main.o, $(OBJS))
TARGET  := actagamma_db

# ---- sources (tests: skill) ----
TEST_SRCS   := $(wildcard tests/skill/*.c)
TEST_OBJS   := $(patsubst %.c, %.o, $(TEST_SRCS))
TEST_TARGET := test_skill

# ---- sources (tests: skill_revision) ----
SRV_TEST_SRCS   := $(wildcard tests/skill_revision/*.c)
SRV_TEST_OBJS   := $(patsubst %.c, %.o, $(SRV_TEST_SRCS))
SRV_TEST_TARGET := test_skill_rev

# ---- sources (tests: skill_folder) ----
SFL_TEST_SRCS   := $(wildcard tests/skill_folder/*.c)
SFL_TEST_OBJS   := $(patsubst %.c, %.o, $(SFL_TEST_SRCS))
SFL_TEST_TARGET := test_skill_folder

# ---- sources (tests: model_folder) ----
MF_TEST_SRCS   := $(wildcard tests/model_folder/*.c)
MF_TEST_OBJS   := $(patsubst %.c, %.o, $(MF_TEST_SRCS))
MF_TEST_TARGET := test_model_folder

# ---- sources (tests: context) ----
CTX_TEST_SRCS   := $(wildcard tests/context/*.c)
CTX_TEST_OBJS   := $(patsubst %.c, %.o, $(CTX_TEST_SRCS))
CTX_TEST_TARGET := test_context

# ---- sources (tests: db) ----
DB_TEST_SRCS   := $(wildcard tests/db/*.c)
DB_TEST_OBJS   := $(patsubst %.c, %.o, $(DB_TEST_SRCS))
DB_TEST_TARGET := test_db

# ---- sources (tests: execution_log) ----
EL_TEST_SRCS   := $(wildcard tests/execution_log/*.c)
EL_TEST_OBJS   := $(patsubst %.c, %.o, $(EL_TEST_SRCS))
EL_TEST_TARGET := test_execution_log

# ---- sources (tests: exec) ----
EXEC_TEST_SRCS   := $(wildcard tests/exec/*.c)
EXEC_TEST_OBJS   := $(patsubst %.c, %.o, $(EXEC_TEST_SRCS))
EXEC_TEST_TARGET := test_exec

# shared test-framework object (lives in tests/skill/, used by all suites)
HELPERS_OBJ := tests/skill/skill_test_helpers.o

# all test targets (drives the `test` phony + clean)
ALL_TEST_TARGETS := $(TEST_TARGET) $(SRV_TEST_TARGET) $(SFL_TEST_TARGET) \
                    $(MF_TEST_TARGET) $(CTX_TEST_TARGET) $(DB_TEST_TARGET) \
                    $(EL_TEST_TARGET) $(EXEC_TEST_TARGET)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

# ---- test binary: skill ----
$(TEST_TARGET): $(TEST_OBJS) $(APP_OBJS_NO_MAIN)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJS) $(APP_OBJS_NO_MAIN) $(LDFLAGS)

# ---- test binary: skill_revision ----
$(SRV_TEST_TARGET): $(SRV_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN)
	$(CC) $(CFLAGS) -o $@ $(SRV_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(LDFLAGS)

# ---- test binary: skill_folder ----
$(SFL_TEST_TARGET): $(SFL_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN)
	$(CC) $(CFLAGS) -o $@ $(SFL_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(LDFLAGS)

# ---- test binary: model_folder ----
$(MF_TEST_TARGET): $(MF_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN)
	$(CC) $(CFLAGS) -o $@ $(MF_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(LDFLAGS)

# ---- test binary: context ----
$(CTX_TEST_TARGET): $(CTX_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN)
	$(CC) $(CFLAGS) -o $@ $(CTX_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(LDFLAGS)

# ---- test binary: db ----
$(DB_TEST_TARGET): $(DB_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN)
	$(CC) $(CFLAGS) -o $@ $(DB_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(LDFLAGS)

# ---- test binary: execution_log ----
$(EL_TEST_TARGET): $(EL_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN)
	$(CC) $(CFLAGS) -o $@ $(EL_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(LDFLAGS)

# ---- test binary: exec ----
$(EXEC_TEST_TARGET): $(EXEC_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN)
	$(CC) $(CFLAGS) -o $@ $(EXEC_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(LDFLAGS)

test: $(ALL_TEST_TARGETS)
	./$(TEST_TARGET)
	./$(SRV_TEST_TARGET)
	./$(SFL_TEST_TARGET)
	./$(MF_TEST_TARGET)
	./$(CTX_TEST_TARGET)
	./$(DB_TEST_TARGET)
	./$(EL_TEST_TARGET)
	./$(EXEC_TEST_TARGET)

# ---- pattern rule ----
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(SRV_TEST_OBJS) $(SFL_TEST_OBJS) \
	      $(MF_TEST_OBJS) \
	      $(CTX_TEST_OBJS) $(DB_TEST_OBJS) $(EL_TEST_OBJS) \
	      $(EXEC_TEST_OBJS) \
	      $(TARGET) $(ALL_TEST_TARGETS)

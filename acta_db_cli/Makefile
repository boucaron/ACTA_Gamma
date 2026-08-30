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
#   tests/skill/          skill CLI tests
#   tests/helpers/        shared test framework (test_helpers.*)
#   tests/skill_rev/      skill_revision CLI tests
#   tests/skill_folder/   skill_folder CLI tests
#   tests/model/          model CLI tests
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
           -Itests/helpers -Itests/skill_folder \
           -Itests/model_folder \
           -Itests/context -Itests/db \
           -Itests/execution_log -Itests/exec \
           -Itests/model

# ---- cJSON (override CJSON_DIR / CJSON_LIB to point at a custom build)
CJSON_DIR ?=
CJSON_LIB ?= -lcjson
ifneq ($(CJSON_DIR),)
CFLAGS   += -I$(CJSON_DIR)
endif

ACTA_DB_LIB := ../acta_db/libacta_db.a
LDFLAGS += $(ACTA_DB_LIB) -lsqlite3 $(CJSON_LIB)

# Executable targets carry $(EXEEXT): empty under POSIX make, ".exe" under
# Windows (mingw) make, so the same Makefile builds on both.
EXEEXT ?=


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
TARGET  := actagamma_db$(EXEEXT)

# ---- sources (tests: skill) ----
TEST_SRCS   := $(wildcard tests/skill/*.c)
TEST_OBJS   := $(patsubst %.c, %.o, $(TEST_SRCS))
TEST_TARGET := test_skill$(EXEEXT)

# ---- sources (tests: skill_revision) ----
SRV_TEST_SRCS   := $(wildcard tests/skill_revision/*.c)
SRV_TEST_OBJS   := $(patsubst %.c, %.o, $(SRV_TEST_SRCS))
SRV_TEST_TARGET := test_skill_rev$(EXEEXT)

# ---- sources (tests: skill_folder) ----
SFL_TEST_SRCS   := $(wildcard tests/skill_folder/*.c)
SFL_TEST_OBJS   := $(patsubst %.c, %.o, $(SFL_TEST_SRCS))
SFL_TEST_TARGET := test_skill_folder$(EXEEXT)

# ---- sources (tests: model) ----
MOD_TEST_SRCS   := $(wildcard tests/model/*.c)
MOD_TEST_OBJS   := $(patsubst %.c, %.o, $(MOD_TEST_SRCS))
MOD_TEST_TARGET := test_model$(EXEEXT)

# ---- sources (tests: model_folder) ----
MF_TEST_SRCS   := $(wildcard tests/model_folder/*.c)
MF_TEST_OBJS   := $(patsubst %.c, %.o, $(MF_TEST_SRCS))
MF_TEST_TARGET := test_model_folder$(EXEEXT)

# ---- sources (tests: context) ----
CTX_TEST_SRCS   := $(wildcard tests/context/*.c)
CTX_TEST_OBJS   := $(patsubst %.c, %.o, $(CTX_TEST_SRCS))
CTX_TEST_TARGET := test_context$(EXEEXT)

# ---- sources (tests: db) ----
DB_TEST_SRCS   := $(wildcard tests/db/*.c)
DB_TEST_OBJS   := $(patsubst %.c, %.o, $(DB_TEST_SRCS))
DB_TEST_TARGET := test_db$(EXEEXT)

# ---- sources (tests: execution_log) ----
EL_TEST_SRCS   := $(wildcard tests/execution_log/*.c)
EL_TEST_OBJS   := $(patsubst %.c, %.o, $(EL_TEST_SRCS))
EL_TEST_TARGET := test_execution_log$(EXEEXT)

# ---- sources (tests: exec) ----
EXEC_TEST_SRCS   := $(wildcard tests/exec/*.c)
EXEC_TEST_OBJS   := $(patsubst %.c, %.o, $(EXEC_TEST_SRCS))
EXEC_TEST_TARGET := test_exec$(EXEEXT)

# shared test-framework object (lives in tests/helpers/, used by all suites)
HELPERS_OBJ := tests/helpers/test_helpers.o

# all test targets (drives the `test` phony + clean)
ALL_TEST_TARGETS := $(TEST_TARGET) $(SRV_TEST_TARGET) $(SFL_TEST_TARGET) \
                    $(MOD_TEST_TARGET) $(MF_TEST_TARGET) \
                    $(CTX_TEST_TARGET) $(DB_TEST_TARGET) \
                    $(EL_TEST_TARGET) $(EXEC_TEST_TARGET)

.PHONY: all clean test

all: $(TARGET)

# Rebuild the lib when missing or when any of its sources changed.
# (Must stay below `all` or it would become the default goal.)
$(ACTA_DB_LIB): $(wildcard ../acta_db/src/*.c) $(wildcard ../acta_db/include/*.h)
	$(MAKE) -C ../acta_db libacta_db.a

$(TARGET): $(OBJS) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

# ---- test binary: skill ----
$(TEST_TARGET): $(TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(LDFLAGS)

# ---- test binary: skill_revision ----
$(SRV_TEST_TARGET): $(SRV_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(SRV_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(LDFLAGS)

# ---- test binary: skill_folder ----
$(SFL_TEST_TARGET): $(SFL_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(SFL_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(LDFLAGS)

# ---- test binary: model ----
$(MOD_TEST_TARGET): $(MOD_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(MOD_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(LDFLAGS)

# ---- test binary: model_folder ----
$(MF_TEST_TARGET): $(MF_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(MF_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(LDFLAGS)

# ---- test binary: context ----
$(CTX_TEST_TARGET): $(CTX_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(CTX_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(LDFLAGS)

# ---- test binary: db ----
$(DB_TEST_TARGET): $(DB_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(DB_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(LDFLAGS)

# ---- test binary: execution_log ----
$(EL_TEST_TARGET): $(EL_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(EL_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(LDFLAGS)

# ---- test binary: exec ----
$(EXEC_TEST_TARGET): $(EXEC_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(EXEC_TEST_OBJS) $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(LDFLAGS)

test: $(ALL_TEST_TARGETS)
	@fail=0; \
	for t in $(ALL_TEST_TARGETS); do \
		echo "== $$t =="; \
		./$$t || fail=1; \
	done; \
	exit $$fail

# ---- pattern rule ----
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(SRV_TEST_OBJS) $(SFL_TEST_OBJS) \
	      $(MOD_TEST_OBJS) \
	      $(MF_TEST_OBJS) \
	      $(CTX_TEST_OBJS) $(DB_TEST_OBJS) $(EL_TEST_OBJS) \
	      $(EXEC_TEST_OBJS) \
	      $(TARGET) $(ALL_TEST_TARGETS)

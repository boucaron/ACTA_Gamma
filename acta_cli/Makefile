# acta_cli — POC Makefile
#
#   make          → builds ./acta_cli
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
#   tests/tools/          --tools (T4) schema contract tests
#   tests/gparse/         global-parse seam (S2) raw-argv tests
#   tests/json/           json layer (J1) unit tests
#   ../acta_db/libacta_db.a
#
# cJSON:
#   System install (pkg-config or /usr/include/cjson/cJSON.h) works out
#   of the box.  Override the paths if it lives elsewhere:
#     make CJSON_DIR=/opt/cjson/include CJSON_LIB=/opt/cjson/lib/libcjson.a

CC      ?= cc
CFLAGS  += -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -g \
           -Iinclude -I../acta_db/include \
           -Itests/helpers

# ---- test-suite directories (auto-derived) -------------------------
# TEST_DIRS = every tests/<suite>/ directory except tests/helpers/.
# New test suites are picked up automatically: create tests/<suite>/
# with part files (each exposing one run_<suite>_test_<part>() function)
# plus a <suite>_test_main.c that declares and calls them — no Makefile
# edits needed beyond that directory.
TEST_DIRS   := $(filter-out tests/helpers/, $(sort $(wildcard tests/*/)))
TEST_SUITES := $(notdir $(patsubst %/,%,$(TEST_DIRS)))

# one -I per test-suite directory
CFLAGS    += $(foreach d,$(TEST_DIRS),-I$(d))

# ---- cJSON (override CJSON_DIR / CJSON_LIB to point at a custom build)
CJSON_DIR ?=
CJSON_LIB ?= -lcjson
ifneq ($(CJSON_DIR),)
CFLAGS   += -I$(CJSON_DIR)
endif

ACTA_DB_LIB := ../acta_db/libacta_db.a
LDLIBS += $(ACTA_DB_LIB) -lsqlite3 $(CJSON_LIB)

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
TARGET  := acta_cli$(EXEEXT)

# ---- test binaries (one per tests/<suite>/ directory) --------------
# One binary per suite, generated from TEST_DIRS above: each links the
# suite's objects + the shared test framework + the app objects (minus
# main.o) + libacta_db.  Adding a suite = adding a tests/<suite>/
# directory (part files + <suite>_test_main.c); no Makefile edit needed.
TEST_BINS   := $(foreach s,$(TEST_SUITES),test_$(s)$(EXEEXT))

# shared test-framework object (lives in tests/helpers/, used by all suites)
HELPERS_OBJ := tests/helpers/test_helpers.o

define TEST_SUITE
test_$(1)$(EXEEXT): $(patsubst %.c,%.o,$(wildcard tests/$(1)/*.c)) \
                    $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $$@ $$^ $(LDLIBS)
endef
$(foreach s,$(TEST_SUITES),$(eval $(call TEST_SUITE,$(s))))

.PHONY: all clean test

all: $(TARGET)

# Rebuild the lib when missing or when any of its sources changed.
# (Must stay below `all` or it would become the default goal.)
$(ACTA_DB_LIB): $(wildcard ../acta_db/src/*.c) $(wildcard ../acta_db/include/*.h)
	$(MAKE) -C ../acta_db libacta_db.a

$(TARGET): $(OBJS) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

test: $(TEST_BINS)
	@fail=0; \
	for t in $(TEST_BINS); do \
		echo "== $$t =="; \
		./$$t || fail=1; \
	done; \
	exit $$fail

# ---- pattern rule ----
# Header dependencies: a changed header must force every object to
# recompile.  Without these, a struct-layout change (e.g. global_opts_t
# in cli.h) silently leaves stale .o files with mismatched offsets.
HDRS := $(wildcard include/*.h) $(wildcard ../acta_db/include/*.h)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(HELPERS_OBJ) \
	      $(foreach d,$(TEST_DIRS),$(wildcard $(d)/*.o)) \
	      $(TARGET) $(TEST_BINS)

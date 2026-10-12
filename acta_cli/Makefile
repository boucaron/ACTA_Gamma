# acta_cli — POC Makefile
#
#   make          → builds ./acta_cli
#   make test     → builds + runs all test binaries (regenerating the
#                   acta_test_ref.db fixture from acta_test_ref.sql when it is
#                   missing or stale — see the FIXTURE rule below)
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
#   tests/db/             db (init / migrate / version / backup / help)
#                         CLI tests
#   tests/execution_log/  execution_log CLI tests
#   tests/exec/           execution CLI tests
#   tests/tools/          --tools (T4) schema contract tests
#   acta_test_ref.sql     test-fixture source of truth (tracked); the
#                         acta_test_ref.db the suites seed from is generated
#                         from it and stays git-ignored
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

# Closed backend vocabulary (docs/plans/backend_dialects.md §9 decision 2):
# src/commands/model.c validates --backend with the runner's dialect module
# (dialect_from_string / dialect_expected_names) so the vocabulary has ONE
# definition.  Unlike acta_dbpath.{c,h} — which is duplicated verbatim in
# both src trees — dialect.{h,c} lives in acta_runner/src and is compiled
# here by path, the same way acta_gui/src/src.pro compiles run.c, backend.c,
# argparse.c and deathmark.c.  -I../acta_runner/src is needed for dialect.h
# only; -Iinclude comes first and acta_runner/src holds just backend.h and
# dialect.h, so nothing here can shadow an acta_cli header.
CFLAGS    += -I../acta_runner/src

# ---- test-suite directories (auto-derived) -------------------------
# TEST_DIRS = every tests/<suite>/ directory except tests/helpers/.
# `$(wildcard tests/*/)` still matches plain files (tests/ holds the
# acta_test_ref.db fixture too) but appends a slash to directories only,
# so the `$(filter %/,…)` keeps the suites and drops the fixture.  Without
# it the fixture became a "suite": a bogus -Itests/acta_test_ref.db on
# every compile (cc1.exe: warning: not a directory) and a
# `test_acta_test_ref.db` target that cannot link (undefined reference to
# `WinMain`), which broke `make test`.
# New test suites are picked up automatically: create tests/<suite>/
# with part files (each exposing one run_<suite>_test_<part>() function)
# plus a <suite>_test_main.c that declares and calls them — no Makefile
# edits needed beyond that directory.
TEST_DIRS   := $(filter-out tests/helpers/, $(filter %/, $(sort $(wildcard tests/*/))))
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
# The out-of-tree object built from ../acta_runner/src/dialect.c (see the
# rule below).  It is NOT part of $(OBJS) — SRCS only globs acta_cli's own
# sources — so it is added explicitly wherever a link happens.
DIALECT_OBJ := src/dialect.o
APP_OBJS_NO_MAIN := $(filter-out src/main.o, $(OBJS)) $(DIALECT_OBJ)
TARGET  := acta_cli$(EXEEXT)

# ---- default goal ----
# `all` must be the FIRST target in the file, or a bare `make` would build
# the first expanded test-suite binary (test_cli_util) instead of $(TARGET).
.PHONY: all
all: $(TARGET)

# ---- test binaries (one per tests/<suite>/ directory) --------------
# One binary per suite, generated from TEST_DIRS above: each links the
# suite's objects + the shared test framework + the app objects (minus
# main.o) + libacta_db.  Adding a suite = adding a tests/<suite>/
# directory (part files + <suite>_test_main.c); no Makefile edit needed.
TEST_BINS   := $(foreach s,$(TEST_SUITES),test_$(s)$(EXEEXT))

# ---- test fixture: acta_test_ref.db ------------------------------------
# Every suite seeds its scratch DB from the fixture acta_test_ref.db
# (stest_init() copies it to tmp/), so the suites cannot run without it.
# The .db is git-ignored; the tracked acta_test_ref.sql is its source of
# truth (a plain `BEGIN TRANSACTION ... COMMIT` dump), so a fresh clone has no
# fixture and `make test` used to die in stest_init() with errno=2.  This
# rule regenerates it on demand: the .sql is the only file that ever needs
# editing, and the .db is rebuilt whenever it is missing or older.
# Regenerate from scratch (rm first): re-running the dump into a partially
# built .db would fail on the existing tables.
# sqlite3 is required by THIS rule only; without it the build stops with a
# named error instead of letting all 17 suites fail on a missing file.
# Override the tool if it lives elsewhere: make SQLITE3=/path/to/sqlite3
SQLITE3      ?= sqlite3
FIXTURE      := acta_test_ref.db
FIXTURE_SQL  := acta_test_ref.sql

$(FIXTURE): $(FIXTURE_SQL)
	@command -v $(SQLITE3) >/dev/null 2>&1 || { \
		echo "ERROR: '$(SQLITE3)' not found - needed to regenerate $(FIXTURE) from $(FIXTURE_SQL) (override with: make SQLITE3=/path/to/sqlite3 test)"; \
		exit 1; }
	rm -f $@
	$(SQLITE3) $@ < $<


# shared test-framework object (lives in tests/helpers/, used by all suites)
HELPERS_OBJ := tests/helpers/test_helpers.o

define TEST_SUITE
test_$(1)$(EXEEXT): $(patsubst %.c,%.o,$(wildcard tests/$(1)/*.c)) \
                    $(HELPERS_OBJ) $(APP_OBJS_NO_MAIN) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $$@ $$^ $(LDLIBS)
endef
$(foreach s,$(TEST_SUITES),$(eval $(call TEST_SUITE,$(s))))

.PHONY: clean test

# Rebuild the lib when missing or when any of its sources changed.
# (Must stay below `all` or it would become the default goal.)
# The generated acta_db headers (single source of truth: acta_db —
# schema.sql -> include/schema_sql.h, migrations/*.sql ->
# include/migrations_sql.h, via the acta_db Makefile rules) are explicit
# prerequisites so a `make clean` (which removes them) is self-healing
# when acta_cli is built on its own.
../acta_db/include/schema_sql.h:
	$(MAKE) -C ../acta_db include/schema_sql.h
../acta_db/include/migrations_sql.h:
	$(MAKE) -C ../acta_db include/migrations_sql.h

$(ACTA_DB_LIB): $(wildcard ../acta_db/src/*.c) $(wildcard ../acta_db/include/*.h) \
	../acta_db/include/schema_sql.h ../acta_db/include/migrations_sql.h
	$(MAKE) -C ../acta_db libacta_db.a

$(TARGET): $(OBJS) $(DIALECT_OBJ) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(DIALECT_OBJ) $(LDLIBS)

test: $(TEST_BINS) $(FIXTURE)
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
HDRS := $(wildcard include/*.h) $(wildcard ../acta_db/include/*.h) \
        ../acta_runner/src/dialect.h

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

# Explicit rule (an explicit rule always wins over the %.o pattern rule):
# compile the runner's dialect module into an acta_cli-local object.  The
# prerequisites matter — the pattern rule's $(HDRS) already lists dialect.h,
# but a prerequisite list is what makes `src/dialect.o` rebuild when the
# runner's dialect changes.
$(DIALECT_OBJ): ../acta_runner/src/dialect.c ../acta_runner/src/dialect.h
	$(CC) $(CFLAGS) -c ../acta_runner/src/dialect.c -o $@

clean:
	rm -f $(OBJS) $(DIALECT_OBJ) $(HELPERS_OBJ) \
	      $(foreach d,$(TEST_DIRS),$(wildcard $(d)/*.o)) \
	      $(TARGET) $(TEST_BINS)

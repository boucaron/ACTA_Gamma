# acta_runner — LLM execution runner (Plan A)
#
#   make          → builds ./acta_runner
#   make test     → builds + runs the test suites: argparse
#                   (tests/argparse), the phase-2 pipeline (tests/run),
#                   the --pending batch suite (tests/run/test_pending.c)
#                   the stale-`running` sweep suite (tests/run/
#                   test_sweep.c) and the soft-delete claim suite
#                   (tests/run/test_deleted.c)
#   make smoke    → builds tests/llama_smoke (manual check against a LIVE
#                   llama-server; run: tests/llama_smoke <base_url> <api_key>
#                   [model] [prompt] [timeout_sec])
#   make test-e2e → builds + runs the dead-runner end-to-end suite
#                   (spawns real acta_runner child processes; ~10-15 s)
#   make clean    → removes objects + binaries
#
# Layout:
#   include/          runner.h, runner_util.h, argparse.h
#   src/              main.c, argparse.c, run.c, backend.c/h
#   tests/            stub_server.c/h, run/test_run.c, run/test_pending.c
#                     (see tests/README.md)
#   ../acta_db/libacta_db.a
#
# Dependencies: libacta_db (built by ../acta_db), sqlite3, curl, cJSON.
#   Override if a dependency lives elsewhere:
#     make CURL_LIB=/opt/curl/lib/libcurl.a CJSON_DIR=/opt/cjson/include

CC      ?= cc
CFLAGS  ?= -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -O2 -g
CFLAGS  += -Iinclude -I../acta_db/include

ACTA_DB_LIB := ../acta_db/libacta_db.a
LDLIBS += $(ACTA_DB_LIB) -lsqlite3

# ---- curl (override CURL_INC / CURL_LIB to point at a custom build) ----
CURL_INC ?=
CURL_LIB ?= -lcurl
ifneq ($(CURL_INC),)
CFLAGS += -I$(CURL_INC)
endif

# ---- cJSON (override CJSON_DIR / CJSON_LIB to point at a custom build) ----
CJSON_DIR ?=
CJSON_LIB ?= -lcjson
ifneq ($(CJSON_DIR),)
CFLAGS += -I$(CJSON_DIR)
endif
LDLIBS += $(CJSON_LIB) $(CURL_LIB)

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
CFLAGS += -DACTA_RUNNER_GIT_HASH=\"$(GIT_HASH)\"

# ---- sources ----
SRCS := $(wildcard src/*.c)
OBJS := $(patsubst %.c, %.o, $(SRCS))
TARGET := acta_runner$(EXEEXT)

# ---- tests (phase 2): pipeline against the in-process stub server ----
# Links the real src/run.c + src/backend.c (no main.o) with the test
# harness and the stub server.
TEST_SRCS  := tests/run/test_run.c tests/stub_server.c \
              src/run.c src/backend.c src/argparse.c
TEST_OBJS  := $(patsubst %.c, %.o, $(TEST_SRCS))
TEST_TARGET := tests/run/test_run$(EXEEXT)

# ---- pending-batch test suite (R3): run --pending loop + --max clamping ----
PEND_SRCS  := tests/run/test_pending.c tests/stub_server.c \
              src/run.c src/backend.c src/argparse.c
PEND_OBJS  := $(patsubst %.c, %.o, $(PEND_SRCS))
PEND_TARGET := tests/run/test_pending$(EXEEXT)

# ---- stale-running sweep test suite (R4): sweep --stale-seconds ----
# Pure DB + time; no HTTP, no stub server, no backend sources.
SWEEP_SRCS  := tests/run/test_sweep.c src/sweep.c src/argparse.c
SWEEP_OBJS  := $(patsubst %.c, %.o, $(SWEEP_SRCS))
SWEEP_TARGET := tests/run/test_sweep$(EXEEXT)
SWEEP_LDLIBS := $(ACTA_DB_LIB) -lsqlite3

# ---- soft-delete claim test suite: `run <id>` and `run --pending`
#      against soft-deleted `pending` rows (single-claim guard +
#      live-only batch claim), via the in-process stub server --------
DEL_SRCS  := tests/run/test_deleted.c tests/stub_server.c \
             src/run.c src/backend.c src/argparse.c
DEL_OBJS  := $(patsubst %.c, %.o, $(DEL_SRCS))
DEL_TARGET := tests/run/test_deleted$(EXEEXT)
# DEL_LDLIBS is defined with TEST_LDLIBS below (it must come after
# $(SOCK_LIBS) is set, since := expands immediately).

# ---- API key presence policy test suite: unset -> error, empty ->
#      warning (docs/plans/drop-runner-api-key-flag.md)
KEY_SRCS  := tests/run/test_api_key.c tests/stub_server.c \
             src/run.c src/backend.c src/argparse.c
KEY_OBJS  := $(patsubst %.c, %.o, $(KEY_SRCS))
KEY_TARGET := tests/run/test_api_key$(EXEEXT)

# ---- dead-runner end-to-end suite: SIGKILL a real runner, sweep it ----
# Spawns real acta_runner child processes (run, then sweep) against the
# in-process stub server; scratch FILE db in tests/run/. ~10-15 s
# (stale-aging sleep), so it stays out of the default `test` target.
DEAD_SRCS   := tests/run/test_deadrunner.c tests/stub_server.c
DEAD_OBJS   := $(patsubst %.c, %.o, $(DEAD_SRCS))
DEAD_TARGET := tests/run/test_deadrunner$(EXEEXT)

# ---- argparse test suite (R2): pass-1/pass-2 parsing, no DB/HTTP ----
AP_SRCS   := tests/argparse/test_argparse.c src/argparse.c
AP_OBJS   := $(patsubst %.c, %.o, $(AP_SRCS))
AP_TARGET := tests/argparse/test_argparse$(EXEEXT)

# ---- smoke test: manual check against a LIVE OpenAI-compatible server ----
# Reuses src/backend.c; no DB needed.
SMOKE_SRCS  := tests/llama_smoke.c src/backend.c
SMOKE_OBJS  := $(patsubst %.c, %.o, $(SMOKE_SRCS))
SMOKE_TARGET := tests/llama_smoke$(EXEEXT)
SMOKE_LDLIBS := $(CJSON_LIB) $(CURL_LIB)

ifeq ($(OS),Windows_NT)
  SOCK_LIBS := -lws2_32
else
  SOCK_LIBS :=
endif
TEST_LDLIBS := $(ACTA_DB_LIB) -lsqlite3 $(CJSON_LIB) $(CURL_LIB) \
                -lpthread $(SOCK_LIBS)
# The DEL suite links the same stub-server sources as the TEST suite,
# so it gets the same link libraries (curl, cJSON, pthread, and
# -lws2_32 on Windows via $(SOCK_LIBS)).
DEL_LDLIBS := $(TEST_LDLIBS)

.PHONY: all clean test test-e2e smoke

all: $(TARGET)

# Rebuild the lib when missing or when any of its sources changed.
# (Must stay below `all` or it would become the default goal.)
$(ACTA_DB_LIB): $(wildcard ../acta_db/src/*.c) $(wildcard ../acta_db/include/*.h)
	$(MAKE) -C ../acta_db libacta_db.a

$(TARGET): $(OBJS) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(TEST_TARGET): $(TEST_OBJS) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJS) $(TEST_LDLIBS)

$(AP_TARGET): $(AP_OBJS)
	$(CC) $(CFLAGS) -o $@ $(AP_OBJS)

$(PEND_TARGET): $(PEND_OBJS) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(PEND_OBJS) $(TEST_LDLIBS)

$(SWEEP_TARGET): $(SWEEP_OBJS) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(SWEEP_OBJS) $(SWEEP_LDLIBS)

$(DEL_TARGET): $(DEL_OBJS) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(DEL_OBJS) $(DEL_LDLIBS)

$(KEY_TARGET): $(KEY_OBJS) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(KEY_OBJS) $(TEST_LDLIBS)

$(DEAD_TARGET): $(DEAD_OBJS) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(DEAD_OBJS) $(TEST_LDLIBS)

# The test executables resolve libcurl/libcjson/libwinpthread through the
# native PATH. When another mingw (e.g. C:\Program Files\Git\mingw64\bin)
# precedes /mingw64/bin, DLL loading dies with 0xC000007D (the shell
# reports exit 127). Prepend msys2's bin when spawning them; harmless on
# POSIX where /mingw64/bin does not exist.
SPAWN_PATH := PATH='/mingw64/bin:$(PATH)'

test: $(TEST_TARGET) $(AP_TARGET) $(PEND_TARGET) $(SWEEP_TARGET) $(DEL_TARGET) $(KEY_TARGET)
	$(SPAWN_PATH) $(AP_TARGET)
	$(SPAWN_PATH) $(TEST_TARGET)
	$(SPAWN_PATH) $(PEND_TARGET)
	$(SPAWN_PATH) $(SWEEP_TARGET)
	$(SPAWN_PATH) $(DEL_TARGET)
	$(SPAWN_PATH) $(KEY_TARGET)

# End-to-end: build the runner binary first, then run the suite from
# acta_runner/ (the test spawns ./acta_runner[.exe] as child processes).
test-e2e: $(DEAD_TARGET) $(TARGET)
	$(SPAWN_PATH) $(DEAD_TARGET)

$(SMOKE_TARGET): $(SMOKE_OBJS)
	$(CC) $(CFLAGS) -Isrc -o $@ $(SMOKE_OBJS) $(SMOKE_LDLIBS)

smoke: $(SMOKE_TARGET)

# ---- pattern rule ----
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# The smoke test includes backend.h from src/ (not in -I paths).
tests/llama_smoke.o: tests/llama_smoke.c src/backend.h
	$(CC) $(CFLAGS) -Isrc -c $< -o $@

# The pipeline tests include stub_server.h from tests/ (not in -I paths).
tests/run/test_run.o: tests/run/test_run.c tests/stub_server.h
	$(CC) $(CFLAGS) -Itests -c $< -o $@

tests/run/test_pending.o: tests/run/test_pending.c tests/stub_server.h
	$(CC) $(CFLAGS) -Itests -c $< -o $@

tests/run/test_sweep.o: tests/run/test_sweep.c
	$(CC) $(CFLAGS) -c $< -o $@

tests/run/test_deleted.o: tests/run/test_deleted.c tests/stub_server.h
	$(CC) $(CFLAGS) -Itests -c $< -o $@

tests/run/test_deadrunner.o: tests/run/test_deadrunner.c tests/stub_server.h
	$(CC) $(CFLAGS) -Itests -c $< -o $@

tests/run/test_api_key.o: tests/run/test_api_key.c tests/stub_server.h
	$(CC) $(CFLAGS) -Itests -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(TEST_OBJS) $(TEST_TARGET) $(AP_OBJS) \
        $(AP_TARGET) $(PEND_OBJS) $(PEND_TARGET) $(SWEEP_OBJS) \
        $(SWEEP_TARGET) $(DEL_OBJS) $(DEL_TARGET) $(DEAD_OBJS) \
        $(DEAD_TARGET) $(KEY_OBJS) $(KEY_TARGET) \
        $(SMOKE_OBJS) $(SMOKE_TARGET)

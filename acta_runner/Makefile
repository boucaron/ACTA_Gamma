# acta_runner — LLM execution runner (Plan A)
#
#   make          → builds ./acta_runner
#   make test     → builds + runs the test suites: argparse
#                   (tests/argparse), the phase-2 pipeline (tests/run),
#                   the --pending batch suite (tests/run/test_pending.c)
#                   and the stale-`running` sweep suite (tests/run/
#                   test_sweep.c)
#   make smoke    → builds tests/llama_smoke (manual check against a LIVE
#                   llama-server; run: tests/llama_smoke <base_url> <api_key>
#                   [model] [prompt] [timeout_sec])
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
CFLAGS  ?= -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -O2 -g
CFLAGS  += -Iinclude -I../acta_db/include

ACTA_DB_LIB := ../acta_db/libacta_db.a
LDFLAGS += $(ACTA_DB_LIB) -lsqlite3

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
LDFLAGS += $(CJSON_LIB) $(CURL_LIB)

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
SWEEP_LDFLAGS := $(ACTA_DB_LIB) -lsqlite3

# ---- argparse test suite (R2): pass-1/pass-2 parsing, no DB/HTTP ----
AP_SRCS   := tests/argparse/test_argparse.c src/argparse.c
AP_OBJS   := $(patsubst %.c, %.o, $(AP_SRCS))
AP_TARGET := tests/argparse/test_argparse$(EXEEXT)

# ---- smoke test: manual check against a LIVE OpenAI-compatible server ----
# Reuses src/backend.c; no DB needed.
SMOKE_SRCS  := tests/llama_smoke.c src/backend.c
SMOKE_OBJS  := $(patsubst %.c, %.o, $(SMOKE_SRCS))
SMOKE_TARGET := tests/llama_smoke$(EXEEXT)
SMOKE_LDFLAGS := $(CJSON_LIB) $(CURL_LIB)

ifeq ($(OS),Windows_NT)
  SOCK_LIBS := -lws2_32
else
  SOCK_LIBS :=
endif
TEST_LDFLAGS := $(ACTA_DB_LIB) -lsqlite3 $(CJSON_LIB) $(CURL_LIB) \
                -lpthread $(SOCK_LIBS)

.PHONY: all clean test smoke

all: $(TARGET)

# Rebuild the lib when missing or when any of its sources changed.
# (Must stay below `all` or it would become the default goal.)
$(ACTA_DB_LIB): $(wildcard ../acta_db/src/*.c) $(wildcard ../acta_db/include/*.h)
	$(MAKE) -C ../acta_db libacta_db.a

$(TARGET): $(OBJS) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(TEST_TARGET): $(TEST_OBJS) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJS) $(TEST_LDFLAGS)

$(AP_TARGET): $(AP_OBJS)
	$(CC) $(CFLAGS) -o $@ $(AP_OBJS)

$(PEND_TARGET): $(PEND_OBJS) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(PEND_OBJS) $(TEST_LDFLAGS)

$(SWEEP_TARGET): $(SWEEP_OBJS) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(SWEEP_OBJS) $(SWEEP_LDFLAGS)

test: $(TEST_TARGET) $(AP_TARGET) $(PEND_TARGET) $(SWEEP_TARGET)
	$(AP_TARGET)
	$(TEST_TARGET)
	$(PEND_TARGET)
	$(SWEEP_TARGET)

$(SMOKE_TARGET): $(SMOKE_OBJS)
	$(CC) $(CFLAGS) -Isrc -o $@ $(SMOKE_OBJS) $(SMOKE_LDFLAGS)

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

clean:
	rm -f $(OBJS) $(TARGET) $(TEST_OBJS) $(TEST_TARGET) $(AP_OBJS) \
        $(AP_TARGET) $(PEND_OBJS) $(PEND_TARGET) $(SWEEP_OBJS) \
        $(SWEEP_TARGET) $(SMOKE_OBJS) $(SMOKE_TARGET)

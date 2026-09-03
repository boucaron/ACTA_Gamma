# acta_runner — LLM execution runner (Plan A)
#
#   make          → builds ./acta_runner
#   make clean    → removes objects + binary
#
# Layout:
#   include/          runner.h, runner_util.h, argparse.h
#   src/              main.c, argparse.c, run.c
#   tests/            (phase 2: argparse smoke tests, mock-backend tests)
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

.PHONY: all clean

all: $(TARGET)

# Rebuild the lib when missing or when any of its sources changed.
# (Must stay below `all` or it would become the default goal.)
$(ACTA_DB_LIB): $(wildcard ../acta_db/src/*.c) $(wildcard ../acta_db/include/*.h)
	$(MAKE) -C ../acta_db libacta_db.a

$(TARGET): $(OBJS) $(ACTA_DB_LIB)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

# ---- pattern rule ----
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

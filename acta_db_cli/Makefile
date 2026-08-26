# actagamma_db — POC Makefile
#
#   make          → builds ./actagamma_db
#   make test     → builds + runs ./test_skill
#   make clean    → removes objects + binaries
#
# Layout:
#   include/        headers
#   src/            top-level .c  (argparse.c, json.c, …)
#   src/commands/   per-entity .c
#   tests/skill/    skill CLI tests
#   ../acta_db/libacta_db.a
#
# cJSON:
#   System install (pkg-config or /usr/include/cjson/cJSON.h) works out
#   of the box.  Override the paths if it lives elsewhere:
#     make CJSON_DIR=/opt/cjson/include CJSON_LIB=/opt/cjson/lib/libcjson.a

CC      ?= cc
CFLAGS  += -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -g \
           -Iinclude -I../acta_db/include

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

# ---- sources (tests) ----
TEST_SRCS   := $(wildcard tests/skill/*.c)
TEST_OBJS   := $(patsubst %.c, %.o, $(TEST_SRCS))
TEST_TARGET := test_skill

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

# ---- test binary (links app objects minus main + test objects) ----
$(TEST_TARGET): $(TEST_OBJS) $(APP_OBJS_NO_MAIN)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJS) $(APP_OBJS_NO_MAIN) $(LDFLAGS)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

# ---- pattern rule ----
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TARGET) $(TEST_TARGET)

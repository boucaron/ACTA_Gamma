# actagamma_db — POC Makefile
#
#   make          → builds ./actagamma_db
#   make test     → builds + runs ./test_skill, ./test_skill_rev, ./test_skill_folder
#   make clean    → removes objects + binaries
#
# Layout:
#   include/          headers
#   src/              top-level .c  (argparse.c, json.c, …)
#   src/commands/     per-entity .c
#   tests/skill/      skill CLI tests
#   tests/skill_rev/  skill_revision CLI tests
#   tests/skill_folder/  skill_folder CLI tests
#   ../acta_db/libacta_db.a
#
# cJSON:
#   System install (pkg-config or /usr/include/cjson/cJSON.h) works out
#   of the box.  Override the paths if it lives elsewhere:
#     make CJSON_DIR=/opt/cjson/include CJSON_LIB=/opt/cjson/lib/libcjson.a

CC      ?= cc
CFLAGS  += -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -g \
           -Iinclude -I../acta_db/include \
           -Itests/skill -Itests/skill_folder

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

# shared test-framework object (lives in tests/skill/, used by all suites)
HELPERS_OBJ := tests/skill/skill_test_helpers.o

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

test: $(TEST_TARGET) $(SRV_TEST_TARGET) $(SFL_TEST_TARGET)
	./$(TEST_TARGET)
	./$(SRV_TEST_TARGET)
	./$(SFL_TEST_TARGET)

# ---- pattern rule ----
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(SRV_TEST_OBJS) $(SFL_TEST_OBJS) \
	      $(TARGET) $(TEST_TARGET) $(SRV_TEST_TARGET) $(SFL_TEST_TARGET)

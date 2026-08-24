# actagamma_db — POC Makefile
#
#   make          → builds ./actagamma_db
#   make clean    → removes objects + binary
#
# Layout:
#   include/   headers
#   src/       top-level .c
#   src/commands/  per-entity .c
#   ../libacta_db.a

CC      ?= cc
CFLAGS  += -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -g -Iinclude
LDFLAGS += ../acta_db/libacta_db.a -lsqlite3

# ---- sources ----
SRCS    := $(wildcard src/*.c)
SRCS   += $(wildcard src/commands/*.c)
OBJS    := $(patsubst %.c, %.o, $(SRCS))
TARGET  := actagamma_db

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

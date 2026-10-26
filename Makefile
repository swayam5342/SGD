CC       ?= gcc
CFLAGS   ?= -std=c11 -O2 -g -Wall -Wextra -Wpedantic
CPPFLAGS += -Isrc/lib
BUILD    := build
OBJ      := $(BUILD)/obj

LIB_HDRS := $(wildcard src/lib/*.h)
LIB_OBJS := $(patsubst src/lib/%.c,$(OBJ)/lib/%.o,$(wildcard src/lib/*.c))
LIBSG    := $(BUILD)/libsg.a

BINS      := $(BUILD)/sg-launch $(BUILD)/notify_openat

.PHONY: all test clean

all: $(BINS)

$(BUILD) $(OBJ)/lib:
	mkdir -p $@

$(OBJ)/lib/%.o: src/lib/%.c $(LIB_HDRS) | $(OBJ)/lib
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(LIBSG): $(LIB_OBJS)
	$(AR) rcs $@ $^

$(BUILD)/sg-launch: $(BUILD)/%: src/%.c $(LIBSG) $(LIB_HDRS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LIBSG)

$(BUILD)/notify_openat: examples/notify_openat.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $<

test: all
	sh tests/notify_openat_test.sh

clean:
	rm -rf $(BUILD)

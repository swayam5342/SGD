CC       ?= gcc
CFLAGS   ?= -std=c11 -O2 -g -Wall -Wextra -Wpedantic
CPPFLAGS += -Isrc/lib
BUILD    := build
OBJ      := $(BUILD)/obj

LIB_HDRS := $(wildcard src/lib/*.h)
LIB_OBJS := $(patsubst src/lib/%.c,$(OBJ)/lib/%.o,$(wildcard src/lib/*.c))
SGD_HDRS := $(wildcard src/sgd/*.h)
SGD_OBJS := $(patsubst src/sgd/%.c,$(OBJ)/sgd/%.o,$(wildcard src/sgd/*.c))
LIBSG    := $(BUILD)/libsg.a

BINS      := $(BUILD)/sgd $(BUILD)/sg-launch $(BUILD)/sgctl $(BUILD)/notify_openat

.PHONY: all test clean

all: $(BINS)

$(BUILD) $(OBJ)/lib $(OBJ)/sgd:
	mkdir -p $@

$(OBJ)/lib/%.o: src/lib/%.c $(LIB_HDRS) | $(OBJ)/lib
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(LIBSG): $(LIB_OBJS)
	$(AR) rcs $@ $^

$(OBJ)/sgd/%.o: src/sgd/%.c $(SGD_HDRS) $(LIB_HDRS) | $(OBJ)/sgd
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD)/sgd: $(SGD_OBJS) $(LIBSG)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/sg-launch $(BUILD)/sgctl: $(BUILD)/%: src/%.c $(LIBSG) $(LIB_HDRS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LIBSG)

$(BUILD)/notify_openat: examples/notify_openat.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $<

test: all
	sh tests/notify_openat_test.sh

clean:
	rm -rf $(BUILD)

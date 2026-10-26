CC       ?= gcc
CFLAGS   ?= -std=c11 -O2 -g -Wall -Wextra -Wpedantic
BUILD    := build

BINS      := $(BUILD)/notify_openat

.PHONY: all test clean

all: $(BINS)

$(BUILD):
	mkdir -p $@

$(BUILD)/notify_openat: examples/notify_openat.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $<

test: all
	sh tests/notify_openat_test.sh

clean:
	rm -rf $(BUILD)

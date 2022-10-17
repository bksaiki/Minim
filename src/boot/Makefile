#
#	Makefile for the bootstrap interpreter
#

GC_DIR		:= ../gc
BUILD_DIR	:= build

SRCS 		:= $(shell find . -name "*.c")
OBJS 		:= $(shell find $(GC_DIR) -name "*.c")

LDFLAGS 	:= -L$(GC_DIR) -lminimgc

MKDIR_P	:= mkdir -p
RM := rm -rf

.PRECIOUS: $(BUILD_DIR)/. $(BUILD_DIR)%/.
.SECONDEXPANSION: $(BUILD_DIR)/%.o

all: boot boot-info;

boot: build/boot.o
	$(CC) $(CFLAGS) -o boot $(LDFLAGS) build/boot.o

boot-info: build/boot-info.o
	$(CC) $(CFLAGS) -o boot $(LDFLAGS) build/boot-info.o

clean:
	$(RM) build

$(BUILD_DIR)/.:
	$(MKDIR_P) $@

$(BUILD_DIR)%/.:
	$(MKDIR_P) $@

$(BUILD_DIR)/%.o: %.c | $$(@D)/.
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

.PHONY: build clean

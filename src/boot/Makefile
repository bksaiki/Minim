#
#	Makefile for the bootstrap interpreter
#

GC_DIR		:= $(shell pwd)/../gc
SRC_DIR		:= c
BUILD_DIR	:= build
TEST_DIR	:= test
BOOT_DIR	:= $(shell pwd)/s

ENTRY		:= $(SRC_DIR)/minim.c
CONFIG		:= $(BUILD_DIR)/config.h

SRCS 		:= $(shell find $(SRC_DIR) -name "*.c" ! -wholename $(ENTRY))
OBJS 		:= $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS 		:= $(OBJS:.o=.d)

TESTS		:= $(shell find $(TEST_DIR) -name "*.c")
TEST_EXES	:= $(TESTS:%.c=$(BUILD_DIR)/%)

CFLAGS		:= -Wall -std=c11 -O0 -g
DEPFLAGS 	:= -MMD -MP
LDFLAGS 	:= -L$(GC_DIR) -lminimgc

MKDIR_P		:= mkdir -p
RM 			:= rm -rf
READLINK	:= readlink -f

.PRECIOUS: $(BUILD_DIR)/. $(BUILD_DIR)%/.
.SECONDEXPANSION: $(BUILD_DIR)/%.o

boot: $(CONFIG) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(ENTRY) $(LDFLAGS) -o minim

test: $(CONFIG) $(BUILD_DIR)/test-prims;
	$(BUILD_DIR)/test-prims

clean:
	$(RM) build

$(CONFIG):
	$(MKDIR_P) $(BUILD_DIR)
	echo "#define BOOT_DIR \"$(BOOT_DIR)/\"" >> $@

$(BUILD_DIR)/.:
	$(MKDIR_P) $@

$(BUILD_DIR)%/.:
	$(MKDIR_P) $@

$(BUILD_DIR)/$(SRC_DIR)/%.o: $(SRC_DIR)/%.c | $$(@D)/.
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(BUILD_DIR)/%: $(TEST_DIR)/%.c $(OBJS)
	$(CC) -g $(CFLAGS) $(DEPFLAGS) -o $@ $(OBJS) $< $(LDFLAGS)

.PHONY: build clean

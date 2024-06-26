#
#	Makefile for the bootstrap interpreter
#

SRC_DIR	= c
BUILD_DIR = build
TEST_DIR = test
BOOT_DIR = $(shell pwd)/s

GC_DIR = $(shell pwd)/../gc
CORE_DIR = $(shell pwd)/../core

ENTRY = $(SRC_DIR)/minim.c
CONFIG = $(BUILD_DIR)/config.h

SRCS = $(shell find $(SRC_DIR) -name "*.c" ! -wholename $(ENTRY))
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS = $(OBJS:.o=.d)

CFLAGS += -Wall -std=c11 -O2 -g
DEPFLAGS += -MMD -MP
LDFLAGS += -L$(GC_DIR) -L$(CORE_DIR) -lgc -lminim

EXENAME = minim
MKDIR_P = mkdir -p
RM = rm -rf
READLINK = readlink -f

.PRECIOUS: $(BUILD_DIR)/. $(BUILD_DIR)%/.
.SECONDEXPANSION: $(BUILD_DIR)/%.o

all: $(CONFIG) $(OBJS)
	$(CC) $(CFLAGS) $(PROFILE) $(OBJS) $(ENTRY) $(LDFLAGS) -o $(EXENAME)

profile:
	$(MAKE) CFLAGS="$(CFLAGS) -pg" all

test: $(CONFIG) $(BUILD_DIR)/jit $(BUILD_DIR)/prims $(BUILD_DIR)/fasl
	$(BUILD_DIR)/jit
	$(BUILD_DIR)/prims
	$(BUILD_DIR)/fasl

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
	$(CC) $(CFLAGS) $(PROFILE) $(DEPFLAGS) -c -o $@ $<

$(BUILD_DIR)/%: $(TEST_DIR)/%.c $(OBJS) $(GC_DIR)/libgc.a $(CORE_DIR)/libminim.a
	$(CC) -g $(CFLAGS) $(PROFILE) $(DEPFLAGS) -o $@ $(OBJS) $< $(LDFLAGS)

.PHONY: clean core profile test

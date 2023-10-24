# Minim Project
# Top-level Makefile

SRC_DIR = src
GC_DIR = bdwgc
BUILD_DIR = build

ENTRY = $(SRC_DIR)/main.c
OBJNAME = minim

SRCS = $(shell find $(SRC_DIR) -name "*.c" ! -wholename $(ENTRY) )
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS = $(OBJS:.o=.d)

CFLAGS = -Wall -std=c11 -O0 -g
DEPFLAGS = -MMD -MP
LDFLAGS = -L$(GC_DIR)/.libs -lgc

CP = cp
ECHO = echo
MKDIR_P	= mkdir -p
RM = rm -rf
SH = bash
FIND = find

.PRECIOUS: $(BUILD_DIR)/. $(BUILD_DIR)%/.
.SECONDEXPANSION: $(BUILD_DIR)/%.o

all: core

core: gc $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(ENTRY) $(LDFLAGS) -o $(OBJNAME)

gc: $(GC_DIR)/Makefile
	$(MAKE) -C $(GC_DIR)

clean:
	$(RM) $(BUILD_DIR)

clean-all:
	$(MAKE) -C $(GC_DIR) clean

$(GC_DIR)/Makefile:
	cd $(GC_DIR) && ./autogen.sh && ./configure --enable-static=yes --enable-shared=no

$(BUILD_DIR)/.:
	$(MKDIR_P) $@

$(BUILD_DIR)%/.:
	$(MKDIR_P) $@

$(BUILD_DIR)/$(SRC_DIR)/%.o: $(SRC_DIR)/%.c | $$(@D)/.
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

PHONY: gc clean clean-all

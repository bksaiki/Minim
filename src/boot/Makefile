#
#	Makefile for the bootstrap interpreter
#

GC_DIR		:= ../gc
BUILD_DIR	:= build

ENTRY		:= minim.c
CONFIG		:= $(BUILD_DIR)/config.h

SRCS 		:= boot.c intern.c io.c
OBJS 		:= $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS 		:= $(OBJS:.o=.d)

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
	echo "#define BOOT_PRELUDE \"$(shell $(READLINK) prelude.min)\"" >> $@

$(BUILD_DIR)/.:
	$(MKDIR_P) $@

$(BUILD_DIR)%/.:
	$(MKDIR_P) $@

$(BUILD_DIR)/%.o: %.c | $$(@D)/.
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(BUILD_DIR)/test-prims: test-prims.c $(OBJS)
	$(CC) $(CFLAGS) $(DEPFLAGS) -o $@ $(OBJS) $< $(LDFLAGS)

.PHONY: build clean

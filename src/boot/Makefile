#
#	Makefile for the bootstrap interpreter
#

GC_DIR		:= ../gc
BUILD_DIR	:= build

SRCS 		:= boot.c intern.c
OBJS 		:= $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS 		:= $(OBJS:.o=.d)

DEPFLAGS 	:= -MMD -MP
LDFLAGS 	:= -L$(GC_DIR) -lminimgc

MKDIR_P	:= mkdir -p
RM := rm -rf

.PRECIOUS: $(BUILD_DIR)/. $(BUILD_DIR)%/.
.SECONDEXPANSION: $(BUILD_DIR)/%.o

boot: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o boot

clean:
	$(RM) build

$(BUILD_DIR)/.:
	$(MKDIR_P) $@

$(BUILD_DIR)%/.:
	$(MKDIR_P) $@

$(BUILD_DIR)/%.o: %.c | $$(@D)/.
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

.PHONY: build clean

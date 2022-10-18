#
#	Makefile for the bootstrap interpreter
#

GC_DIR		:= ../gc
BUILD_DIR	:= build

SRCS 		:= intern.o
OBJS 		:= $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS 		:= $(OBJS:.o=.d)

DEPFLAGS 	:= -MMD -MP
LDFLAGS 	:= -L$(GC_DIR) -lminimgc

MKDIR_P	:= mkdir -p
RM := rm -rf

.PRECIOUS: $(BUILD_DIR)/. $(BUILD_DIR)%/.
.SECONDEXPANSION: $(BUILD_DIR)/%.o

all: boot boot-info;

boot: build/boot.o $(OBJS)
	$(CC) $(CFLAGS) build/boot.o $(OBJS) $(LDFLAGS) -o boot

clean:
	$(RM) build

$(BUILD_DIR)/.:
	$(MKDIR_P) $@

$(BUILD_DIR)%/.:
	$(MKDIR_P) $@

$(BUILD_DIR)/%.o: %.c | $$(@D)/.
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

.PHONY: build clean

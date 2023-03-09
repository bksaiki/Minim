#
#	Makefile for the core Minim library
#

GC_DIR		:= $(shell pwd)/../gc
SRC_DIR		:= c
BUILD_DIR	:= build
TEST_DIR	:= test
BOOT_DIR	:= $(shell pwd)/s

CONFIG		:= $(BUILD_DIR)/config.h
LIBNAME		:= libminim.a

SRCS 		:= $(shell find $(SRC_DIR) -name "*.c")
OBJS 		:= $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS 		:= $(OBJS:.o=.d)

CFLAGS		:= -Wall -std=c11 -O0 -g
DEPFLAGS 	:= -MMD -MP
LDFLAGS 	:= -L$(GC_DIR) -lgc

MKDIR_P		:= mkdir -p
RM 			:= rm -rf
READLINK	:= readlink -f

.PRECIOUS: $(BUILD_DIR)/. $(BUILD_DIR)%/.
.SECONDEXPANSION: $(BUILD_DIR)/%.o

all: $(LIBNAME)

$(LIBNAME): $(CONFIG) $(OBJS)
	ar -rc $(LIBNAME) $(OBJS)

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

.PHONY: all build clean

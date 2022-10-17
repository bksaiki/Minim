#
#	Makefile for the garbage collector
#

BUILD_DIR	:= build
LIBNAME		:= libminimgc.a

SRCS 		:= gc.c gc-impl.c
OBJS 		:= $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS 		:= $(OBJS:.o=.d)

MKDIR_P	:= mkdir -p
RM := rm -rf

.PRECIOUS: $(BUILD_DIR)/. $(BUILD_DIR)%/.
.SECONDEXPANSION: $(BUILD_DIR)/%.o

all: $(OBJS)
	ar -rc $(LIBNAME) $(OBJS)

clean:
	$(RM) build $(LIBNAME)

$(BUILD_DIR)/.:
	$(MKDIR_P) $@

$(BUILD_DIR)%/.:
	$(MKDIR_P) $@

$(BUILD_DIR)/%.o: %.c | $$(@D)/.
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

.PHONY: build clean

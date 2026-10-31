BUILD_DIR = build
GC_DIR = bdwgc
SRC_DIR = src
TEST_DIR = tests

CFLAGS ?= -g -Wall -std=c11 -O3 -march=native
DEPFLAGS ?= -MMD -MP
INCFLAGS ?= -I$(GC_DIR)/include
LDFLAGS ?= -L$(BUILD_DIR) -lgc

ENTRY = $(SRC_DIR)/minim.c
CONFIG = $(BUILD_DIR)/config.h
EXENAME = minim

SRCS = $(shell find $(SRC_DIR) -name "*.c" ! -wholename $(ENTRY))
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS = $(OBJS:.o=.d)

TESTS = $(shell find $(TEST_DIR) -name "*.c" -printf "%f\n")
TEST_OBJS = $(TESTS:%.c=$(BUILD_DIR)/%)

MKDIR_P	= mkdir -p
RM = rm -rf

.PRECIOUS: $(BUILD_DIR)/. $(BUILD_DIR)%/.
.SECONDEXPANSION: $(BUILD_DIR)/%.o

all: $(EXENAME)

base: $(CONFIG) gc

clean:
	$(RM) $(BUILD_DIR)

clean-all: clean
	$(MAKE) -C $(GC_DIR) clean

gc: $(BUILD_DIR)/libgc.a

test: base $(OBJS) $(TEST_OBJS)
	$(TEST_DIR)/unit.sh $(TEST_OBJS)
	
$(EXENAME): base $(OBJS)
	$(CC) $(CFLAGS) $(INCFLAGS) $(OBJS) $(ENTRY) $(LDFLAGS) -o $(EXENAME)

$(GC_DIR)/Makefile:
	cd $(GC_DIR) && ./autogen.sh && ./configure --enable-static=yes --enable-shared=no

$(BUILD_DIR)/libgc.a: $(GC_DIR)/Makefile
	$(MAKE) -C $(GC_DIR)
	cp $(GC_DIR)/.libs/libgc.a $(BUILD_DIR)/libgc.a
 
$(CONFIG): $(BUILD_DIR)
	echo "#define MINIM_X86_64 1" >> $@
	echo "#define KERNEL_PATH \"$(realpath $(SRC_DIR))/kernel.min\"" >> $@

$(BUILD_DIR):
	$(MKDIR_P) $@

$(BUILD_DIR)/.:
	$(MKDIR_P) $@

$(BUILD_DIR)%/.:
	$(MKDIR_P) $@

$(BUILD_DIR)/$(SRC_DIR)/%.o: $(SRC_DIR)/%.c | $$(@D)/.
	$(CC) $(CFLAGS) $(INCFLAGS) $(DEPFLAGS) -c -o $@ $<

$(BUILD_DIR)/%: $(TEST_DIR)/%.c $(OBJS)
	$(CC) $(CFLAGS) $(INCFLAGS) $(DEPFLAGS) -o $@ $(OBJS) $< $(LDFLAGS)

.PHONY: all base clean clean-all gc test

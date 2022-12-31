#
#	Top-level Makefile
#

BUILD_DIR	:= build
BOOT_DIR	:= src/boot
SRC_DIR 	:= src
TEST_DIR	:= tests
INSTALL_DIR := /usr/bin

ENTRY		:= src/minim.c
EXE         := minim

SRCS 		:= $(shell find $(SRC_DIR) -name "*.c" ! -path "src/boot/*" ! -wholename $(ENTRY))
OBJS 		:= $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS 		:= $(OBJS:.o=.d)

TESTS 		:= $(shell find $(TEST_DIR) -name "*.c")
TEST_EXES   := $(TESTS:$(TEST_DIR)/%.c=$(BUILD_DIR)/%)

DEPFLAGS 	:= -MMD -MP
CFLAGS 		:= -Wall -std=c11
LDFLAGS 	:= -lm -lgmp

DEBUG_FLAGS		:= -O2 -g -DENABLE_STATS
PROFILE_FLAGS	:= -O2 -DNDEBUG -march=native -pg
COVERAGE_FLAGS	:= -O2 -g -march=native -fprofile-arcs -ftest-coverage
RELEASE_FLAGS 	:= -O2 -DNDEBUG -march=native

CP := cp
ECHO := echo
MKDIR_P	:= mkdir -p
RM := rm -rf
SH := bash
FIND := find

.PRECIOUS: $(BUILD_DIR)/. $(BUILD_DIR)%/.
.SECONDEXPANSION: $(BUILD_DIR)/%.o

# Top level rules

debug: boot
	$(MAKE) CFLAGS="$(DEBUG_FLAGS) $(CFLAGS) -DUSE_MINIM_GC" minim

profile: boot
	$(MAKE) CFLAGS="$(PROFILE_FLAGS) $(CFLAGS) -DUSE_MINIM_GC" minim

coverage: boot
	$(MAKE) CFLAGS="$(COVERAGE_FLAGS) $(CFLAGS) -DUSE_MINIM_GC" minim

release: boot
	$(MAKE) CFLAGS="$(RELEASE_FLAGS) $(CFLAGS) -DUSE_MINIM_GC" minim

install:
	$(CP) $(EXE) $(INSTALL_DIR)/$(EXE)

boot: gc
	$(MAKE) CFLAGS="$(DEBUG_FLAGS) $(CFLAGS)" -C src/boot

gc: boehm-gc minim-gc
	cp boehm-gc/.libs/libgc.a src/gc/
	cp src/gc/minim-gc/libminimgc.a src/gc/

boehm-gc: boehm-gc/Makefile
	$(MAKE) -C boehm-gc

boehm-gc/Makefile:
	cd boehm-gc && ./autogen.sh && ./configure --enable-static=yes --enable-shared=no

minim-gc:
	$(MAKE) CFLAGS="$(DEBUG_FLAGS) $(CFLAGS)" -C src/gc

minim: $(BUILD_DIR)/config.h $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(ENTRY) $(LDFLAGS) -o $(EXE)

test: minim
	$(MAKE) boot-tests
	$(MAKE) unit-tests
	$(MAKE) lib-tests

boot-tests:
	$(MAKE) CFLAGS="$(DEBUG_FLAGS) $(CFLAGS)" -C src/boot test

unit-tests: $(TEST_EXES)
	$(MAKE) -C src/boot test
	$(SH) $(TEST_DIR)/test.sh $(TEST_EXES)

memcheck: $(TEST_EXES)
	$(SH) $(TEST_DIR)/memcheck.sh $(TEST_EXES)

examples:
	$(SH) $(TEST_DIR)/examples.sh

lib-tests:
	$(SH) $(TEST_DIR)/lib.sh

clean:
	$(MAKE) -C boehm-gc clean
	$(MAKE) -C src/gc clean
	$(MAKE) -C src/boot clean
	$(RM) $(OBJS) $(EXE)

clean-all: clean clean-cache
	$(RM) boehm-gc/Makefile
	$(RM) $(BUILD_DIR) tmp

clean-cache:
	$(FIND) . -type d -name ".cache" | xargs $(RM)

uninstall:
	$(RM) $(INSTALL_DIR)/$(EXE)

### Specific rules

$(BUILD_DIR)/config.h:
	$(MKDIR_P) $(BUILD_DIR)
	$(ECHO) "#define MINIM_LIB_PATH \"$(shell pwd)/src/\"" > build/config.h

### General rules

$(BUILD_DIR)/.:
	$(MKDIR_P) $@

$(BUILD_DIR)%/.:
	$(MKDIR_P) $@

$(BUILD_DIR)/$(SRC_DIR)/%.o: $(SRC_DIR)/%.c | $$(@D)/.
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(BUILD_DIR)/%: $(TEST_DIR)/%.c $(OBJS)
	$(CC) -g $(CFLAGS) $(DEPFLAGS) -o $@ $(OBJS) $< $(LDFLAGS)
	
-include $(DEPS)

.PHONY: release debug minim tests unit-tests memcheck examples lib-tests \
		clean clean-all clean-cache install uninstall minim-gc boehm-gc

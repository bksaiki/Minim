#
#	Top-level Makefile
#

BUILD_DIR	:= build
BOOT_DIR	:= src/boot
SRC_DIR 	:= src
GC_DIR		:= src/gc
TEST_DIR	:= tests
INSTALL_DIR := /usr/bin

ENTRY		:= src/minim.c
EXE         := minim

SRCS 		:= $(shell find $(SRC_DIR) -name "*.c" ! -path "src/boot/*" ! -path "src/gc/*" ! -wholename $(ENTRY))
OBJS 		:= $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS 		:= $(OBJS:.o=.d)

TESTS 		:= $(shell find $(TEST_DIR) -name "*.c")
TEST_EXES   := $(TESTS:$(TEST_DIR)/%.c=$(BUILD_DIR)/%)

DEPFLAGS 	:= -MMD -MP
CFLAGS 		:= -Wall -std=c11
LDFLAGS 	:= -lm -lgmp -L$(GC_DIR) -lgc

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

debug:
	$(MAKE) CFLAGS="$(DEBUG_FLAGS) $(CFLAGS)" minim

profile:
	$(MAKE) CFLAGS="$(PROFILE_FLAGS) $(CFLAGS)" minim

coverage:
	$(MAKE) CFLAGS="$(COVERAGE_FLAGS) $(CFLAGS)" minim

release:
	$(MAKE) CFLAGS="$(RELEASE_FLAGS) $(CFLAGS)" minim

minim: boot $(BUILD_DIR)/config.h $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(ENTRY) $(LDFLAGS) -o $(EXE)

install:
	$(CP) $(EXE) $(INSTALL_DIR)/$(EXE)

boot: boehm-gc
	$(MAKE) -C src/boot

boehm-gc: src/gc/boehm-gc/Makefile
	$(MAKE) -C src/gc/boehm-gc
	$(MKDIR_P) src/gc/build
	$(ECHO) "" > src/gc/build/config.h
	cp src/gc/boehm-gc/.libs/libgc.a src/gc/libgc.a

src/gc/boehm-gc/Makefile:
	cd src/gc/boehm-gc && ./autogen.sh && ./configure --enable-static=yes --enable-shared=no

minim-gc:
	$(MAKE) CFLAGS="$(CFLAGS)" -C src/gc/minim-gc
	$(MKDIR_P) src/gc/build
	$(ECHO) "#define USE_MINIM_GC 1" > src/gc/build/config.h
	cp src/gc/minim-gc/libminimgc.a src/gc/libgc.a

test: minim
	$(MAKE) boot-tests
	$(MAKE) unit-tests
	$(MAKE) lib-tests

boot-tests:
	$(MAKE) -C src/boot test

unit-tests: $(TEST_EXES)
	$(SH) $(TEST_DIR)/test.sh $(TEST_EXES)

memcheck: $(TEST_EXES)
	$(SH) $(TEST_DIR)/memcheck.sh $(TEST_EXES)

examples:
	$(SH) $(TEST_DIR)/examples.sh

lib-tests:
	$(SH) $(TEST_DIR)/lib.sh

clean:
	$(MAKE) -C src/boot clean
	$(RM) src/gc/*.a
	$(RM) $(OBJS) $(EXE)

clean-gc:
	$(MAKE) -C src/gc/boehm-gc clean
	$(MAKE) -C src/gc/minim-gc clean

clean-all: clean-gc clean clean-cache
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
	$(CC) -g $(CFLAGS) -DUSE_MINIM_GC $(DEPFLAGS) -o $@ $(OBJS) $< $(LDFLAGS)
	
-include $(DEPS)

.PHONY: release debug minim tests unit-tests memcheck examples lib-tests \
		clean clean-all clean-cache install uninstall minim-gc boehm-gc

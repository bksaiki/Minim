BUILD_DIR	:= build
SRC_DIR 	:= src
TEST_DIR	:= tests
INSTALL_DIR := /usr/bin

ENTRY		:= src/minim.c
EXE         := minim

SRCS 		:= $(shell find $(SRC_DIR) -name *.c ! -wholename $(ENTRY))
OBJS 		:= $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS 		:= $(OBJS:.o=.d)

TESTS 		:= $(shell find $(TEST_DIR) -name *.c)
TEST_EXES   := $(TESTS:$(TEST_DIR)/%.c=$(BUILD_DIR)/%)

DEPFLAGS 	:= -MMD -MP
CFLAGS 		:= -Wall -std=c11
LDFLAGS 	:= -lm -lgmp

CP := cp
ECHO := echo
MKDIR_P	:= mkdir -p
RM := rm -rf
SH := bash

.PRECIOUS: $(BUILD_DIR)/. $(BUILD_DIR)%/.
.SECONDEXPANSION: $(BUILD_DIR)/%.o

# Top level rules

debug:
	$(MAKE) CFLAGS="-g $(CFLAGS)" minim

profile:
	$(MAKE) CFLAGS="-O3 -march=native -pg $(CFLAGS)" minim

release:
	$(MAKE) CFLAGS="-O3 -march=native $(CFLAGS)" minim

install:
	$(CP) $(EXE) $(INSTALL_DIR)/$(EXE)

minim: $(BUILD_DIR)/config.h $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(ENTRY) $(LDFLAGS) -o $(EXE)

tests: minim unit-tests lib-tests;

unit-tests: $(TEST_EXES)
	$(SH) $(TEST_DIR)/test.sh $(TEST_EXES)

memcheck: $(TEST_EXES)
	$(SH) $(TEST_DIR)/memcheck.sh $(TEST_EXES)

examples:
	$(SH) $(TEST_DIR)/examples.sh

lib-tests:
	$(SH) $(TEST_DIR)/lib.sh

clean:
	$(RM) $(OBJS) $(EXE)

clean-all:
	$(RM) $(BUILD_DIR) tmp $(EXE)

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
		clean clean-all

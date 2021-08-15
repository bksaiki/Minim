BUILD_DIR	= build
SRC_DIR 	= src
TEST_DIR	= tests

ENTRY		= src/minim.c
EXE			= minim

SRCS 		= $(shell find $(SRC_DIR) -name *.c ! -wholename $(ENTRY))
OBJS 		= $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS 		= $(OBJS:.o=.d)

TESTS 		:= $(shell find $(TEST_DIR) -name *.c)
TEST_EXES 	:= $(TESTS:$(TEST_DIR)/%.c=$(BUILD_DIR)/%)

DEPFLAGS 	= -MMD -MP
CFLAGS 		= -Wall -std=c11
LDFLAGS 	= -lm -lgmp

.PRECIOUS: $(BUILD_DIR)/. $(BUILD_DIR)%/.
.SECONDEXPANSION: $(BUILD_DIR)/%.o

# Top level rules

debug:
	$(MAKE) CFLAGS="-g $(CFLAGS)" minim

profile:
	$(MAKE) CFLAGS="-O3 -march=native -pg $(CFLAGS)" minim

release:
	$(MAKE) CFLAGS="-O3 -march=native $(CFLAGS)" minim

minim: $(BUILD_DIR)/config.h $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(ENTRY) $(LDFLAGS) -o $(EXE)

tests: minim unit-tests lib-tests;

unit-tests: $(TEST_EXES)
	sh $(TEST_DIR)/test.sh $(TEST_EXES)

memcheck: $(TEST_EXES)
	sh $(TEST_DIR)/memcheck.sh $(TEST_EXES)

examples:
	sh $(TEST_DIR)/examples.sh

lib-tests:
	sh $(TEST_DIR)/lib.sh

clean:
	$(RM) $(OBJS) $(EXE)

clean-all:
	rm -r -f $(BUILD_DIR) tmp $(EXE)

### Specific rules

$(BUILD_DIR)/config.h:
	mkdir -p $(BUILD_DIR)
	echo "#define MINIM_LIB_PATH \"$(shell pwd)/src/\"" > build/config.h

### General rules

$(BUILD_DIR)/.:
	mkdir -p $@

$(BUILD_DIR)%/.:
	mkdir -p $@

$(BUILD_DIR)/$(SRC_DIR)/%.o: $(SRC_DIR)/%.c | $$(@D)/.
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(BUILD_DIR)/%: $(TEST_DIR)/%.c $(OBJS)
	$(CC) -g $(CFLAGS) $(DEPFLAGS) -o $@ $(OBJS) $< $(LDFLAGS)
	
-include $(DEPS)
.PHONY: release debug minim tests unit-tests memcheck examples lib-tests \
		clean clean-all

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
CFLAGS 		= -g -Wall -std=c11
LDFLAGS 	= -lm -lgmp

.PRECIOUS: $(BUILD_DIR)/. $(BUILD_DIR)%/.
.SECONDEXPANSION: $(BUILD_DIR)/%.o

# Specific rules

main: $(EXE);

build: $(OBJS);

tests: $(TEST_EXES)
	$(TEST_DIR)/test.sh $(TEST_EXES)

memcheck: $(TEST_EXES)
	$(TEST_DIR)/memcheck.sh $(TEST_EXES)

clean:
	$(RM) $(OBJS) $(EXE)

clean-deps:
	$(RM) -r $(DEPS)

clean-all:
	rm -r -f $(BUILD_DIR) tmp $(EXE)

### Specific tests


### General rules

$(BUILD_DIR)/.:
	mkdir -p $@

$(BUILD_DIR)%/.:
	mkdir -p $@

$(BUILD_DIR)/$(SRC_DIR)/%.o: $(SRC_DIR)/%.c | $$(@D)/.
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(BUILD_DIR)/%: $(TEST_DIR)/%.c $(OBJS)
	$(CC) $(CFLAGS) $(DEPFLAGS) -o $@ $(OBJS) $< $(LDFLAGS)

$(EXE): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(ENTRY) $(LDFLAGS) -o $(EXE)
	
-include $(DEPS)
.PHONY: main build clean clean-deps clean-all

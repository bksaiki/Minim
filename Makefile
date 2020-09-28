BUILD_DIR	= build
SRC_DIR 	= src

ENTRY		= 
EXE			= minim

SRCS 		= $(shell find $(SRC_DIR) -name *.c)
OBJS 		= $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS 		= $(OBJS:.o=.d)

DEPFLAGS 	= -MMD -MP
CFLAGS 		= -g -Wall -std=c11
LDFLAGS 	= -lgmp

.PRECIOUS: $(BUILD_DIR)/. $(BUILD_DIR)%/.
.SECONDEXPANSION: $(BUILD_DIR)/%.o

# Specific rules

main: $(EXE);

build: $(OBJS);

clean:
	$(RM) $(OBJS) $(EXE)

clean-deps:
	$(RM) -r $(DEPS)

clean-all:
	rm -r -f $(BUILD_DIR) tmp $(EXE)

### Specific tests

# not tracked
test-sandbox: build/test-sandbox
	$(TEST_DIR)/test.sh build/test-sandbox

### General rules

$(BUILD_DIR)/.:
	mkdir -p $@

$(BUILD_DIR)%/.:
	mkdir -p $@

$(BUILD_DIR)/$(SRC_DIR)/%.o: $(SRC_DIR)/%.c | $$(@D)/.
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $< $(LDFLAGS)

$(EXE): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(EXE)
	
-include $(DEPS)
.PHONY: main build clean clean-deps clean-all

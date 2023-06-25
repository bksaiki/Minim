#
#	Top-level Makefile
#

SRC_DIR = src
BOOT_DIR = src/boot
CORE_DIR = src/core
LIB_DIR = src/library
GC_DIR = src/gc
BUILD_DIR = build
TEST_DIR = test

CP = cp
ECHO = echo
MKDIR_P	= mkdir -p
RM = rm -rf
SH = bash
FIND = find

# Top level rules

all: boot

core: gc
	$(MAKE) -C $(CORE_DIR)

boot: core
	$(MKDIR_P) $(BUILD_DIR)/boot
	$(MAKE) -C $(BOOT_DIR)
	$(CP) $(BOOT_DIR)/minim $(BUILD_DIR)/boot/

boot-file:
	$(BOOT_DIR)/minim $(LIB_DIR)/gen-boot.min \
					  $(LIB_DIR) \
					  $(BOOT_DIR)/s/boot.min

gc:
	$(MAKE) -C $(GC_DIR)

boehm-gc:
	$(MAKE) -C $(GC_DIR) boehm-gc

minim-gc:
	$(MAKE) -C $(GC_DIR) minim-gc

test: boot-tests compile-tests

boot-tests:
	$(MAKE) -C $(BOOT_DIR) test
	$(MAKE) -C $(TEST_DIR) boot

compile-tests: boot
	$(MAKE) -C $(TEST_DIR) compile

clean:
	$(MAKE) -C $(CORE_DIR) clean
	$(MAKE) -C $(BOOT_DIR) clean
	$(RM) $(BUILD_DIR)

clean-all: clean
	$(MAKE) -C $(GC_DIR) clean

.PHONY: all boot boot-file gc boehm-gc minim-gc \
	    test boot-tests clean clean-all

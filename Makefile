#
#	Top-level Makefile
#

SRC_DIR = src
CORE_DIR = src/core
BOOT_DIR = src/boot
GC_DIR = src/gc
BUILD_DIR = build

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

gc:
	$(MAKE) -C $(GC_DIR)

boehm-gc:
	$(MAKE) -C $(GC_DIR) boehm-gc

minim-gc:
	$(MAKE) -C $(GC_DIR) minim-gc

test:
	$(MAKE) boot-tests

boot-tests:
	$(MAKE) -C $(BOOT_DIR) test

clean:
	$(MAKE) -C $(CORE_DIR) clean
	$(MAKE) -C $(BOOT_DIR) clean
	$(RM) $(BUILD_DIR)

clean-all: clean
	$(MAKE) -C $(GC_DIR) clean

.PHONY: all boot gc boehm-gc minim-gc test boot-tests clean clean-all

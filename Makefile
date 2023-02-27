#
#	Top-level Makefile
#

SRC_DIR = src
CORE_DIR = src/core
BOOT_DIR = src/boot
GC_DIR = src/gc

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
	$(MAKE) -C $(BOOT_DIR)

gc:
	$(MAKE) -C $(GC_DIR)

boehm-gc:
	$(MAKE) -C $(GC_DIR) boehm-gc

minim-gc:
	$(MAKE) -C $(GC_DIR) minim-gc

test:
	$(MAKE) boot-tests

clean:
	$(MAKE) -C $(CORE_DIR) clean
	$(MAKE) -C $(BOOT_DIR) clean

clean-all: clean
	$(MAKE) -C $(GC_DIR) clean

.PHONY: all boot gc boehm-gc minim-gc test clean clean-all

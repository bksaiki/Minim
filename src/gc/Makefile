#
#	Makefile for the garbage collectors
#

all: minimgc boehmgc

minimgc:
	$(MAKE) -C minimgc/
	cp minimgc/libminimgc.a .

boehmgc:
	cp ../../boehm-gc/.libs/libgc.a .

clean:
	$(MAKE) -C minimgc/ clean

.PHONY: all clean minimgc

#
#	Makefile for the garbage collectors
#

all: minimgc

minimgc:
	$(MAKE) -C minimgc/
	cp minimgc/libminimgc.a .

clean:
	$(MAKE) -C minimgc/ clean

.PHONY: all clean minimgc

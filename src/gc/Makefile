#
#	Builds either Boehm's GC or Minim's GC
#

GC_LIB = libgc.a

RM := rm -rf
ECHO := echo
MKDIR_P	:= mkdir -p

default: boehm-gc

boehm-gc: boehm-gc/Makefile
	$(MAKE) -C boehm-gc
	cp boehm-gc/.libs/libgc.a $(GC_LIB)
	$(MKDIR_P) build
	$(ECHO) "// #define USE_MINIM_GC 1" > build/config.h

boehm-gc/Makefile:
	cd boehm-gc && ./autogen.sh && ./configure --enable-static=yes --enable-shared=no

minim-gc:
	$(MAKE) CFLAGS="$(CFLAGS)" -C minim-gc
	cp minim-gc/libminimgc.a $(GC_LIB)
	$(MKDIR_P) build
	$(ECHO) "#define USE_MINIM_GC 1" > build/config.h

clean:
	($(MAKE) -C minim-gc clean)
	($(MAKE) -C boehm-gc clean)
	$(RM) build/ boehm-gc/Makefile src/gc/*.a

.PHONY: boehm-gc minim-gc clean

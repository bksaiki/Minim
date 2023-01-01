#
#	Builds either Boehm's GC or Minim's GC
#

RM := rm -rf
ECHO := echo
MKDIR_P	:= mkdir -p

default: boehm-gc

boehm-gc: boehm-gc/Makefile
	$(MAKE) -C boehm-gc
	$(MKDIR_P) build
	$(ECHO) "" > build/config.h
	cp boehm-gc/.libs/libgc.a libgc.a

boehm-gc/Makefile:
	cd boehm-gc && ./autogen.sh && ./configure --enable-static=yes --enable-shared=no

minim-gc:
	$(MAKE) CFLAGS="$(CFLAGS)" -C minim-gc
	$(MKDIR_P) build
	$(ECHO) "#define USE_MINIM_GC 1" > build/config.h
	cp minim-gc/libminimgc.a libgc.a

clean:
	($(MAKE) -C minim-gc clean)
	($(MAKE) -C boehm-gc clean)
	$(RM) build/ boehm-gc/Makefile src/gc/*.a

.PHONY: boehm-gc minim-gc clean

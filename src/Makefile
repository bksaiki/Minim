config: build/ build/config.h;

build/:
	mkdir -p build

build/config.h:
	echo "#define MINIM_LIB_PATH \"$(shell pwd)/\"" > build/config.h

clean:
	rm -r build

# Minim

Minim is a Scheme implementation.

# Building

Requires CMake 3.21+ and a C17 compiler.

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
./build/dev/minim
```

The `dev` preset is a debug build with sanitizers; `stress` also collects
garbage on every allocation.  For an optimized build:

```sh
cmake --preset release
cmake --build --preset release
./build/release/minim
```

`-DMINIM_BUILD_TESTS=OFF` skips building the tests.

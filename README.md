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

The `stress` preset also collects garbage on every allocation.

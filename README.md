# Minim

Minim is a Scheme implementation.

## Building

Requires CMake 3.21+ and a C17 compiler.

```sh
cmake --preset release
cmake --build --preset release
./build/release/minim
```

## Development

The `dev` preset is a debug build with sanitizers and tests; `stress` also
collects garbage on every allocation.  Tests are off otherwise
(`-DMINIM_BUILD_TESTS=ON` enables them).

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
./build/dev/minim
```

Design notes are in [`docs/design/`](docs/design/README.md), and planned work
in [`docs/roadmap.md`](docs/roadmap.md).

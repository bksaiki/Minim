# Minim

Minim is a Scheme implementation.

## Building

Requires CMake 3.21+ and a C17 compiler.

```sh
cmake --preset release
cmake --build --preset release
./build/release/minim             # a REPL
./build/release/minim file.scm    # runs a file
```

Running a file prints only what it `display`s or `write`s.  An error stops it
with exit status 1.

## Development

The `dev` preset is a debug build with sanitizers and tests; `stress` also
collects garbage on every allocation.  Tests are off otherwise
(`-DMINIM_BUILD_TESTS=ON` enables them).

In any build, the environment variables `MINIM_GC_NURSERY` (bytes allocated
between collections) and `MINIM_GC_RADIX` (every *n*-th collection is major)
change when the collector runs.  `MINIM_GC_NURSERY=1` collects at every
allocation, which exposes missing GC roots under the sanitizers.  See
[`docs/design/environment.md`](docs/design/environment.md).

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
./build/dev/minim
```

Design notes are in [`docs/design/`](docs/design/README.md), and planned work
in [`docs/roadmap.md`](docs/roadmap.md).

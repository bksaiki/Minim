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

The `dev` preset is a debug build with sanitizers and tests.  Tests are off
otherwise (`-DMINIM_BUILD_TESTS=ON` enables them).

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev       # the tests
ctest --preset stress    # each test again, collecting garbage at every allocation
./build/dev/minim
```

The stress tests expose missing GC roots under the sanitizers (see
[`docs/design/testing.md`](docs/design/testing.md)).  Environment variables do
the same for any program (see
[`docs/design/environment.md`](docs/design/environment.md)):

```sh
MINIM_GC_NURSERY_BYTES=1 MINIM_GC_RADIX=1 ./build/dev/minim file.scm
```

The documentation is indexed in [`docs/README.md`](docs/README.md): design
notes, roadmaps, and plans in progress.

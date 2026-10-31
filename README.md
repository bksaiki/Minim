# Minim
A small Scheme-like language inspired by Racket. Currently supported on Linux. Not tested on macOS or Windows.

The `main` branch contains the newest features, but there is no guarantee on stability.
For the most recent stable release, checkout the `stable` branch.

#### Prerequisites
 - `gcc`, `make` (not tested with `clang`)
 - `autoconf`, `libtool` (if building with Boehm's garbage collector)
 - GNU MP library (GMP)

#### Install
1. Clone this repository (clone recusive if building Boehm's garbage collector)
2. Run either `make boehm-gc` or `make minim-gc`
3. Run `make release` in this directory.

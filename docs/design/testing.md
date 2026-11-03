# Testing

Tests are registered in `tests/CMakeLists.txt` and need the `dev` build
(sanitizers on).  `ctest --preset dev` runs them; `ctest --preset stress`
runs each again under GC stress.

| Kind | Source | Checks |
|---|---|---|
| unit | `tests/test_<name>.c` | the C API, with `CHECK` and `CHECK_EQ` (`tests/test.h`) |
| REPL | `tests/repl/<name>.scm` | piped into `minim`: stdout against `<name>.out`, and stderr against `<name>.err` if it exists |
| script | `tests/scripts/<name>.scm` | run as `minim <file>`: the exit status too |
| conformance | a section of `tests/r7rs/r7rs-tests.scm` | run by `tests/r7rs/run.scm`: its report against `tests/r7rs/<section>.out` |

`tests/run_minim.cmake` runs the REPL and script tests.  The `boot` test
(`tests/check_boot.cmake`) fails if `scheme/prelude.boot` is out of date
(`prelude.md`).

Each behavior is tested once, where it fits:

- A unit test covers one module of `src/`: `test_object` its constructors,
  `test_tag` value tagging, the rest the module they name.  `test_eval`
  covers the machine: frames, tail calls, deep recursion, long calls, and the
  stacks after errors.  `tests/test.h` has the shared helpers: evaluating
  source (`evals_to`, `fails_with`), checking the last error, building a
  record type, and counting type predicates.
- A REPL test covers a topic of the language, named for it: `numbers`,
  `lists`, `core` (core forms, printing, the REPL's recovery from errors),
  `exceptions`, `guard`, `top-level`, `libraries`, and so on.  Internals with
  output worth reading keep their own files: `boot`, `expansion`, `aliases`,
  and the record primitives at the end of `records`.  A file that uses
  internals starts with `(import (minim system))`.

## Conformance

R7RS has no official test suite; the de facto one is Chibi Scheme's, by the
chair of the R7RS-small working group, vendored unchanged as
`tests/r7rs/r7rs-tests.scm` with its license, `COPYING`.  Each of its 20
sections is a test, `r7rs_<section>` (`r7rs_6.4`, `r7rs_read-syntax`),
labelled `r7rs`: `ctest -L r7rs` runs only them, `ctest -LE r7rs` the rest.

`minim run.scm "6.4 Lists"` runs a section with a shim for the suite's
`(chibi test)` forms.  It splits the section into its top-level forms, those
starting at column 0, unless the form before would end inside a list,
vector, or block comment, and reads and evaluates each under `guard`, so syntax
or procedures Minim lacks fail only their form.  It reports each failing
test (`FAIL`), other error (`ERROR`, `READ-ERROR`), and skipped form
(`SKIP`, for forms that would hang Minim) with the line it starts at, then
the counts.

The expected reports hold today's failures, so a test fails only when a
report changes: a fix as much as a regression.  After a deliberate change,
`cmake --build --preset dev --target r7rs-golden` rewrites them, to be
committed with it.  The sections have no stress copies: the suite is slow
under stress, and the other tests stress the same code.

## GC stress

Every test has a copy, `<name>_stress`, labelled `stress`, run with
`MINIM_GC_NURSERY_BYTES=1`: a collection at every allocation, startup
included, on the default schedule (`gc.md`).  An unrooted object in
generation g is caught only at a site the tests run about 4^g times;
`MINIM_GC_RADIX=1`, by hand, catches it at every site, but takes minutes.
Script tests that set their own environment, or are marked `NO_STRESS`, have
none.

- `startup` checks that the settings apply from the start of `minim_init`,
  so the stress copies stress startup too.
- `stale`, built only with sanitizers, checks that AddressSanitizer reports
  a stale pointer into a collected chunk.
- Slow tests do less work when `gc_stressed()`; tests that rely on when
  collections happen set `use_default_gc_config()` (both in `tests/test.h`).

## Benchmarks

`bench/run.sh A B [runs]` runs each `bench/*.scm` with `A/minim` and
`B/minim` alternately and prints each one's median and minimum seconds;
`empty.scm` measures startup.  Build both with
`-DCMAKE_C_FLAGS="-falign-functions=64 -falign-loops=64 -falign-jumps=64"`:
otherwise code placement alone moves results by several percent.

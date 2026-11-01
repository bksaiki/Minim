# Testing

Tests are registered in `tests/CMakeLists.txt` and need the `dev` build
(sanitizers on).  `ctest --preset dev` runs them; `ctest --preset stress`
runs each again under GC stress.

| Kind | Source | Checks |
|---|---|---|
| unit | `tests/test_<name>.c` | the C API, with `CHECK` and `CHECK_EQ` (`tests/test.h`) |
| REPL | `tests/repl/<name>.scm` | piped into `minim`: stdout against `<name>.out`, and stderr against `<name>.err` if it exists |
| script | `tests/scripts/<name>.scm` | run as `minim <file>`: the exit status too |

`tests/run_minim.cmake` runs the REPL and script tests.  The `boot` test
(`tests/check_boot.cmake`) fails if `src/prelude.boot` is out of date
(`prelude.md`).

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

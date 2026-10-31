# Testing

Tests are registered in `tests/CMakeLists.txt` and need the `dev` build
(sanitizers on).  `ctest --preset dev` runs them; `ctest --preset stress`
runs each again under GC stress.

| Kind | Source | Checks |
|---|---|---|
| unit | `tests/test_<name>.c` | the C API, with `CHECK` and `CHECK_EQ` (`tests/test.h`) |
| REPL | `tests/repl/<name>.scm` | piped into `minim`: stdout against `<name>.out`, and stderr against `<name>.err` if it exists |
| script | `tests/scripts/<name>.scm` | run as `minim <file>`: the exit status too |

`tests/run_minim.cmake` runs the REPL and script tests.

## GC stress

Every test has a copy, `<name>_stress`, labelled `stress`, run with
`MINIM_GC_NURSERY_BYTES=1 MINIM_GC_RADIX=2`: after startup, a collection at
every allocation, alternately minor and major (`gc.md`).  Script tests that
set their own environment, or are marked `NO_STRESS`, have none.

- `startup_stress` stresses startup alone, with `MINIM_GC_STARTUP=1` and
  radix 1.
- `stale`, built only with sanitizers, checks that AddressSanitizer reports
  a stale pointer into a collected chunk.
- Slow tests do less work when `gc_stressed()`; tests that rely on when
  collections happen set `use_default_gc_config()` (both in `tests/test.h`).

# Time: `(scheme time)`

Implementation plan.  The design is settled; what follows is the phase
breakdown, one phase per commit.

## Working policy

- **Pause after each phase for review.** Do not begin the next phase until the
  current one has been looked at.
- **Do not commit.** The author of the change leaves the working tree dirty;
  commits are made by the repository owner.
- **Run only the tests relevant to the phase.** The full suite runs once, at
  the end, after the last phase.
- **Comments stay succinct**, and notes about *process* (what was tried, what
  a phase decided, why an ordering was chosen) belong in this document, not in
  source comments.
- **Design docs are synced after the last phase**, not during the phases, so
  they never describe an intermediate state.  Each phase records its decisions
  in this document instead.
- **Design docs stay succinct**: only the details a reader needs to understand
  the current implementation, said in as few words as possible.

## Context

`(scheme time)` isn't defined: `current-second`, `current-jiffy`, and
`jiffies-per-second` are unbound.  6.14's report fails 4 tests on them,
which also need `real?`, `inexact?`, and `exact?`.

Depends on `flonums.md`: `current-second` returns an inexact number, and
the tests need the number predicates.

## Design

**`current-second`** is the wall-clock time, a flonum of seconds since the
epoch, from C11's `timespec_get` with `TIME_UTC`: `tv_sec + tv_nsec / 1e9`.

**`current-jiffy`** is a fixnum of nanoseconds since Minim started, from a
monotonic clock, so it never goes backward when the wall clock is set.
`jiffies-per-second` is 1,000,000,000.  Counting from startup keeps jiffies
small; a fixnum holds 36 years of them.

The monotonic clock is a new wrapper in `src/system.c`, `minim_monotonic_ns`:
`clock_gettime(CLOCK_MONOTONIC)` on POSIX, `QueryPerformanceCounter` scaled
by `QueryPerformanceFrequency` on Windows.  C17 has no monotonic clock (C23's
`TIME_MONOTONIC` isn't widely available).  `minim_init` records the start.

The three are C primitives in `src/system.c`; `(scheme time)` in
`scheme/standard.scm` exports them, and `(minim)` includes it.

## Phases

### Phase 1 - The clocks

- `src/system.c`, `src/system.h`: `minim_monotonic_ns`, the start time, and
  the three primitives.
- `scheme/standard.scm`: `(scheme time)`, and its names in `(minim)`.
  `scheme/prelude.boot` regenerated.

One phase: three small procedures over two clocks.

Tests: `tests/repl/time.scm`, a new REPL test, checking properties, since
the values vary: `current-second` is an inexact real after 2025's start;
`current-jiffy` is an exact integer, and doesn't decrease across a loop;
`jiffies-per-second` is an exact positive integer; a short busy loop takes a
positive number of jiffies; `(import (scheme time))` in a library.  The 6.14
report updated.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'time|libraries|r7rs'
```

### After the last phase

Sync the docs: `primitives.md` (System), `r7rs-small.md` (coverage:
`(scheme time)` 3 of 3; the conformance table; milestone 9).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

### UTC or TAI for `current-second`?

R7RS asks for TAI, but allows "Coordinated Universal Time plus a suitable
constant" as the best an implementation may do.  UTC needs no table of leap
seconds and is what the C library gives; TAI is currently UTC plus 37
seconds, a constant that changes with each new leap second.  Provisional:
UTC; reopen if a program needs TAI.

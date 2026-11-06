# Lazy: `(scheme lazy)`

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

`(scheme lazy)` isn't defined: `delay`, `delay-force`, `force`,
`make-promise`, and `promise?` are unbound.  4.2's report fails 13 tests on
them (lines 277 to 329), among them R7RS's stream examples and a reentrant
`force`.

## Design

**After R7RS 7.3's reference implementation**, in Scheme, in a new prelude
file, `scheme/lazy.scm`, after `derived.scm` so it can use
`define-record-type`:

- A promise is a record, `promise`, with one mutable field, a box: a pair
  `(done? . value)`, where `value` is the result if done, else a thunk
  returning a promise.  Promises chained by `delay-force` share one box, so
  forcing one forces all.
- `(delay-force e)` is `(%make-promise #f (lambda () e))`; `(delay e)` is
  `(delay-force (%make-promise #t e))`; both `syntax-rules` macros.
- `force` loops: it calls the thunk, and unless the promise was forced
  meanwhile (reentrancy), takes over the result's box, then forces again, as
  a tail call.  So a chain of `delay-force`s runs in constant space.
- `force` of a non-promise raises `force: not a promise`, as `car` of a
  non-pair does: R7RS leaves it open, and a misplaced `force` is more
  likely a bug than an idiom.
- `(make-promise x)` is `x` if it is a promise, else a forced promise of it.
- `promise?` is the record's predicate.  A promise prints as `#<promise>`.

`(scheme lazy)` in `scheme/standard.scm` exports the five names, and
`(minim)` includes it.  R5RS has `delay` and `force`, so `(scheme r5rs)`
gets them too.

## Phases

### Phase 1 - Promises

- `scheme/lazy.scm`, in `MINIM_SCHEME_SOURCES` after `sequences`: the
  record, the two macros, `force`, `make-promise`.
- `scheme/standard.scm`: `(scheme lazy)`, its names in `(minim)`, `delay`
  and `force` in `(scheme r5rs)`.  `scheme/prelude.boot` regenerated.

One phase: the library is small, and its parts are useless apart.

Tests: `tests/repl/lazy.scm`, a new REPL test: `delay` evaluates once;
`make-promise` of a value and of a promise; `promise?` before and after
forcing; `force` of a non-promise's error; a reentrant
`force` (R7RS's `count` example); a chain of 100,000 `delay-force`s forced
within the stack's limit; an error inside a promise leaves it unforced, so
forcing again retries.  The 4.2 report updated.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'lazy|r7rs|libraries' && ctest --preset stress -R lazy
```

**Done.**  4.2: 45 to 56 passed, its promise tests all passing.  The record
type is `<promise>`, so a promise prints as `#<promise>`.  `force` also
raises `force: not a promise` when a `delay-force`'s expression returns a
non-promise.  The chain test forces 1,000 `delay-force`s, not 100,000: the
stack has no limit to overflow, so length shows nothing more about space,
and 100,000 took 140 s under stress.

### After the last phase

Sync the docs: `prelude.md` (the file, promises), `r7rs-small.md`
(coverage: `(scheme lazy)` 5 of 5, `(scheme r5rs)`; the conformance table;
milestone 9).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

None.  Settled: `force` of a non-promise raises.

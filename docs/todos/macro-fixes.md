# Macro fixes: top-level `begin` definitions, and an ellipsis among literals

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

The 4.3 report fails two tests, both expander bugs (`scheme/expander.scm`):

| Input | Today | R7RS |
|---|---|---|
| a top-level `(ffoo ff)` expanding to `(begin (define (ff x) (gg x)) (define (gg x) (* x x)))`, then `(ff 10)` | `unbound variable: gg` | 100 |
| `(syntax-rules ... (...) ((_ x) '(x ...)))` applied to 100 | `bad syntax: x` | `(100 ...)` |

**Top-level `begin`.**  `%expand-top` expands a `begin`'s forms one by
one.  `ff`'s body refers to the macro-introduced `gg` before `gg` is
defined, so the alias resolves to the global `gg`; the definition that
follows gives the alias a fresh name of its own (`%top-define!`), as
hygiene requires, and the reference misses it.  In a body the same macro
works, since `%expand-body` binds every definition before expanding any
right-hand side; `%expand-library-body` does the same for libraries.

**An ellipsis among literals.**  R7RS 4.3.2: a subpattern followed by the
ellipsis matches zero or more elements "unless `<ellipsis>` appears in the
`<literal>`s, in which case it is matched as a literal."  `%syntax-rules`'s
`ellipsis?` doesn't check the literals, so `x ...` in the pattern is a
sequence and the template's `...` an ellipsis over a depth-0 variable.

## Design

**Top-level `begin` expands as a library body does**: a first pass expands
each form's head, binding each definition's names (`%top-define!`) and each
`define-syntax`'s macro in order, then the right-hand sides and expressions
are expanded, and run in order.  `%expand-library-body`'s scan becomes
shared, taking whether `import` and `define-library` are allowed: at top
level they are, run as the first pass meets them, as today; in a library
body they stay `bad syntax`.

**`ellipsis?` is false for a literal**, in patterns and templates alike,
so a literal `...` matches and is inserted as itself.

## Phases

### Phase 1 - Definitions in a top-level `begin`

- `scheme/expander.scm`, `scheme/libraries.scm`: the shared scan; `%expand-top`'s
  `begin` through it.  `scheme/prelude.boot` regenerated.

Tests: `tests/repl/top-level.scm`: the `ffoo` macro at top level; a `begin`
whose first definition calls one after it; a `begin` with a `define-syntax`
used by a later form in it; `import` in a top-level `begin` still working.
The 4.3 report updated.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'top-level|syntax-rules|libraries|boot|r7rs'
```

### Phase 2 - An ellipsis among literals

- `scheme/expander.scm`: `ellipsis?`.  `scheme/prelude.boot` regenerated.

Tests: `tests/repl/syntax-rules.scm`: `elli-lit-1` as R7RS's test has it;
`...` as a literal in the default ellipsis's place, `(syntax-rules (...)
...)`; a custom ellipsis not in the literals still an ellipsis.  The 4.3
report updated.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'syntax-rules|boot|r7rs'
```

### After the last phase

Sync the docs: `macros.md` (top-level `begin`; the ellipsis rule),
`r7rs-small.md` (the conformance table).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

None.  Settled: a top-level `begin` binds before it expands, as bodies and
libraries do, as Chibi's R7RS tests expect.

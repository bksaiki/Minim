# Core forms: expand into the -values binding forms

Implementation plan.  The design is settled; what follows is the phase
breakdown, one phase per commit.

This is the second of two plans.  The first added multiple values and the
core forms `define-values`, `let-values`, and `letrec-values`.  This one
makes the expander produce them, as Racket's fully expanded programs do:
bodies become `letrec-values`, and `let` becomes `let-values`.

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

The expander (`src/prelude.scm`, `macros.md`) leaves internal definitions as
`define` forms, which the evaluator binds in the procedure's frame, and
writes every local binding construct as a `lambda` call:

| Source | Expansion today |
|---|---|
| `(lambda (a) (define x 1) (+ a x))` | unchanged (names renamed) |
| `(let ((y 1)) y)` | `((lambda (y) y) 1)` |
| `(let () b)` | `((lambda () b))` |
| `(let* ((a 1) (b a)) b)` | `((lambda (a) ((lambda (b) ((lambda () b))) a)) 1)` |
| `(letrec ((f e)) (f))` | `((lambda () (define f e) ((lambda () (f)))))` |
| `(let-syntax (...) (m))` | `((lambda () ...))` |
| `(lambda () (define x 1))` | accepted; returns void |

So:

- **Every `let` allocates a closure and calls it.**  `let*` nests one per
  binding, plus one for its body.
- **A frame's variables aren't known until its `define`s run**, which the
  roadmap's lexical-address pass needs to know up front.
- **A body of only definitions is accepted**, though R7RS 5.4 requires an
  expression after them.

## Design

**Bodies.**  `%expand-body` turns a body with definitions into one
`letrec-values` form, each `(define x e)` a clause `((x) e)` and each
`(define-values formals e)` a clause `(formals e)`, in order.  So it keeps
`letrec*` semantics (Phase 2 of the first plan).  A body without definitions
is unchanged.  A body of only definitions is `bad syntax`.

The definitions now get their own frame, inside the procedure's: one more
environment frame per call of a procedure whose body has definitions.  The
lexical-address pass can merge the two later.

**Derived forms.**

| Form | Expands to |
|---|---|
| `(let ((x e) ...) body ...)` | `(let-values (((x) e) ...) body ...)` |
| named `let` | unchanged in shape: `((letrec ((tag (lambda ...))) tag) e ...)` |
| `(letrec* ((x e) ...) body ...)`, `letrec` | `(letrec-values (((x) e) ...) body ...)` |
| `(let-syntax ...)`, `(letrec-syntax ...)` | `(let-values () body ...)` |

`let*` and `do` are written with `let`, so they follow.

**The evaluator keeps internal `define`.**  The expander's own section of the
prelude runs unexpanded, before `%expand` exists, and uses 72 internal
`define`s.  So the evaluator still binds `define` and `define-values` in a
procedure's frame, but expanded code no longer uses that.  See Open items.

## Phases

### Phase 1 - Expansion tests

**Done.**  As planned; also `letrec*`, `letrec-syntax`, and a nested body.
The test has no stderr, so no `.err` file.

- `tests/repl/expansion.scm` (new): prints `(%expand 'form)` for each row of
  the Context table, plus `do`, `let*-values`, nested bodies, and a body with
  `define-syntax`.  Uninterned names print as their base names, so the output
  is readable.

Separate because it records today's expansions as a regression net; the
later phases change its expected output, and the diff shows exactly what
each phase changed.

```sh
cmake --build --preset dev && ctest --preset dev -R 'expansion'
```

### Phase 2 - Bodies into letrec-values

**Done.**  Details settled during the phase:

- The body scan now collects `(formals . expression)` for both `define` and
  `define-values`; `finish` builds the clauses.
- The new tests went into `tests/repl/expander.scm`, beside the existing
  internal-definition tests, rather than the three files planned: a
  definition using a parameter, and bodies of only definitions in `lambda`,
  `let`, `let-values`, and with `define-syntax`.  `(let () (define z 1))`
  reports `(lambda () (define z 1))`, the form the `let` macro made; Phase 3
  changes it to `let-values`.
- Fixed in passing: the first plan placed `let*-values` between `letrec*` and
  its comment.
- Measured against `main`, release, run alternately three times: the
  allocation programs unchanged (2.7 s, 1.8 s); a loop calling a procedure
  with two internal definitions 2M times unchanged (2.6-2.8 s), so the extra
  frame costs nothing visible; 20 startups 0.61-0.65 s (`main`: 0.68-0.73 s).

- `src/prelude.scm`: `%expand-body`'s `finish` builds the `letrec-values`
  form; a body of only definitions is `bad syntax`.  `letrec*` (and so
  `letrec`) becomes `letrec-values` directly.

Separate because it changes the shape of every procedure with internal
definitions, including the whole library section of the prelude.

Tests:

- `tests/repl/expansion.out` updated;
- `tests/repl/derived.scm`, `local-macros.scm`, `values.scm`: a body of only
  definitions is `bad syntax`, in `lambda`, `let`, and `let-values`; internal
  definitions still see each other and the procedure's parameters, and shadow
  them;
- every REPL test, since the prelude's library changes shape.

```sh
cmake --build --preset dev && ctest --preset dev -R 'repl|eval' && ctest --preset stress -R 'repl|eval'
```

### Phase 3 - let into let-values

- `src/prelude.scm`: `let` (unnamed) becomes `let-values`;
  `%expand-let-syntax` returns `(let-values () body ...)`.

Separate because it changes the most common binding form, and its measurement
should be taken alone.

Tests:

- `tests/repl/expansion.out` updated; every REPL test.
- Measure against `main`, run alternately: the release benchmarks, and a
  `let`-heavy loop.

```sh
cmake --build --preset dev && ctest --preset dev -R 'repl|eval' && ctest --preset stress -R 'repl|eval'
```

### After the last phase

Sync the docs: `macros.md` (bodies, derived forms, `let-syntax`), `eval.md`
(internal `define` serves only unexpanded code), and the roadmap (the core
form change done; the bootstrap note no longer says "next PR", and records
the internal-`define` question below).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

### Should the evaluator drop internal `define` now?

Expanded code no longer needs it, but the expander's source, which runs
unexpanded, uses it 72 times.  Dropping it now means rewriting those as
`letrec-values` by hand: verbose, and churn in code that a bootstrap would
let be written with any macro.  Keeping it leaves one evaluator form that
only the expander's source uses.

Provisionally keep it, and decide with the prelude bootstrap, which changes
how the expander's source runs.  Reopen if the bootstrap is dropped.

### Is one extra frame per call acceptable for bodies with definitions?

The alternative is to keep definitions in the procedure's frame by expanding
`(lambda (a) (define x e) b)` specially, which `letrec-values` can't express.

Provisionally accept the extra frame; the lexical-address pass can merge
frames.  Reopen if the Phase 3 measurements show it matters.

# Core quality of life: void, exit, case-lambda, and comments

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

Strengthening the core before going deeper: features that make Minim's own
Scheme simpler and the REPL nicer to use, rather than ones chosen by
conformance failures.

| Input | Today |
|---|---|
| `(void)` | `unbound variable: void`; the prelude writes `(if #f #f)` (`for-each`, `unless`, `do`) |
| `(exit)` | `unbound variable: exit`; the REPL ends only at the end of its input, a script can't choose its status |
| `(case-lambda (() 0) ((x) x))` | `unbound variable: case-lambda` |
| `(list 1 #;2 3)`, `#\| ... \|#` | `bad # syntax` |

Fourteen procedures in `scheme/ports.scm`, and `make-parameter`, take
optional arguments as a rest list that they take apart by hand: `%port-arg`,
a "too many arguments" message each, and `write-string`'s nested `cond`.

## Design

**`(void)`**, after Chez (not R7RS): void, with no arguments.  In
`(minim)`.  The prelude's three `(if #f #f)` become `(void)`.

**`(exit [obj])` and `(emergency-exit [obj])`** (R7RS 6.14, `(scheme
process-context)`): end the process with status 0 for no argument or a true
non-integer, 1 for `#f`, and `obj` itself for a fixnum.  `exit` first runs
the `after`s of the `dynamic-wind`s in progress, innermost first, with the
prelude's `%rewind!` to the empty list; `emergency-exit` doesn't.  Both end
through `(%exit status)`, which runs `minim_shutdown`, flushing and closing
file ports by their destructors, then C's `exit`.  At the REPL or in a
script or program, alike.

**`case-lambda`** (R7RS 4.2.9, `(scheme case-lambda)`): a macro over
`lambda`, R7RS's definition being in terms of it too.
`(case-lambda (formals body ...) ...)` expands to `(%case-lambda (list
(lambda formals body ...) ...))`.  `%case-lambda` reads each clause's arity
once, from its parameters (`%closure-code`): so many required, and whether
a rest follows.  It returns a procedure taking any arguments, applying the
first clause that accepts their number, and raising `case-lambda: arity
mismatch` if none does.  Then the optional-argument procedures are written
with it.

**Comments** (R7RS 2.2): `#;` comments out the next datum, and `#| ... |#`
a region, nesting.  Both are atmosphere, in the reader, wherever whitespace
may be: `(a #;b)` is `(a)`.

## Phases

### Phase 1 - void

- `src/control.c`: the primitive `void`.  `scheme/standard.scm`: in
  `(minim)`.
- `scheme/lists.scm`, `scheme/derived.scm`: `(void)` for `(if #f #f)` in
  `for-each`, `unless`, and `do`.  `scheme/prelude.boot` regenerated.

Separate because it is a procedure of its own, and the smallest.

Tests (`tests/repl/core.scm`): `(void)` prints nothing, `(list (void))` is
`(#<void>)`, `(void 1)` is an arity error.  `unless`, `do`, and `for-each`
unchanged (`derived`, `lists`).  Measure a million-iteration `do` loop
against before, by hand (no benchmark uses `do` or `unless`): a call
replaces an `if`.

```sh
cmake --build --preset dev && ctest --preset dev -R 'core|derived|lists|boot|r7rs'
```

### Phase 2 - exit and emergency-exit

- `src/system.c`: `%exit`.  `scheme/control.scm`: `exit` and
  `emergency-exit`.  `scheme/standard.scm`: in `(scheme process-context)`.
  `scheme/prelude.boot` regenerated.

Separate because they end the process, a behavior of their own.

Tests (`tests/scripts/`): a script's status after `(exit)`, `(exit #t)`,
`(exit #f)`, `(exit 3)`, and `(exit 'other)`; `exit` inside a
`dynamic-wind` runs its `after`, `emergency-exit` doesn't; output written
to a file port, unclosed, before `exit` reaches the file; `(exit)` as the
REPL's input ends it, with what follows unread.  The conformance reports
updated (`r7rs-golden`): 6.14.

```sh
cmake --build --preset dev && ctest --preset dev -R 'script|r7rs|boot'
```

### Phase 3 - case-lambda

- `scheme/control.scm`: `%case-lambda`.  `scheme/derived.scm`: the
  `case-lambda` macro.  `scheme/standard.scm`: `(scheme case-lambda)`, and
  in `(minim)`.  `scheme/prelude.boot` regenerated.

Separate because it is a missing form; the next phase uses it.

Tests (`tests/repl/derived.scm`): clauses by exact count and with rests,
the first matching clause chosen, no clause matching an error, a clause
body with definitions, `case-lambda` in a library importing
`(scheme case-lambda)`.  The conformance reports updated: 4.2.

```sh
cmake --build --preset dev && ctest --preset dev -R 'derived|libraries|r7rs|boot'
```

### Phase 4 - Optional arguments with case-lambda

- `scheme/ports.scm`: the procedures with optional ports, and
  `write-string`, with `case-lambda`; `%port-arg` gone.
- `scheme/control.scm`: `make-parameter`.  `scheme/prelude.boot`
  regenerated.

Separate because it is a rewrite, with no change in behavior but the arity
errors' wording.

Tests: `tests/repl/ports.scm` and `parameters.scm` unchanged but for "too
many arguments" becoming `case-lambda: arity mismatch`.  Measure a loop of
`read-char` over a long string port against before.

```sh
cmake --build --preset dev && ctest --preset dev -R 'port|parameters|script|r7rs|boot'
```

### Phase 5 - Datum and block comments

- `src/read.c`: `#;` and `#| ... |#`, nesting, as atmosphere.

Separate because it changes the reader alone.

Tests (`tests/test_read.c`): `#;` before a datum, inside a list, before
`)`, and twice in a row; a block comment nested, spanning lines, and
unterminated (`eof in comment`); `#|` inside a string unaffected.  The
conformance reports updated: read syntax.

```sh
cmake --build --preset dev && ctest --preset dev -R 'read|r7rs'
```

### After the last phase

Sync the docs: `primitives.md` (`void`, `%exit`), `prelude.md` (`exit`,
`emergency-exit`, `%case-lambda`, the ports' `case-lambda`), `macros.md`
(`case-lambda` among the derived forms), `repl.md` (comments; `exit` ends
the REPL), and the roadmap (`r7rs-small.md`: coverage of
`(scheme case-lambda)` and `(scheme process-context)`, milestone 9, the
conformance table).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

### case-lambda as a macro, or the evaluator's core form?

Chez compiles every procedure to `case-lambda` (`s/syntax.ss`,
`build-lambda` emits a one-clause one): the arity check and dispatch are the
machine's.  In Minim that would change the closure layout, the expander's
core forms, and the evaluator, for a form few procedures use.  The macro
costs a rest list and an `apply` per call.

Provisionally the macro.  Reopen with `compile.md`'s compilation pass, or if
Phase 4's measurement shows the ports' procedures slower.

### Should `void` take any arguments?

Racket's `void` accepts and ignores any; Chez's takes none.  Provisionally
none, as Chez: `(void x)` is likely a mistake.

### What does `(exit obj)` do for other objects?

R7RS: `#f` is abnormal, anything else translated to "an appropriate exit
value".  Provisionally a fixnum is the status itself, any other true value
0.  The status is then whatever the OS keeps of it (the low 8 bits on
POSIX).

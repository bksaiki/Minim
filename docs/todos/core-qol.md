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
| `(%closure-code (lambda (x) x))` | `((x) x)`: a closure's code is its `lambda`'s `(params . body)` |
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

**`case-lambda`** (R7RS 4.2.9, `(scheme case-lambda)`) is a core form,
after Chez Scheme, where every procedure is a `case-lambda` and `lambda` the
one-clause case (`s/syntax.ss`, `build-lambda`).  Both `lambda` and
`case-lambda` reach the evaluator: the expander expands `case-lambda`'s
clauses as it does `lambda`'s parameters and body, and `(define (f . x)
...)` still means `lambda`.

- **A closure's code is its form:** `(lambda params body ...)` or
  `(case-lambda (params body ...) ...)`, the expanded form itself, shared by
  every closure it makes, so making one allocates nothing more than today,
  and `parameter?`'s comparison of code (`%closure-code`) still holds.
  Unlike Chez's, Minim's closure stays `[code | env]`, an environment
  chain, not flat.
- **Applying a closure** takes a `lambda`'s parameters and body as now, and
  a `case-lambda`'s first clause whose parameters accept the arguments'
  number, or raises `arity mismatch` if none does, as for a `lambda`.  Both
  the stack's calls and `apply`'s long ones (`eval.md`) choose the clause.

Then the optional-argument procedures are written with it.

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

**Done.**

- `void` is a primitive in `src/control.c`, with `procedure?`, in `(minim)`.
  `unless`'s and `do`'s templates call it; their alias resolves in the
  system's environment, so a program's own `void` doesn't change them.
- The boot file settles on the second regeneration: the first expands with
  the old `unless` and `do`, the second with the new ones, in the prelude's
  own uses.
- Measured, release builds, a million-iteration `do` with `unless`, 9 runs
  alternated: median 1.02 s before, 1.03 s after: noise.
- No conformance report changed.
- Ran `ctest -R 'core|derived|lists|boot|r7rs'`.

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

**Done.**

- `%exit` (`src/system.c`) runs `minim_shutdown`, then C's `exit`; `exit`
  and `emergency-exit` (`scheme/control.scm`) compute the status with
  `%exit-status`.  They take their optional argument as a rest list until
  Phase 5 rewrites them with `case-lambda`.
- `tests/scripts/exit.scm` takes the kind of exit as its argument, so one
  script covers every status, inside a `dynamic-wind`: `exit` prints its
  `after`, `emergency-exit` doesn't, and what was displayed before reaches
  stdout.  `tests/repl/exit.scm` checks the REPL stops at `(exit)`.
- No test of a file port flushed by `exit`: `%exit` runs `minim_shutdown`,
  whose flushing of file ports `test_port` already checks.
- No conformance report changed: 6.14 never calls `exit`.
- Ran `ctest -R 'script|exit|r7rs|boot'`, and stress `exit`.

### Phase 3 - case-lambda in the evaluator

- `src/eval.c`: closures' code is their form: a `lambda` keeps its
  `(lambda params body ...)`, not `(params body ...)`; the core form
  `case-lambda`, its clauses checked like `lambda`'s; applying a closure, on
  the stack and through `apply`'s long path, chooses its clause.
- `src/control.c`: `%closure-code` returns the form, unchanged otherwise;
  `parameter?` (`scheme/control.scm`) compares it as before.

Separate because it changes the machine, under the expander, with no new
syntax yet: hand-written core forms test it.

Tests (`tests/test_eval.c`, through `minim_eval_expanded`): clauses by exact
count, with a rest, and `()`; the first that accepts chosen; none accepting
`arity mismatch`; a clause's tail call; a call through `apply` beyond the
stack's limit choosing its clause; malformed clauses `bad syntax`.
`tests/repl/parameters.scm`'s `%closure-code` output updated.  Measure
`bench/run.sh` against `main`: applying a `lambda` now takes its parameters
one `cdr` further.

```sh
cmake --build --preset dev && ctest --preset dev -R 'eval|parameters|expansion' && ctest --preset stress -R 'eval'
```

### Phase 4 - case-lambda in the expander

- `scheme/expander.scm`: `case-lambda` a core form, each clause expanded as
  a `lambda`'s parameters and body.  `scheme/standard.scm`:
  `(scheme case-lambda)`, and in `(minim)`.  `scheme/prelude.boot`
  regenerated.

Separate because it is the syntax, on Phase 3.

Tests (`tests/repl/expansion.scm`, `derived.scm`): a `case-lambda`'s
expansion; clauses by count and with rests; a clause body with definitions
and internal macros; an identifier a macro inserts into a clause stays
hygienic; `case-lambda` in a library importing `(scheme case-lambda)`.  The
conformance reports updated: 4.2.

```sh
cmake --build --preset dev && ctest --preset dev -R 'expansion|derived|libraries|r7rs|boot'
```

### Phase 5 - Optional arguments with case-lambda

- `scheme/ports.scm`: the procedures with optional ports, and
  `write-string`, with `case-lambda`; `%port-arg` gone.
- `scheme/control.scm`: `make-parameter`, `exit`, and `emergency-exit`.
  `scheme/prelude.boot` regenerated.

Separate because it is a rewrite, with no change in behavior but the arity
errors' wording.

Tests: `tests/repl/ports.scm` and `parameters.scm` unchanged but for "too
many arguments" becoming the evaluator's `arity mismatch`.  Measure a loop
of `read-char` over a long string port against before: the rest list and
its parsing are gone.

```sh
cmake --build --preset dev && ctest --preset dev -R 'port|parameters|script|r7rs|boot'
```

### Phase 6 - Datum and block comments

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

Sync the docs: `eval.md` (the core form `case-lambda`; a closure's code is
its form; choosing a clause), `gc.md` (the closure layout), `primitives.md`
(`void`, `%exit`, `%closure-code`), `prelude.md` (`exit`, `emergency-exit`,
the ports' `case-lambda`), `macros.md` (`case-lambda` among the core
forms), `repl.md` (comments; `exit` ends the REPL), `compile.md` (its
compiled form covers `case-lambda`), and the roadmap (`r7rs-small.md`: coverage of
`(scheme case-lambda)` and `(scheme process-context)`, milestone 9, the
conformance table).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

### Flat closures and code objects, as Chez's?

Chez's closure is `[code | free variables]`, and its code a shared object
holding the compiled `case-lambda`, its name (`#<procedure f>`), and an
arity mask.  Minim's stays `[form | env]`: an environment chain keeps every
enclosing variable alive, and closures print without names.  Both are a
compilation pass's work (free-variable analysis, a code object to hold the
name and arity).

Provisionally out of this plan.  Reopen with `compile.md`.

### Should `void` take any arguments?

Racket's `void` accepts and ignores any; Chez's takes none.  Provisionally
none, as Chez: `(void x)` is likely a mistake.

### What does `(exit obj)` do for other objects?

R7RS: `#f` is abnormal, anything else translated to "an appropriate exit
value".  Provisionally a fixnum is the status itself, any other true value
0.  The status is then whatever the OS keeps of it (the low 8 bits on
POSIX).

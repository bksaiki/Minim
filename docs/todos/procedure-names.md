# Procedure names

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

Primitives print with their names, `#<procedure car>`, and their errors
name them (`car: not a pair`).  Closures print as `#<procedure>`: neither
the REPL nor an error says which one.  Since the optional-argument
procedures became `case-lambda`s, this got worse:

| Input | Today | Before `case-lambda` |
|---|---|---|
| `(read-char (current-input-port) 1)` | `arity mismatch: #<procedure>` | `read-char: too many arguments` |
| `(define (f x) x)`, then `f` | `#<procedure>` | `#<procedure>` |
| `(f 1 2)` | `arity mismatch: #<procedure>` | the same |

A closure is `[code | env]` (`gc.md`), its code its `lambda` or
`case-lambda` form, shared by every closure the form makes (`eval.md`).

## Design

After Chez Scheme, where a procedure's name is in its code object, shared by
its closures, and inferred from what binds it: **a name is part of the
form.**  `case-lambda` takes an optional name, a symbol before its clauses,
unambiguous since clauses are lists: `(case-lambda f ((x) body ...))`.  A
named procedure is a `case-lambda`, as every procedure is in Chez; `lambda`
stays for anonymous ones.  Closures keep their two words, and naming costs
nothing per closure.

**The evaluator** skips a `case-lambda`'s name when it checks and chooses
clauses.  `minim_closure_name(c)` (`minim.h`) is the name, or `#f`.

**The printer** prints a named closure as `#<procedure f>`, as a primitive;
so do errors, which print their irritants: `arity mismatch: #<procedure
read-char>`.

**The expander names** a procedure bound to a single identifier where its
init expands to a `lambda` or unnamed `case-lambda`, as Chez infers names:

- a top-level, library, or internal `define`, `(define (f x) ...)` and
  `(define f (lambda ...))` alike;
- a `let-values` or `letrec-values` clause with one formal, so `let`,
  `letrec`, and named `let`'s loop.

`(lambda params body ...)` becomes `(case-lambda f (params body ...))`; an
unnamed `case-lambda` takes the name; a named one keeps its own.  The name
is the identifier's label: a local's fresh name prints as the identifier
does.

## Phases

### Phase 1 - Names in the evaluator and printer

- `src/eval.c`: `case-lambda`'s optional name, skipped by
  `valid_lambda_clauses` and `clause_for`.
- `include/minim.h`, `src/object.c`: `minim_closure_name`.
- `src/print.c`: `#<procedure f>` for a named closure.

Separate because it is the representation, with nothing naming procedures
yet: hand-written core forms test it.

Tests: `tests/test_eval.c` (core forms through `minim_eval_expanded`): a
named `case-lambda` applies like an unnamed one; a name that isn't a symbol,
or a name with no clauses after it, is `bad syntax`.  `tests/test_print.c`:
a named closure prints its name, an unnamed one `#<procedure>`;
`minim_closure_name` of each.

```sh
cmake --build --preset dev && ctest --preset dev -R 'eval|print' && ctest --preset stress -R 'eval|print'
```

### Phase 2 - Names from the expander

- `scheme/expander.scm`: naming a `define`'s, and a single-formal
  `let-values` or `letrec-values` clause's, `lambda` or unnamed
  `case-lambda`, at top level, in libraries (`scheme/libraries.scm`), and in
  bodies.  `scheme/prelude.boot` regenerated: the prelude's procedures are
  named too.

Separate because it is where names come from, on Phase 1.

Tests (`tests/repl/expansion.scm`, `core.scm`): a `define`'s procedure
prints its name; so do a `let`'s, a `letrec`'s, and a named `let`'s loop; an
anonymous `lambda` stays unnamed; `(define g f)` keeps `f`'s name; a
`case-lambda`'s name kept over the binding's; `(read-char p 1)` is `arity
mismatch: #<procedure read-char>`.  Every REPL test's output updated where a
procedure prints.  The conformance reports updated.  Measure `bench/run.sh`
against `main`: named procedures now apply through `case-lambda`'s clause
loop.

```sh
cmake --build --preset dev && ctest --preset dev && ctest --preset stress -R 'expansion|core|boot'
```

### After the last phase

Sync the docs: `eval.md` (`case-lambda`'s name; a closure's name),
`macros.md` (which bindings name their procedures), `repl.md` (how named
closures print), and `gc.md` if the closure layout's note mentions the
form.

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

### Where does the name live?

- **In the form, `case-lambda`'s** (planned): no change to closures or the
  collector, nothing per closure, a name per definition as Chez's.  But
  every named procedure's expansion becomes a `case-lambda`, longer to read.
- **A wrapper core form, `(named f (lambda ...))`**: `lambda` stays as
  written, but one more core form, and one more step to a named closure's
  clauses.
- **A third closure word**: a name per closure, so set at run time.  But
  closures share the pair space, which assumes a pair's size: they'd need a
  space of their own, and each would grow from 16 to 32 bytes.

Provisionally the form.  Reopen with `compile.md`, whose code objects could
hold the name as Chez's do.

### Which bindings name a procedure?

Provisionally `define` and single-formal `let-values` and `letrec-values`
clauses, as Chez infers from `define`, `let`, and `letrec`.  Not `set!`, nor
`define-values`, whose values aren't known to be procedures.  Reopen if
anonymous procedures stay common in error messages.

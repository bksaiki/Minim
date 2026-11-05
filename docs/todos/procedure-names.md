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

A closure is `[code | env]` (`gc.md`): its code its `lambda` or
`case-lambda` form, R7RS's syntax, shared by every closure the form makes
(`eval.md`).  Closures live in the pair space, which holds only two-word
objects.

## Design

**A closure has a name**, its own: `[code | env | name]`, 24 B, rounded to
32, the name a symbol or `#f`.  A name is per closure, not per form, so two
closures of one `lambda` may be named differently.  The core forms stay
R7RS's.

**Closures get a space of their own** in the old generations, beside pairs,
symbols, and typed objects: types differ in size, lifetime, and access
pattern, so mixing them costs locality, and the collector sizes and scans a
space by its kind.  `minim_closure_name(c)`
and `minim_set_closure_name(c, name)` (`minim.h`), the latter through the
write barrier.

**Binding names an unnamed closure.**  When `define`, `define-values`, or a
`let-values` or `letrec-values` clause binds a variable to a closure
without a name, the closure takes the variable's name: all go through
`minim_env_define`.  A named closure keeps its name.  Arguments don't name:
a call's parameters are bound by `minim_env_extend`, so a procedure passed
to `map` stays as it was.

```scheme
(define (make-adder n) (lambda (x) (+ x n)))
(define add1 (make-adder 1))     ; #<procedure add1>
(define add2 (make-adder 2))     ; #<procedure add2>
(define g add1)                  ; #<procedure add1>: already named
(let loop ((i 0)) loop)          ; #<procedure loop>
```

The name is the variable's label: a local's fresh name prints as its
identifier does, and so does a definition's fresh global.

**The printer** prints a named closure as `#<procedure f>`, as a primitive;
so do errors, which print their irritants: `arity mismatch: #<procedure
read-char>`.

## Phases

### Phase 1 - The closure's name

- `include/minim.h`, `src/object.c`: `MINIM_CLOSURE_SIZE` 3 words, a new
  closure's name `#f`; `minim_closure_name`, `minim_set_closure_name`.
- `src/heap.h`, `src/gc.c`: a closure space, `MINIM_KIND_CLOSURE`: copying
  a closure into it, its size, and its three traced fields.
- `src/print.c`: `#<procedure f>` for a named closure, with primitives
  through one helper.

Separate because it changes a heap layout and the collector, with nothing
naming closures yet.

**Done.**  `object_size` switches on the tag; `scan` maps a kind to its tag
by table.  `test_gc`'s `test_closures` covers survival through generations
0–3, both spaces, and the barrier (an old closure given a young name stays
remembered and correct through another round).  No byte counts changed: the
counting tests use only pairs.

Tests: `tests/test_object.c`: a new closure's name is `#f`; setting it.
`tests/test_gc.c`: a closure and its name survive collections, young and
promoted; an old closure's young name (the barrier); closures and pairs in
their own spaces.  `tests/test_print.c`: a named and an unnamed closure.
The tests counting live bytes updated for the larger closures.

```sh
cmake --build --preset dev && ctest --preset dev -R 'object|gc|print|heap' && ctest --preset stress -R 'object|gc|print'
```

### Phase 2 - Binding names closures

- `src/env.c`: `minim_env_define` names an unnamed closure.

Separate because it is the behavior, on Phase 1.

**Done.**  `let` and named `let` reach `minim_env_define` through
`let-values` and `letrec-values`, so the one check covers every binding
form.  Changed outputs: `continuations` (`call/cc`), `libraries` (`bump`),
`ports` (`read-char`, `write`), `records` (`kons`).  The conformance reports
and `prelude.boot` didn't change.  An anonymous closure passed through `map`
stays unnamed.  Benchmarks (aligned release builds, alternating, `main`
first): `empty`, `expand`, `fib`, `globals` unchanged; `let` about 2% slower
by median over 15 runs (0.49 s to 0.50 s), the check on each binding.

Tests (`tests/repl/core.scm`): a `define`'s procedure prints its name, as
do `make-adder`'s two closures under two names; `(define g f)` keeps `f`'s;
a `let`'s, a `letrec`'s, a named `let`'s, and a `define-values`'
procedures; an internal `define`'s; an anonymous `lambda` printed directly
stays unnamed, as does one passed as an argument; `(read-char p 1)` is
`arity mismatch: #<procedure read-char>`.  Every REPL test's output updated
where a procedure prints.  The conformance reports updated.  Measure
`bench/run.sh` against `main`: closures are larger, and bindings check
their values.

```sh
cmake --build --preset dev && ctest --preset dev && ctest --preset stress -R 'core|gc|eval'
```

### After the last phase

Sync the docs: `gc.md` (the closure layout and space), `eval.md` (a
closure's name, and binding naming it), `repl.md` (how named closures
print), `primitives.md` if it mentions closures' printing.

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

### Should a call's parameters name closures too?

Not provisionally: `(map (lambda (x) x) l)` would name the procedure `f`,
`map`'s parameter, which says nothing.  Reopen if anonymous procedures stay
common in error messages.

### Should typed objects get spaces by type?

`MINIM_KIND_TYPED` holds vectors, strings, records, primitives, ports, and
error objects, whose footprints differ: strings hold no pointers and need no
scanning, vectors are mostly pointers.  Chez splits spaces along these lines.
Out of scope here; a follow-up branch on the collector.

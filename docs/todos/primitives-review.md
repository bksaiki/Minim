# Primitives review: move to Scheme what the kernel doesn't need

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

Minim has about 250 primitives, C functions in the tables of `src/*.c`
(`primitives.md`), and puts in the prelude (`scheme/*.scm`, `prelude.md`)
mostly what primitives can't express: higher-order procedures.  So much
that Scheme expresses easily is C: `vector->list`, `string-append`,
`char-ci<?`, `zero?`, `nan?`.
Each costs C code, a table entry, and argument checks, and keeps the kernel
larger than it needs to be.

## Design

**The rule.**  For each primitive, two questions:

1. Does the kernel need it?  Yes if it is one of:
   - a type's predicate: `pair?`, `boolean?`, and the numeric tower's
     (`number?`, `integer?`, `exact?`);
   - a type's primitive operation: reaching its representation (`car`,
     `vector-ref`, `char->integer`), or the type's own basic operations
     (`not` on booleans, `char<?` on chars, `+` and `<` on numbers,
     `length` on lists);
   - a type's basic constructor (`cons`, `list`, `make-vector`), or an
     immediate's (`void`, `eof-object`);
   - what only C can do: the OS, the collector, the evaluator (`apply`,
     `values`), or C's tables and libraries (Unicode, `libm`, `strtod`).

   Operations and constructors built from these (`string-append`,
   `vector->list`, `char-ci<?`, `zero?`) are not needed.
2. Can Scheme express it easily, from the primitives that stay, with the
   same results and errors?

No and yes: it moves to the prelude.  Otherwise it stays.

**The review.**  What moves, by area:

| Area | Moves | Stays, and why |
|---|---|---|
| Pairs and lists | none | `cons`, `list`: constructors; `pair?`, `null?`: predicates; `car`, `cdr`, `set-car!`, `set-cdr!`: representation; `length`: lists' operation; the 28 `c[ad]+r`: in Scheme, keeping their errors (`caddr: not a pair` with the argument) would need a path-walking helper, no simpler than the C |
| Booleans and control | `boolean=?` | `boolean?`: predicate; `not`: booleans' operation; `void`: an immediate's constructor; `procedure?`: predicate; `apply`, `load`, `values`, `call-with-values`, `%call/cc`, `%eval-expanded`: the evaluator; `%closure-code`, the winders and handlers: representation.  New: `procedure-rename` (below), and `identity` in Scheme |
| Equality | none | `eq?`, `eqv?` (flonum bits), `equal?` (the cycle-safe walk, and the expander's hot path), `equal-hash`, `string-hash` (64-bit hashing, past the fixnums) |
| Chars | the five `char-ci` comparisons: `char-foldcase`, then a comparison | `char?`: predicate; `char->integer`, `integer->char`: representation; `char=?` `char<?` `char>?` `char<=?` `char>=?`: chars' operations; the classes, `digit-value`, and case: Unicode tables |
| Strings and symbols | `symbol=?`, `string`, `string-append`, `substring`, `string-copy`, `string-copy!`, `string-fill!`, `string->list`, `list->string`, the five comparisons, the five `-ci` comparisons: built on chars | `string?`, `symbol?`: predicates; `make-string`: constructor; `string-length`, `string-ref`, `string-set!`, `symbol->string`, `string->symbol`, `%getprop`, `%putprop!`: representation and interning; `string-upcase`, `-downcase`, `-foldcase`: Unicode's full mappings |
| Vectors | `vector`, `vector->list`, `list->vector`, `vector-copy`, `vector-copy!`, `vector-fill!`, `vector-append`, `string->vector`, `vector->string` | `vector?`: predicate; `make-vector`: constructor; `vector-length`, `vector-ref`, `vector-set!`: representation |
| Bytevectors | `bytevector`, `bytevector-copy`, `bytevector-copy!`, `bytevector-append` | `bytevector?`: predicate; `make-bytevector`: constructor; `bytevector-length`, `bytevector-u8-ref`, `bytevector-u8-set!`: representation; `utf8->string`, `string->utf8`: a codec, not easily Scheme |
| Numbers | `finite?`, `infinite?`, `nan?`, `zero?`, `positive?`, `negative?`, `odd?`, `even?`, `abs`, `max`, `min`, `exact->inexact`, `inexact->exact` | `number?`, `complex?`, `real?`, `rational?`, `integer?`, `exact-integer?`, `exact?`, `inexact?`, `fixnum?`, `flonum?`: predicates; `+` `-` `*` `/` and the comparisons: numbers' operations; `exact`, `inexact`, `floor`, `ceiling`, `truncate`, `round`, `quotient`, `remainder`: conversion and `libm`; `number->string`, `string->number`: the printer and parser |
| Ports | `close-input-port`, `close-output-port`: `close-port` after a check; `%read-line`, `%read-string`, `%write-string`, `%read-bytevector`, `%read-bytevector!`, `%write-bytevector`: loops over a char or byte | the predicates; `eof-object`: an immediate's constructor; the rest: representation and the OS |
| Errors, records, syntax, system | none | representation, the expander's aliases, the OS, the collector |

About 60 move.  Notes on the harder ones:

- **Errors stay the same.**  A moved procedure checks its arguments and
  raises with `error`, as the primitive did, with the same message and
  irritant.  Optional arguments are `case-lambda` (`prelude.md`); ranges
  share a helper checking them as `range_args` does.
- **Numbers keep their edge cases**: `abs` of `-0.0` is `0.0`; `max` and
  `min` are inexact if any argument is and NaN if any is; `nan?` is `(not
  (= x x))`; `odd?` of a non-integer raises `odd?: not an integer`.
- **`procedure-rename`**, after Racket's: `(procedure-rename proc name)` is
  a new procedure that behaves as `proc` but prints with the symbol `name`.
  A closure is copied with the name replaced; a primitive likewise, keeping
  its function pointer, so the evaluator still recognizes `apply` and the
  rest; a continuation raises `procedure-rename: not a closure or
  primitive`.  It is a primitive in `src/control.c`, exported from
  `(minim)`.  `exact->inexact` and `inexact->exact` become
  `(procedure-rename inexact 'exact->inexact)` and its mirror, so they
  print as themselves.  Their errors still name `inexact` and `exact`,
  since a primitive's message is fixed.
- **The C helpers stay** where C code calls them (`minim_string_append`,
  `minim_list_length`, ...); only the primitive and its table entry go, and
  helpers left unused.
- **Load order.**  The prelude loads definitions in file order, and the
  moved procedures must exist before anything that calls them at load
  time.  A new first file, `scheme/base.scm`, holds `identity`,
  `boolean=?`, `symbol=?`, and the `char-ci` comparisons; strings,
  vectors, and bytevectors go in `sequences.scm`; numbers in `numbers.scm`
  (shared with `math.md`; whichever lands first creates it); ports in
  `ports.scm`.  The `boot` test catches a procedure called before it is
  defined.

## Phases

Each phase moves one row of the review: deletes the primitives and their
now-unused helpers, defines the procedures in the prelude, and regenerates
`scheme/prelude.boot`.  The existing tests are the regression net: they
pass unchanged, error messages included, or the phase records why not.
Each phase measures `bench/run.sh` against `main`, and records any
benchmark that slows past noise (see Open items).

### Phase 1 - `boolean=?`, `symbol=?`, the `char-ci` comparisons, and `identity`

- `src/equal.c`, `src/string.c`, `src/char.c`: `boolean=?`, `symbol=?`,
  and the `char-ci` comparisons out.
- `scheme/base.scm` (new; first in `MINIM_SCHEME_SOURCES`): them, and
  `identity`, public in `(minim)`.

First, as the smallest, and to set up `base.scm`.

Tests: the existing `equality`, `chars`, `symbols` REPL tests; new cases
for each moved procedure's errors where none exist.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'equality|chars|symbols|boot|r7rs'
```

**Done.**  `scheme/base.scm` holds `%compare`, which checks every argument
and then compares adjacent pairs by a key, and `identity`.  The rule
changed during the phase: `not`, `boolean?`, `void`, and the five char
comparisons were moved, measured, and put back as primitives (see Settled
in review).  `prims.h`'s `compare` stays, for the char comparisons.
Building the boot file needs both the C primitives and the Scheme
definitions present once: the old boot file loads before the new
definitions exist, so a primitive removed first leaves it unloadable.
`tests/repl/chars.scm` gained a type error after a false comparison and
an arity error; `core.scm`, `identity`.  Benchmarks against `main`, 11
runs, release: the suite unchanged (fib 0.498 to 0.502 s median, expand
0.417 to 0.412); a million `(char-ci<? #\a #\b)` 0.56-0.63 s to 5.3 s, 9
times slower (and `char<?`, before it went back, 0.54 to 4.8 s).

### Phase 2 - Strings

- `src/string.c`: the string and symbol primitives in the review out.
- `scheme/sequences.scm`: them, and the range helper.

Tests: the existing `strings` REPL test, ranges and their errors included.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'strings|boot|r7rs'
```

### Phase 3 - Vectors and bytevectors

- `src/vector.c`, `src/bytevector.c`: the primitives in the review out.
- `scheme/sequences.scm`: them.

Tests: the existing `vectors` and `bytevectors` REPL tests.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'vectors|bytevectors|boot|r7rs'
```

### Phase 4 - Numbers, and `procedure-rename`

- `src/number.c`: `finite?`, `infinite?`, `nan?`, the sign tests,
  `odd?`, `even?`, `abs`, `max`, `min`, and R5RS's names out.
- `src/control.c`: `procedure-rename`.  `scheme/standard.scm`: it in
  `(minim)`.
- `scheme/numbers.scm`: the moved procedures; R5RS's names through
  `procedure-rename`.

`procedure-rename` lands here, with its first use.

Tests: the existing `numbers` REPL test, which covers each edge case above;
`exact->inexact` printing as itself.  `tests/repl/procedures.scm` (or the
nearest existing test): `procedure-rename` of a closure, a `case-lambda`,
a primitive (still callable, and `apply` renamed still applying), and a
parameter (still `parameter?`); the original keeping its name; a
continuation and a non-procedure raising.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'numbers|boot|r7rs'
```

### Phase 5 - Ports

- `src/port.c`: `close-input-port`, `close-output-port`, and the bulk
  reads and writes out.
- `scheme/ports.scm`: them, over `%read-char`, `%peek-char`, `%write-char`,
  `%read-u8`, `%write-u8`.

Tests: the existing `ports` REPL test and `test_port`.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'ports|port|boot|r7rs'
```

### After the last phase

Sync the docs: `primitives.md` (the rule; each moved procedure's row goes
to `prelude.md`; `procedure-rename`), `prelude.md` (the rule replaces "what
is easiest in Scheme"; `base.scm` in the file table; the moved procedures in
Library), `r7rs-small.md` (`procedure-rename` among Minim's own names, if
listed).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

### Keep hot or bulk operations in C for speed?

The rule doesn't weigh speed, and Minim interprets the prelude: a moved
`zero?` is a closure call that calls `=`, and a moved `string-append` or
`read-line` handles each char through the interpreter, where C copies or
reads in one call.  Chez can move these to Scheme because it compiles
them.  `char<?` in Scheme measured 9 times slower per call than the
primitive.  Provisional: follow the rule,
and list every benchmark a phase slows past noise, with the procedures
responsible, for a decision before the next phase; the bulk I/O of Phase 5
is the likeliest to stay.

### Are the string and `-ci` comparisons worth moving?

They are built on char comparisons, so the rule moves them, but they are
variadic and called in loops (`string<?` in sorting), and the `-ci` ones
fold every char per call.  Provisional: move them, under the item above.

## Settled in review

- The rule's first question keeps a type's predicate, its own basic
  operations, and its basic constructor, an immediate's included.  So
  `not`, `boolean?`, `void`, `eof-object`, the five char comparisons, and
  the numeric tower's predicates stay primitives.
- `list` and `length` stay primitives: lists' constructor and operation.
- `identity` is public, in `(minim)`; `make-parameter` uses it.
- R5RS's `exact->inexact` and `inexact->exact` keep their names through a
  new `procedure-rename`, after Racket's, rather than printing as
  `inexact` and `exact`.
- The `c[ad]+r` stay primitives: keeping their errors in Scheme needs a
  helper no simpler than the C.
- `equal?`, `equal-hash`, and `string-hash` stay in C.

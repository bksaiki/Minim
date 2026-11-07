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
that Scheme expresses easily is C: `string-append`, `vector-fill!`,
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
   - a conversion between a type and lists (`string->list`,
     `list->vector`), which the prelude's higher-order procedures build
     on;
   - what only C can do: the OS, the collector, the evaluator (`apply`,
     `values`), or C's tables and libraries (Unicode, `libm`, `strtod`).

   Operations and constructors built from these (`string-append`,
   `string->vector`, `char-ci<?`, `zero?`) are not needed.
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
| Strings and symbols | `symbol=?`, `string`, `string-append`, `substring`, `string-copy`, `string-copy!`, `string-fill!`, the five comparisons, the five `-ci` comparisons: built on chars | `string?`, `symbol?`: predicates; `make-string`: constructor; `string->list`, `list->string`: conversions; `string-length`, `string-ref`, `string-set!`, `symbol->string`, `string->symbol`, `%getprop`, `%putprop!`: representation and interning; `string-upcase`, `-downcase`, `-foldcase`: Unicode's full mappings |
| Vectors | `vector`, `vector-copy`, `vector-copy!`, `vector-fill!`, `vector-append`, `string->vector`, `vector->string` | `vector?`: predicate; `make-vector`: constructor; `vector->list`, `list->vector`: conversions; `vector-length`, `vector-ref`, `vector-set!`: representation |
| Bytevectors | `bytevector`, `bytevector-copy`, `bytevector-copy!`, `bytevector-append` | `bytevector?`: predicate; `make-bytevector`: constructor; `bytevector-length`, `bytevector-u8-ref`, `bytevector-u8-set!`: representation; `utf8->string`, `string->utf8`: a codec, not easily Scheme |
| Numbers | `finite?`, `infinite?`, `nan?`, `zero?`, `positive?`, `negative?`, `odd?`, `even?`, `abs`, `max`, `min`, `exact->inexact`, `inexact->exact` | `number?`, `complex?`, `real?`, `rational?`, `integer?`, `exact-integer?`, `exact?`, `inexact?`, `fixnum?`, `flonum?`: predicates; `+` `-` `*` `/` and the comparisons: numbers' operations; `exact`, `inexact`, `floor`, `ceiling`, `truncate`, `round`, `quotient`, `remainder`: conversion and `libm`; `number->string`, `string->number`: the printer and parser |
| Ports | `close-input-port`, `close-output-port`: `close-port` after a check; `%read-line`, `%read-string`, `%write-string`, `%read-bytevector`, `%read-bytevector!`, `%write-bytevector`: loops over a char or byte | the predicates; `eof-object`: an immediate's constructor; the rest: representation and the OS |
| Errors, records, syntax, system | none | representation, the expander's aliases, the OS, the collector |

About 55 move.  Notes on the harder ones:

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
benchmark that slows past noise (see Settled in review).

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

**Done.**  Diverged from the plan:

- The strings are in a new `scheme/strings.scm`, after `lists.scm`, not in
  `sequences.scm`: `records.scm` calls `string-append` while the prelude
  loads.
- `string-copy!` and `string-fill!` raise `immutable string` before
  writing, which Scheme can't test, so `mutable-string?` is a new
  primitive, after Chez's, public in `(minim)`.

Defaults of an omitted `end` go through `%length-or-0`, so the full form
checks the arguments in the primitive's order: `(string-copy! 'x 0 'y)`
still names `x`.  `string-ci` comparisons compare the strings'
`string-foldcase`s, so full folding is unchanged.  Regenerating the boot
file needed the old `string.c` back once: the old boot file defined the
strings after `records.scm` needed them.  `tests/repl/strings.scm` gained
`mutable-string?` and two argument-order errors; the rest of its output is
unchanged.

`string->list` and `list->string` were moved, measured, and restored as
primitives: `string-map`, `string-for-each`, and library paths
(`%split`, `%path-dir`) build on them, so their cost multiplied.  The
rule's first question now names conversions between a type and lists,
which keeps `vector->list` and `list->vector` in Phase 3 too.

Benchmarks against `main`, release: the suite unchanged (11 runs; fib
0.482 to 0.478 s median, expand 0.402 to 0.400).  Microbenchmarks, 3 runs
each:

| Loop | `main` | Phase 2 | Slower |
|---|---|---|---|
| 200,000 `(string-append "hello" " " "world")` | 0.12 s | 5.1 s | 43x |
| 200,000 `(string<? "hello world" "hello worle")` | 0.11 s | 4.3 s | 39x |
| 1,000 `(list->string (string->list s))`, 1,000 chars | 0.02 s | 2.3 s, then 0.02 s restored | 100x, then none |

Decided: the rest stay in Scheme; acceleration comes back only where a
program truly needs it.

### Phase 3 - Vectors and bytevectors

- `src/vector.c`, `src/bytevector.c`: the primitives in the review out;
  `vector->list` and `list->vector` stay.
- `scheme/sequences.scm`: them, or an earlier file if the prelude calls
  them while loading, as `strings.scm` for strings.

Tests: the existing `vectors` and `bytevectors` REPL tests.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'vectors|bytevectors|boot|r7rs'
```

**Done.**  `expander.scm` calls `vector` while the prelude loads, so
`sequences.scm` moved to right after `lists.scm`, and Phase 2's
`strings.scm` folded into it: strings, vectors, bytevectors, and their
higher-order procedures.  Their shared helpers take the type's procedures
as arguments: `%copy!` (overlap-safe, and across types, for
`string->vector` and `vector->string`), `%subsequence`, `%append`, and the
checked `%copy`, `%copy-to!`, and `%fill!`.  The string procedures now use
them; `%length-or-0` takes the type's predicate and length.
`tests/repl/vectors.scm` and `bytevectors.scm` gained argument-order
errors; the rest of their output is unchanged.

Benchmarks against `main`, release: the suite within noise (11 runs;
expand 0.417 to 0.431 s median, 0.391 to 0.398 minimum, from `vector` in
`%make-top`).  Microbenchmarks, 3 runs each:

| Loop | `main` | Phase 3 | Slower |
|---|---|---|---|
| 1,000,000 `(vector 1 2 3)` | 0.61 s | 0.80 s | 1.3x |
| 1,000 `vector-copy` of 1,000 elements | 0.01 s | 1.1 s | 100x |
| 200,000 `(bytevector-append #u8(1 2 3) #u8(4 5))` | 0.12 s | 3.5 s | 30x |

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

**Done.**  `scheme/numbers.scm` loads after `sequences.scm`, whose `%check`
it uses; nothing calls its procedures while the prelude loads.
`procedure-rename` (`src/control.c`) copies a primitive with
`minim_make_primitive` and a closure with `minim_make_closure` and
`minim_set_closure_name`.  `abs` compares with `%fixnum-min`, the least
fixnum, to raise `abs: fixnum overflow` as before.  `odd?` uses
`remainder` on fixnums and, on flonums, whether `n / 2` is integral, as
`remainder` takes only fixnums.  `max` and `min` share `%extreme`, which
makes the result inexact at the end rather than at each step; the two
differ only for fixnums past 2^53, which a double rounds.  A
continuation isn't tested with `procedure-rename`: `(minim)` reaches one
only through `call/cc`, which wraps it in a closure.  `tests/repl/core.scm`
gained `procedure-rename`; `numbers.scm`'s output is unchanged.

Benchmarks against `main`, release: the suite within noise (11 runs; fib
0.497 to 0.507 s median, expand 0.416 to 0.421).  Microbenchmarks, 3 runs
each:

| Loop | `main` | Phase 4 | Slower |
|---|---|---|---|
| 1,000,000 `(zero? i)` in a countdown | 0.36 s | 0.92 s | 2.6x |
| 1,000,000 `(max i acc 3)` | 0.57 s | 8.9 s | 15x |

### Phase 5 - Ports

- `src/port.c`: `close-input-port`, `close-output-port`, and the bulk
  reads and writes out.
- `scheme/ports.scm`: them, over `%read-char`, `%peek-char`, `%write-char`,
  `%read-u8`, `%write-u8`.

Tests: the existing `ports` REPL test and `test_port`.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'ports|port|boot|r7rs'
```

**Done.**  The moved procedures keep their `%` names in `ports.scm`, so the
wrappers that default the port are unchanged; they check their port with
`%check-open`, as C's `check_open`, and their ranges and counts with
`sequences.scm`'s helpers and a new `%check-count`.  `reserve_bytes`
(`src/port.c`) keeps its general size, though `write_byte`, its last
caller, asks for one byte.  `tests/repl/ports.scm` gained `read-line` over
`\r\n` and `\r`, and four argument errors; the rest of its output is
unchanged.

Benchmarks against `main`, release: the suite within noise (11 runs; fib
0.492 to 0.498 s median, expand 0.398 to 0.408).  Microbenchmarks, 3 runs
each:

| Loop | `main` | Phase 5 | Slower |
|---|---|---|---|
| `read-line` over a 20,000-line, 1 MB file | 0.04 s | 1.2 s | 30x |
| 100,000 `(write-string "hello world" out)` | 0.13 s | 1.2 s | 9x |
| 10 `read-bytevector` of 100,000 bytes | 0.01 s | 1.1 s | 100x |

### Phase 6 - Investigate the moved procedures' overhead

An investigation, not a fixed change: Phases 2 to 5 accept the moved
procedures' cost (Settled in review), and this phase looks for what is cheap
to win back without returning to C.  `(vector-copy v)` of n elements makes
about 10 closure calls of fixed overhead, then a closure call and 6
primitive calls per element in `%copy!`'s `do` loop.

**The principle:** change the prelude only where the result is idiomatic
Scheme or removes redundant work, never to dodge this interpreter's call
cost, which is `compile.md`'s to lower.  Chez (`s/5_6.ss`) and Chibi
(`lib/init-7.scm`) both fill in an omitted argument's default and go
straight to the full code, not re-entering the procedure; Chez does so
though its compiler would make re-entering cheap.  Candidates:

- Short `case-lambda` clauses calling the checked helper (`%copy`,
  `%fill!`, ...) directly with their defaults, as Chez and Chibi do.
- `%extreme`, behind `max` and `min`: it calls `nan?`, which checks again
  that its argument is a number, twice per argument; `(= y y)` inline
  drops the redundant check.
- `%copy!` looping over one index with a running offset, saving two
  primitive calls per element, if no less clear.

Not a candidate: flattening `%check-range`'s two `%check-position`s, which
a compiler inlines; it would help only this interpreter.

Measure each against the phase's start with the microbenchmarks recorded in
Phases 2 to 5; keep what wins past noise, and record the rest here.  The
per-element closure call stays, so most of the gap to C remains; closing it
is `compile.md`'s lexical addressing, or C, where a program needs it.

Tests: the existing `strings`, `vectors`, `bytevectors`, `numbers`, and
`ports` REPL tests, unchanged.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'strings|vectors|bytevectors|numbers|ports|boot|r7rs'
```

**Done.**  Measured against the phase's start, release, median of 5
alternating runs:

| Candidate | Loop | Before | After | Kept |
|---|---|---|---|---|
| Short clauses calling `%copy` directly (tried on `vector-copy`) | 300,000 copies of a 3-element vector | 2.96, 3.23, 3.14 s | 2.90, 3.24, 3.17 s | no: noise, and longer code |
| `%extreme` testing `(= y y)`, not calling `nan?` | 1,000,000 `(max i acc 3)` | 9.72 s | 6.87 s | no: `nan?` says what it tests |
| `%copy!` adding an offset computed once | 1,000 `vector-copy`s of 1,000 elements | 0.59, 0.57, 0.61 s | 0.53, 0.52, 0.56 s | yes, and clearer |
| | 200,000 `(string-append "hello" " " "world")` | 3.12 s | 3.05 s | |

The short clauses' re-entry is a few closure calls of fixed overhead, small
next to the loop, so they stay as they are.  `%extreme` keeps `nan?`: the
inline test wins only by skipping a call, and obscures what it tests.  The suite is unchanged (11
runs; fib 0.279 to 0.278 s median, expand 0.230 to 0.227).

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

None.  Both were decided in Phase 2; see Settled in review.

## Settled in review

- The rule's first question keeps a type's predicate, its own basic
  operations, and its basic constructor, an immediate's included.  So
  `not`, `boolean?`, `void`, `eof-object`, the five char comparisons, and
  the numeric tower's predicates stay primitives.
- `list` and `length` stay primitives: lists' constructor and operation.
- Conversions between a type and lists stay primitives: `string->list`,
  `list->string`, `vector->list`, `list->vector`.
- Speed: the rule doesn't weigh it, and Minim interprets the prelude, so
  a moved procedure costs a closure call and handles each element through
  the interpreter (`char<?` measured 9 times slower, `string-append` 43).
  Accepted, the string and `-ci` comparisons included: acceleration
  returns to C only where a program truly needs it.  Each phase still
  records its measurements.
- `identity` is public, in `(minim)`; `make-parameter` uses it.
- R5RS's `exact->inexact` and `inexact->exact` keep their names through a
  new `procedure-rename`, after Racket's, rather than printing as
  `inexact` and `exact`.
- The `c[ad]+r` stay primitives: keeping their errors in Scheme needs a
  helper no simpler than the C.
- `equal?`, `equal-hash`, and `string-hash` stay in C.

# Flonums: inexact real numbers

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

Minim's only numbers are 61-bit fixnums (`src/number.c`): `+`, `-`, `*`,
`quotient`, `remainder`, the comparisons, `zero?`, `number->string`.
Overflow raises.  The reader reads `1.5` as a symbol, so `1.0` is an
unbound variable.

| Report | Failing | Cause |
|---|---|---|
| Numeric syntax | 105 errors | `1.0`, `+inf.0`, `#x`, `#e`, ... |
| 6.2 Numbers | 120, 76 errors | flonums, the predicates, `/`, rationals, bignums, ... |
| 4.2, 6.8, 6.10, ... | a few each | `number?`, `round`, `abs`, `exact`, ... |

The goal is flonum support: the object, its syntax, arithmetic and
comparison across fixnums and flonums, and the predicates and conversions
R7RS builds on them.  Not here: bignums (fixnum overflow still raises),
rationals, `(scheme complex)`, and most of `(scheme inexact)`'s functions
(`sqrt`, `exp`, `log`, the trigonometric ones), which are thin wrappers over
`math.h` for a later change.

## Design

**A flonum is a boxed IEEE double on primary tag `010`**, reserved for it
(`gc.md`), as Chez boxes them: `[ double | unused ]`, 16 bytes, since every
object holds a forwarding record.  It gets its own space, `MINIM_KIND_FLONUM`,
with no traced fields, so the collector never scans its bits as pointers.
`minim.h` gets `minim_make_flonum`, `minim_is_flonum`, `minim_flonum_value`.

**Syntax.**  One parser, in `src/number.c`, serves the reader and
`string->number`: an optional prefix, `#x` `#o` `#b` `#d` and `#e` `#i` in
either order; then an integer in the radix, or, in radix 10, a decimal with
`.` or an exponent (`1.5`, `.5`, `1.`, `1e10`, `-2.5e-3`); or `+inf.0`,
`-inf.0`, `+nan.0`, `-nan.0`.  Decimals go through `strtod`, which rounds
correctly.  `#e` of a decimal is its fixnum if integral, else an error (no
rationals); `#i` of an integer is a flonum.

**Printing** is shortest round-trip: the fewest significant digits, 1 to
17, that `strtod` reads back as the same double, then in Scheme syntax: a
`.0` if it has no `.` or exponent, an exponent without `+` or leading zeros
(`1e21`), and `+inf.0`, `-inf.0`, `+nan.0`.  `-0.0` keeps its sign.

**Arithmetic** dispatches on its operands; the fixnum path comes first and
stays as it is.  A flonum operand makes the result a flonum (contagion).
`/` is new: exact when the fixnums divide evenly, else a flonum (no
rationals); an exact zero divisor raises, an inexact one gives an infinity
or NaN.

**Comparison** of a fixnum and a flonum is exact, though a fixnum may not
fit a double: compare `(double)i` with `d`, and if equal, compare `i` with
`(intptr_t)d`, which is then integral and in range.  Rounding is monotonic,
so an inequality between `(double)i` and `d` holds for `i` too.  NaN is
unordered: every comparison with it is false.

**Equivalence.**  `eqv?` compares flonums by their bits, so `(eqv? 0.0
-0.0)` is false and a NaN is `eqv?` to the same NaN; `equal?` uses
`eqv?` for them, and `equal-hash` hashes their bits.  `eq?` is identity:
flonums are boxed.  `memv`, `assv`, and `case` follow `eqv?`.

**The procedures**, in C:

- Arithmetic: `+`, `-`, `*`, `/`, `abs`, `max`, `min` (inexact if any
  argument is), `floor`, `ceiling`, `round` (to even), `truncate`.
- Comparison: `=`, `<`, `>`, `<=`, `>=`, `zero?`, `positive?`, `negative?`,
  `odd?`, `even?` (of integers, including integral flonums).
- Predicates: `number?`, `complex?`, `real?`, `rational?` (finite),
  `integer?` (integral), `exact?`, `inexact?`, `exact-integer?`, and
  Minim's own `flonum?`, as Chez's.
- Conversion: `exact` (an integral flonum in fixnum range; else raises),
  `inexact`, R5RS's `exact->inexact` and `inexact->exact`, `number->string`
  and `string->number` (radix 10 for flonums).
- `(scheme inexact)`, partly: `finite?`, `infinite?`, `nan?`.

## Phases

### Phase 1 - The object

- `include/minim.h`, `src/object.c`: the tag, size, constructor, accessors.
- `src/heap.h`, `src/gc.c`: `MINIM_KIND_FLONUM`; copying, size, and no
  traced fields (`pointer_fields`, the scan's tag table).
- `src/print.c`: shortest round-trip printing.

First because everything after builds on the object.

Tests: `tests/test_object.c`, the accessors and `minim_tag`;
`tests/test_gc.c`, flonums survive collections, young and promoted, in their
own space, and an old vector holding a young flonum keeps it (the barrier);
`tests/test_print.c`, `1.0`, `0.1`, `-0.0`, `1e21`, `1e-7`, `123.456`,
`5e-324`, `1.7976931348623157e308`, the infinities and NaN.

```sh
cmake --build --preset dev && ctest --preset dev -R 'object|gc|print|heap' && ctest --preset stress -R 'object|gc|print'
```

### Phase 2 - Syntax

- `src/number.c`: the parser; `string->number`; `number->string` of
  flonums.
- `src/read.c`: tokens through the parser instead of `parse_fixnum`, and
  `#x`, `#e`, ... starting a number.
- `scheme/standard.scm`: `string->number`.  `scheme/prelude.boot`
  regenerated.

Separate because it is the reader, before any arithmetic.

Tests: `tests/test_read.c`: each decimal form, the prefixes in both orders,
the infinities and NaN, `#e1.0` and `#e1.5`'s error, a fixnum out of range,
tokens that stay symbols (`.`, `...`, `+`, `-`, `1+`, `+i`); every Phase 1
print test read back to the same double; `tests/repl/numbers.scm`,
`string->number` with a radix and of a non-number (`#f`).  The numeric
syntax report updated.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'read|numbers|r7rs'
```

### Phase 3 - Equivalence

- `src/equal.c`: `eqv?`, its own primitive; `equal?` and `equal-hash` of
  flonums.

Tests: `tests/repl/equality.scm`: `eqv?` of equal flonums, of `0.0` and
`-0.0`, of NaNs, of `1` and `1.0`; `equal?` inside lists; `memv` and `case`
on flonums; `equal-hash` of equal flonums.

```sh
cmake --build --preset dev && ctest --preset dev -R 'equality|r7rs'
```

### Phase 4 - Arithmetic and comparison

- `src/number.c`: generic `+`, `-`, `*`, `/`, the comparisons, `abs`,
  `max`, `min`, `zero?`, `positive?`, `negative?`, `odd?`, `even?`.
- `scheme/standard.scm`: the new names.  `scheme/prelude.boot`
  regenerated.

Tests: `tests/repl/numbers.scm`: contagion in each operation; `/` exact and
inexact, and by exact and inexact zero; comparisons across types, with
`(= 9007199254740993 9007199254740992.0)` false and NaN unordered; `max`'s
exactness; `odd?` of `3.0` and of `3.5`'s error; fixnum overflow still
raising.  Measure `bench/run.sh` against `main`: the fixnum path must not
slow.  The reports updated.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'numbers|r7rs' && ctest --preset stress -R numbers
```

### Phase 5 - Predicates and conversion

- `src/number.c`: the predicates, `exact`, `inexact`, `floor`, `ceiling`,
  `round`, `truncate`, `finite?`, `infinite?`, `nan?`.
- `scheme/standard.scm`: the names; `(scheme inexact)` with its three;
  `exact->inexact` and `inexact->exact` in `(scheme r5rs)`.
  `scheme/prelude.boot` regenerated.

Tests: `tests/repl/numbers.scm`: each predicate on a fixnum, an integral
flonum, a fraction, an infinity, and NaN; `exact` of `2.0`, `2.5`, and
`+inf.0`; rounding of `2.5`, `-2.5`, `3.5`, and `-0.4` (to `-0.0`); the
names imported from `(scheme inexact)`.  The reports updated.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'numbers|libraries|r7rs'
```

### After the last phase

Sync the docs: `gc.md` (the tag, layout, and space), `primitives.md`
(Fixnums becomes Numbers), `repl.md` (printing flonums), `r7rs-small.md`
(milestone 7's flonums done, coverage, the conformance table).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

None.  Settled:

- `/` of fixnums that don't divide evenly returns a flonum, until rationals
  land.
- `quotient` and `remainder` take fixnums only, as today; `modulo` and the
  `floor/` family are left to a change on integer division.

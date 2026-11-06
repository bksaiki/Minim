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

**Done.**  Diverged from the design: the layout is `[ 0 | double ]`, not
`[ double | unused ]`.  The collector takes an object whose word 0 is
`MINIM_FORWARD` as forwarded, and a double's bits can be that (a
subnormal); word 0 is now always 0.  `test_gc`'s `test_flonums` covers a
flonum with those bits, and fails with the double in word 0.
`pointer_fields` returns none for a flonum.  Printing uses the shortest
`%.*e` that `strtod` reads back, positionally for exponents -6 to 20 (as
JavaScript does): `0.000001` but `1e-7`, `100000000000000000000.0` but
`1e21`.  `count_predicates` (`tests/test.h`) gained `minim_is_bytevector`,
missed by the bytevectors work, and `minim_is_flonum`.

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

**Done.**  The parser is `minim_parse_number` (`src/number.h`), returning
`MINIM_PARSE_OK`, `_NOT_NUMBER`, `_OUT_OF_RANGE`, or `_NOT_EXACT`; the
reader raises `fixnum out of range` or `no exact representation` for the
last two, `string->number` the same with its name.  The reader's `#` tokens
now keep their `#`, so a bad one's irritant is `"#x1.5"`, not `"x1.5"`.
Prefixes and exponent markers are case-insensitive; `+inf.0` and `+nan.0`
aren't.  An inexact integer in radix 2, 8, or 16 is accumulated in a double,
rounding at each step past 2^53.  `number->string` of a flonum writes it as
the printer does, through a string port.  `string->number` of a rational,
`"1/2"`, is `#f`.  Reports: Numeric syntax 8 to 90 passed (its forms now
read, so it counts more tests), the rest mostly `equal?` of flonums (Phase
3), complex numbers, and rationals; 6.9 all 39; Read syntax 78 to 85; 6.2's
errors 76 to 18.  `(scheme r5rs)` gained `string->number`.

### Phase 3 - Equivalence

- `src/equal.c`: `eqv?`, its own primitive; `equal?` and `equal-hash` of
  flonums.

Tests: `tests/repl/equality.scm`: `eqv?` of equal flonums, of `0.0` and
`-0.0`, of NaNs, of `1` and `1.0`; `equal?` inside lists; `memv` and `case`
on flonums; `equal-hash` of equal flonums.

```sh
cmake --build --preset dev && ctest --preset dev -R 'equality|r7rs'
```

**Done.**  `is_eqv` compares flonums' bits; `equal?` falls back to it for
values not compared by contents, and `equal-hash` mixes the bits under a
new `HASH_FLONUM` marker.  `memv`, `assv`, and `case` needed no change.
Reports: Numeric syntax 90 to 119 passed; 6.2 27 to 28.  Numeric syntax's
remaining failures: exponent markers `s` `f` `d` `l` (R5RS's, not R7RS's),
case-insensitive `+NAN.0` and `+InF.0`, rationals, and complex numbers.

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

**Done.**  One path, not a fixnum fast path beside a generic one: each
argument becomes a `num` (a fixnum, or a flonum if `flo`), and `fold` applies
an operation pairwise, left to right, allocating only the result.  The
comparisons return an outcome bit (`LT`, `EQ`, `GT`, `UNORDERED`), so each
procedure is a mask, and `zero?`, `positive?`, `negative?` compare with 0.
Left to right means a fixnum overflow before a flonum argument still raises:
`(+ 1152921504606846975 1 1.0)` raises, `(+ 1.0 1152921504606846975 1)`
doesn't.  `(* 0 1.5)` is `0.0`.  `max` and `min` give NaN if any argument is.
Type errors now say `not a number`; `/`'s errors have no irritant, as
overflow's.  `CMakeLists.txt` links `libm` (not on MSVC), now that `fabs`
and `fmod` are called.  `quotient` and `remainder` stay fixnum-only.
Benchmark against this branch's previous commit, 15 runs, release: fib
0.263 to 0.265 s median, let 0.269 to 0.272, noise.  Reports: 6.2 28 to 74
passed; 6.11 24 to 29; 4.2, 4.3, 6.4, and 6.10 one or two more each;
Numeric syntax 119 to 121, its two new failures rationals (`1/2`).

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

**Done.**  `number?`, `complex?`, and `real?` are one primitive;
`exact-integer?` is `fixnum?`'s.  `exact?`, `inexact?`, `finite?`,
`infinite?`, and `nan?` raise on a non-number; the others are `#f`.  `exact`
raises `no exact representation` for a fraction, an infinity, or NaN, and
`fixnum out of range` past it; `inexact->exact` and `exact->inexact` share
`exact`'s and `inexact`'s functions, so their errors say `exact:` and
`inexact:`.  `round` is `nearbyint`, to even by the default rounding mode.
`is_integral` now also serves the parser's `#e` and `odd?`.  `(minim)`
gained `(scheme inexact)`, `flonum?`, `exact->inexact`, and
`inexact->exact`.  `libraries.scm`'s `(scheme r5rs)` library calls the
R5RS names.  Reports: 6.2 74 to 110 passed, its rest mostly `atan`,
`expt`, `log` and the other math functions, `modulo` and the `floor/`
family, and rationals; 4.2 58 to 61; 6.4 all 65.

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

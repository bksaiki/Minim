# Math: integer division, powers, roots, and transcendental functions

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

Numbers are fixnums and flonums (`src/number.c`, `primitives.md`): generic
arithmetic and comparison, the predicates, `exact`, `inexact`, and rounding.
What R7RS's number procedures still lack:

| Library | Missing |
|---|---|
| `(scheme base)` | `floor/` `floor-quotient` `floor-remainder` `truncate/` `truncate-quotient` `truncate-remainder` `modulo` `gcd` `lcm` `square` `exact-integer-sqrt` `expt` `numerator` `denominator` `rationalize` |
| `(scheme inexact)` | `exp` `log` `sin` `cos` `tan` `asin` `acos` `atan` `sqrt` |
| `(scheme r5rs)` | the same, but for the `/` family, `square`, and `exact-integer-sqrt` |

`quotient` and `remainder` take fixnums only, so `(remainder -13 -4.0)`
raises.  These account for most of 6.2's 92 failures and 17 errors, and for
the failures of 4.2, 6.8, 6.10, 6.11, and 6.12.  The rest of 6.2 needs
complex numbers, rationals, or bignums (`(expt 2 119)`), which this plan
leaves out.

## Design

**C or Scheme**, by the primitives rule (`primitives.md`): C keeps
`quotient` and `remainder`, numbers' basic operations, and what calls
`libm`: `sqrt`, the transcendental functions, and `%expt` over `pow`.
The rest is Scheme, in `scheme/numbers.scm`, slower, as the rule accepts.

**Integer division** (R7RS 6.2.6) takes integers: fixnums or integral
flonums.  The result is inexact if either argument is.  A zero divisor,
exact or inexact, raises `division by zero`.

- `quotient` and `remainder`, primitives, are C's `/` and `%` on fixnums;
  on flonums, the remainder is `fmod` and the quotient `(n - r) / m`, exact
  while the integers fit in a double.  `truncate-quotient` and
  `truncate-remainder` are them by `procedure-rename`, so their errors say
  `quotient` and `remainder`.
- `floor-quotient` and `floor-remainder` adjust a nonzero remainder whose
  sign differs from the divisor's; `modulo` is R5RS's name for
  `floor-remainder`.
- `floor/` and `truncate/` return both: `(values (floor-quotient n m)
  (floor-remainder n m))`.
- `(quotient MINIM_FIXNUM_MIN -1)` still raises `fixnum overflow`.

**`gcd` and `lcm`** take any number of integers: `(gcd)` is 0, `(lcm)` is 1;
results are non-negative, inexact if any argument is; `lcm` past the fixnums
raises `fixnum overflow`.

**Powers and roots.**

- `square` is `(* z z)`, overflow raising as `*` does.
- `expt` of an exact base and a non-negative exact exponent is exact, by
  repeated squaring, raising `fixnum overflow` past the fixnums.  A negative
  exact exponent divides 1 by the positive power, as `/` does: exact if it
  divides evenly, else a flonum.  Otherwise it is `%expt`, `pow` on
  doubles.  `(expt 0 0)` is 1, `(expt 0.0 0)` is 1.0.
- `exact-integer-sqrt` of an exact non-negative integer `k` returns `s` and
  `k - s^2`, with `s^2 <= k < (s+1)^2`: `s` from `(sqrt (inexact k))`,
  corrected by one either way, as a double may round.
- `sqrt` of an exact perfect square is exact, as Chez's: `(sqrt 9)` is 3.
  Otherwise it is a flonum.

**Transcendental functions** are C's: `exp`, `log` (with an optional base:
`(log z b)` is `log z / log b`), `sin`, `cos`, `tan`, `asin`, `acos`, `atan`
(with an optional second argument, `atan2`).  Their results are flonums,
except at exact special points, where they are exact, as Chez's: `(exp 0)`
1, `(log 1)` 0, `(sin 0)`, `(tan 0)`, `(asin 0)`, and `(atan 0)` 0, `(cos 0)`
1, `(acos 1)` 0, and `(atan 0 x)` 0 for an exact positive `x`.

**Results that would be complex.**  Without complex numbers, a function
whose true result is complex raises `<name>: no real result` if its
arguments are exact, as exact operations raise for results they can't
represent: `(sqrt -4)`, `(log -1)`, `(asin 2)`, `(acos 2)`.  With an
inexact argument it gives NaN, as IEEE and `/` do: `(sqrt -4.0)` is
+nan.0.

**Numerator and denominator**, Scheme.  An integer's numerator is itself and its
denominator 1, of its exactness.  A flonum is a dyadic rational: doubling it
until it is integral, counting the doublings, gives its numerator and a
denominator of 2 to their number, both flonums: `(numerator 5.5)` is 11.0,
`(denominator 5.5)` 2.0.  An infinity or NaN raises `not a rational`.

**`rationalize`**: the simplest rational
in `[x - y, x + y]`, by R7RS's definition (the Stern-Brocot recursion on
`floor`).  Generic arithmetic gives its exactness: exact integers give an
exact integer, and anything inexact a flonum.  Without rationals, an exact
non-integral result can't occur: the simplest rational in an interval with
integer ends is an integer.

**Libraries.**  `(scheme base)` gets the integer division, `gcd`, `lcm`,
`square`, `exact-integer-sqrt`, `expt`, `numerator`, `denominator`, and
`rationalize`; `(scheme inexact)` the transcendental functions and `sqrt`,
completing it; `(scheme r5rs)` R5RS's of them.

## Phases

### Phase 1 - Integer division, `gcd`, and `lcm`

- `src/number.c`: `quotient` and `remainder` on integral flonums.
- `scheme/numbers.scm`: `truncate-quotient`, `truncate-remainder`,
  `floor-quotient`, `floor-remainder`, `modulo`, `floor/`, `truncate/`,
  `gcd`, `lcm`.
- `scheme/standard.scm`: the names.  `scheme/prelude.boot` regenerated.

First, because it extends `quotient` and `remainder`, which exist, and the
rest builds on them.

Tests: `tests/repl/numbers.scm`: R7RS 6.2.6's sign table for `floor/` and
`truncate/` (`5 2`, `-5 2`, `5 -2`, `-5 -2`); `modulo` and `remainder` of
mixed signs; integral flonums giving flonums, `(remainder -13 -4.0)`;
`(quotient 7.5 2)` raising `not an integer`; zero divisors, exact and
inexact; the fixnum minimum over -1; `(gcd 32 -36)`, `(gcd)`,
`(lcm 32 -36)`, `(lcm 32.0 -36)`, `(lcm)`.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'numbers|boot|r7rs'
```

**Done.**  `quotient` and `remainder` take integers through a new
`integer_arg`, their type error now `not an integer`, not `not a fixnum`.
`floor-quotient` adjusts `quotient`'s result by one rather than computing
`(n - r) / m`, which could overflow beside the least fixnum.
`%floor-adjusts?` is the shared test.  `gcd` and `lcm` fold through
`%fold-integers`, which checks each argument and makes the result inexact
if any is, which Euclid's loop alone loses: `(gcd 0.0)` is 0.0.  `lcm`
divides before multiplying.  The Scheme procedures' errors come from the
primitives they call: `(modulo 7 0)` raises `remainder: division by zero`.
The 6.2 report: 110 passed, 92 failed, 17 errors to 129, 73, 17.

### Phase 2 - Powers, roots, and transcendental functions

- `src/number.c`: `%expt`, `sqrt`, `exp`, `log`, `sin`, `cos`, `tan`,
  `asin`, `acos`, `atan`, with the exact special points and `no real
  result`.
- `scheme/numbers.scm`: `square`, `expt`, `exact-integer-sqrt`.
- `scheme/standard.scm`: the names.  `scheme/prelude.boot` regenerated.

Tests: `tests/repl/numbers.scm`: `square` exact, inexact, and overflowing;
`expt` with each combination of exactness, `(expt 0 0)`, `(expt 0.0 0)`,
`(expt 2 -1)` as 0.5, `(expt -1 -3)` as -1, `(expt 2 61)` raising;
`exact-integer-sqrt` of 0, 4, 5, the fixnum maximum, and -1 raising; `sqrt`
of 9, 2, and 9.0; each transcendental function at a few points, `(log 100
10)`, `(atan 1 1)`; each importable from `(scheme inexact)` in a library.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'numbers|libraries|boot|r7rs'
```

**Done.**  The `libm` functions share `libm(a, f, at, value)`, which
returns the exact `value` at the exact `at`, and `check_real`, which raises
`no real result` for an exact argument out of the real domain.  `(log 1 b)`
is exact 0 for an exact base other than 1; `(log 0)` is -inf.0, a real
limit, not an error.  `sqrt` corrects `sqrt((double)k)` in `intptr_t`,
where `(s + 1)^2` can't overflow.  `exact-integer-sqrt` can't square in
fixnums: the largest fixnum rounds to 2^60 as a double, whose root squared
is one past the fixnums, so it compares `s` with `k / s`.  `expt` squares
the base only while bits of the exponent remain, so `(expt 2 59)` doesn't
overflow on a square it never uses.  The `(scheme base)` and `(scheme
r5rs)` lists were re-sorted and refilled at 92 columns, their width.
Tests compare non-special results within 1e-12, as `libm`s may differ in
the last bit.  The reports: 6.2 129 to 154 passed; 6.8, 6.10, 6.11, and
6.12 now pass entirely; 4.2 64 passed, 4 failed, 5 errors, the rest
needing bignums or rationals.

### Phase 3 - `numerator`, `denominator`, and `rationalize`

- `scheme/numbers.scm`: `numerator`, `denominator`, `rationalize`.
- `scheme/standard.scm`: the names.  `scheme/prelude.boot` regenerated.

Last, as the least used.

Tests: `tests/repl/numbers.scm`: `numerator` and `denominator` of 6, 5.5,
5.0, -0.75, and `+inf.0` raising; `(rationalize 3 1)` exact,
`(rationalize .3 .1)` as 0.3333333333333333, `(rationalize 5 0)`.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'numbers|boot|r7rs'
```

### After the last phase

Sync the docs: `primitives.md` (Numbers: each procedure; integer
division's conventions), `prelude.md` (`numbers.scm` in the file table and
its procedures in Library), `r7rs-small.md` (coverage: `(scheme base)`,
`(scheme inexact)` 12 of 12, `(scheme r5rs)`; milestone 7's math and
integer division done; the Numbers gap; the conformance table).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

None; see Settled in review.

## Settled in review

- Results that would be complex: raise `<name>: no real result` for exact
  arguments, NaN for inexact ones, matching how Minim already treats
  results it can't represent exactly and IEEE's flonums.  Reopen with
  complex numbers.
- Exact results at exact special points, as Chez and Racket give: `(exp 0)`
  is 1, `(sin 0)` 0, and so on (Design).  R7RS permits either, and Chibi's
  tests accept both.
- The plan follows the primitives rule, which postdates it: only
  `quotient`, `remainder`, and what calls `libm` are C.
- `quotient` and `remainder` stay the primitives; `truncate-quotient` and
  `truncate-remainder` are them renamed, their errors naming `quotient` and
  `remainder`.

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

**Integer division** (R7RS 6.2.6) takes integers: fixnums or integral
flonums.  The result is inexact if either argument is.  A zero divisor,
exact or inexact, raises `division by zero`.

- `truncate-quotient` and `truncate-remainder` are C's `/` and `%`;
  `quotient` and `remainder` are R5RS's names for them.
- `floor-quotient` and `floor-remainder` adjust a nonzero remainder whose
  sign differs from the divisor's; `modulo` is R5RS's name for
  `floor-remainder`.
- On flonums, the remainder is `fmod`, adjusted likewise, and the quotient
  `(n - r) / m`, exact while the integers fit in a double.
- `floor/` and `truncate/` return two values, which a C primitive can't, so
  they are Scheme, in a new prelude file, `scheme/numbers.scm`:
  `(values (floor-quotient n m) (floor-remainder n m))`.
- `(quotient MINIM_FIXNUM_MIN -1)` still raises `fixnum overflow`.

**`gcd` and `lcm`** take any number of integers: `(gcd)` is 0, `(lcm)` is 1;
results are non-negative, inexact if any argument is; `lcm` past the fixnums
raises `fixnum overflow`.

**Powers and roots.**

- `square` is `(* z z)`, overflow raising as `*` does.
- `expt` of an exact base and a non-negative exact exponent is exact, by
  repeated squaring, raising `fixnum overflow` past the fixnums.  A negative
  exact exponent divides 1 by the positive power, as `/` does: exact if it
  divides evenly, else a flonum.  Otherwise it is `pow` on doubles.
  `(expt 0 0)` is 1, `(expt 0.0 0)` is 1.0.
- `exact-integer-sqrt` of an exact non-negative integer `k` returns `s` and
  `k - s^2`, with `s^2 <= k < (s+1)^2`.  `%exact-integer-sqrt` (C) finds
  `s` from `sqrt((double)k)`, then corrects it by one either way;
  `exact-integer-sqrt` (Scheme) adds the remainder and returns both.
- `sqrt` of an exact perfect square is exact, as Chez's: `(sqrt 9)` is 3.
  Otherwise it is a flonum.

**Transcendental functions** are C's: `exp`, `log` (with an optional base:
`(log z b)` is `log z / log b`), `sin`, `cos`, `tan`, `asin`, `acos`, `atan`
(with an optional second argument, `atan2`).  Their results are always
flonums, as R7RS permits: `(exp 0)` is 1.0.

**Numerator and denominator.**  An integer's numerator is itself and its
denominator 1, of its exactness.  A flonum is a dyadic rational: doubling it
until it is integral, counting the doublings, gives its numerator and a
denominator of 2 to their number, both flonums: `(numerator 5.5)` is 11.0,
`(denominator 5.5)` 2.0.  An infinity or NaN raises `not a rational`.

**`rationalize`** is Scheme, in `scheme/numbers.scm`: the simplest rational
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

- `src/number.c`: the six quotient and remainder primitives, `quotient`,
  `remainder`, and `modulo` among them; `gcd`, `lcm`.
- `scheme/numbers.scm` (new; `MINIM_SCHEME_SOURCES`, after `lists`):
  `floor/`, `truncate/`.
- `scheme/standard.scm`: the names.  `scheme/prelude.boot` regenerated.

First, because it extends `quotient` and `remainder`, which exist, and
sets up `numbers.scm` for the later phases.

Tests: `tests/repl/numbers.scm`: R7RS 6.2.6's sign table for `floor/` and
`truncate/` (`5 2`, `-5 2`, `5 -2`, `-5 -2`); `modulo` and `remainder` of
mixed signs; integral flonums giving flonums, `(remainder -13 -4.0)`;
`(quotient 7.5 2)` raising `not an integer`; zero divisors, exact and
inexact; the fixnum minimum over -1; `(gcd 32 -36)`, `(gcd)`,
`(lcm 32 -36)`, `(lcm 32.0 -36)`, `(lcm)`.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'numbers|boot|r7rs'
```

### Phase 2 - Powers, roots, and transcendental functions

- `src/number.c`: `square`, `expt`, `%exact-integer-sqrt`, `sqrt`, `exp`,
  `log`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`.
- `scheme/numbers.scm`: `exact-integer-sqrt`.
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

### Phase 3 - `numerator`, `denominator`, and `rationalize`

- `src/number.c`: `numerator`, `denominator`.
- `scheme/numbers.scm`: `rationalize`.
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

### Results that would be complex?

`(sqrt -4)` is `+2i` in R7RS, and so are `log` of a negative number, `asin`
and `acos` outside [-1, 1], and `expt` of a negative base and a
non-integral exponent.  Without complex numbers, C returns NaN.  Raising an
implementation restriction is more honest for exact arguments but costs a
check in each function.  Provisional: NaN, as C and IEEE give; reopen with
complex numbers.

### Exact results for exact arguments?

Chez gives `(exp 0)` as 1 and `(sin 0)` as 0; R7RS permits either, and
Chibi's tests accept both.  Provisional: always inexact, except `sqrt` of a
perfect square, which R7RS's examples show exact.  Reopen if a program
depends on it.

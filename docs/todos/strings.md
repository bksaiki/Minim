# Strings: `(scheme base)`'s string procedures, full case mapping, and escapes

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

Strings are vectors of code points (`src/string.c`).  Missing from `(scheme
base)`: `string`, `string-copy`, `string-copy!`, `string-fill!`,
`string-map`, `string-for-each`, and `string->list`'s optional start and
end.  Missing from `(scheme char)`: `string-upcase`, `string-downcase`,
`string-foldcase`, and the five `string-ci` comparisons.  The reader lacks
the escapes `\a`, `\b`, `\r`, `\|`, and line continuations.

| Report | Failing | Cause |
|---|---|---|
| 6.7 Strings | 77, 1 error | the procedures above; Greek, `ß`, `İ`, `ǰ`, `ſ`, and final sigma among the case tests |
| 6.10 Control | 4 | `string-map`, `string-for-each` |
| 6.13 I/O | 2 errors, 1 read error | `string`; `\a` in a string |
| 6.4 Lists | 1 | `string-ci=?` |
| Read syntax | 8 | the escapes and line continuations |

Not here: `string->vector` and `vector->string` (`vectors.md`),
`string->utf8` and `utf8->string` (bytevectors), `string->number`
(numbers).

Builds on the chars work (#99): the Unicode tables and their generator
(`src/unicode.c`, `tools/unicode.py`), `(scheme char)`, and char case
mapping.

## Design

**Ranges.**  A helper in `src/prims.h`, `range_args`, reads optional start
and end arguments against a length, raising `msg` on a bad index; every
procedure taking `[start [end]]` uses it, `vector->list` included.

**The base procedures** are C primitives in `src/string.c`: `string` (of its
char arguments), `string-copy`, `string-copy!` (overlap-safe, `memmove`),
`string-fill!`, and `string->list` with a range.  The mutators raise on an
immutable string, a symbol's name, as `string-set!` does.

**`string-map` and `string-for-each`** take procedures, so they are Scheme,
in a new prelude file, `scheme/sequences.scm`: over one or more strings, to
the shortest's length (R7RS 6.10).

**Full case mapping**, after Unicode's default case algorithms (Unicode 3.13)
and Chez:

- `string-upcase` and `string-downcase` use the full mappings, which may
  change a string's length: `ß` upcases to `SS`, `İ` downcases to `i̇`.
  Downcasing maps `Σ` to final `ς` at the end of a word (`Final_Sigma`: a
  cased letter before it, none after, skipping case-ignorable chars).
- `string-foldcase` uses full folding: `Maß` folds to `mass`.
- The `string-ci` comparisons compare full foldings, a char at a time,
  without allocating.

The generator adds the full mappings that differ from the simple ones
(`SpecialCasing.txt`'s unconditional ones, `CaseFolding.txt`'s status `F`),
at most three chars each, and the `Cased` and `Case_Ignorable` properties
(`DerivedCoreProperties.txt`) to the property bits.

**Escapes.**  The reader adds `\a`, `\b`, `\r`, `\|`, and the line
continuation: `\`, intraline white space, a newline, and intraline white
space, all dropped.  The printer writes `\a`, `\b`, and `\r` instead of hex
escapes.

**The libraries.**  The new base names in `(scheme base)`; the 8 string
names in `(scheme char)`.

## Phases

### Phase 1 - Escapes

- `src/read.c`: the escapes and line continuations.
- `src/print.c`: `\a`, `\b`, `\r`.

First because it is independent and small.

Tests: `tests/test_read.c`, each escape and each continuation form from the
suite; `tests/test_print.c`, the three escapes.  The read-syntax and 6.13
reports updated.

```sh
cmake --build --preset dev && ctest --preset dev -R 'read|print|r7rs'
```

**Done.**  Read syntax: 68 to 76 passed; 6.13: one read error fewer (37
passed).  The continuation reads one code point past its white space and
pushes it back, so a string source's `unread` now returns to where its last
code point started (`source.prev`) instead of one byte back, which handled
only ASCII.  `\|` in Read syntax still fails: its test reads `#x7C`, a hex
number.

### Phase 2 - The base procedures

- `src/prims.h`: `range_args`; `src/vector.c`'s `vector->list` uses it.
- `src/string.c`: `string`, `string-copy`, `string-copy!`, `string-fill!`,
  `string->list`'s range.
- `scheme/standard.scm`: the names.  `scheme/prelude.boot` regenerated.

Tests: `tests/repl/strings.scm`: each procedure with and without its range,
`string-copy!` within one string both ways, a bad range's error,
`string-fill!` on `(symbol->string 'a)`'s error.  The 6.7 and 6.13 reports updated.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'strings|vectors|r7rs' && ctest --preset stress -R strings
```

**Done.**  6.7: 52 to 78 passed, its other 52 failures all case mapping;
6.13: 37 to 42 passed.  `range_args` returns a `range` struct; it reads the
start argument only to report a bad range, since it may be absent.
`substring` and `string-copy` share `copy_range`.  `(scheme r5rs)` gained
`string`, `string-copy`, and `string-fill!`.

### Phase 3 - `string-map` and `string-for-each`

- `scheme/sequences.scm`, in `MINIM_SCHEME_SOURCES`: the two procedures.
- `scheme/standard.scm`: their names.  `scheme/prelude.boot` regenerated.

Separate because it is Scheme, on Phase 2's `string`.

Tests: `tests/repl/strings.scm`: one string, two of different lengths, and
an empty one.  The 6.10 report updated.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'strings|r7rs'
```

### Phase 4 - Full case mapping

- `tools/unicode.py`, `src/unicode_data.inc` (regenerated), `src/unicode.h`,
  `src/unicode.c`: the full mappings, `Cased`, `Case_Ignorable`.
- `src/string.c`: `string-upcase`, `string-downcase`, `string-foldcase`, the
  `string-ci` comparisons.
- `scheme/standard.scm`: the 8 names in `(scheme char)`.
  `scheme/prelude.boot` regenerated.

Last because it is the largest, and the only one needing the generator.

Tests: `tests/test_unicode.c`, the full mappings of `ß`, `İ`, `ǰ`, and `ﬃ`;
`tests/repl/strings.scm`: each procedure on ASCII and Greek, `ß` both ways,
final sigma mid-word and at a word's end, `string-ci` comparisons across a
length-changing fold.  The 6.7 and 6.4 reports updated.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'unicode|strings|r7rs' && ctest --preset stress -R strings
```

### After the last phase

Sync the docs: `primitives.md` (Strings: the new procedures, ranges, full
case mapping), `prelude.md` (`sequences.scm`), `repl.md` (string escapes in
printing), `r7rs-small.md` (coverage: `(scheme base)`, `(scheme char)` 22 of
22; the conformance table; the ports milestone's reader gaps).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

None.  Settled: no language-specific case mapping (Turkish, Lithuanian,
Azeri), which needs a locale R7RS doesn't have; unconditional mappings and
`Final_Sigma` only.

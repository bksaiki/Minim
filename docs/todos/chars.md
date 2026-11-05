# Chars: `(scheme char)`'s char procedures, Unicode tables, and char names

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

`(scheme base)`'s char procedures are done (`src/char.c`): `char?`,
`char->integer`, `integer->char`, and the five comparisons.  `(scheme char)`
isn't defined: none of its 22 names exist.  This plan brings its 14 char
procedures; `strings.md` brings its 8 string procedures.

The reader knows four char names (`nul`, `tab`, `newline`, `space`); R7RS
has nine.  The printer has its own copy of the four.

| Report | Failing | Cause |
|---|---|---|
| 6.6 Characters | 59 | `char-ci=?` ..., `char-alphabetic?` ..., `digit-value`, `char-upcase` ... unbound; Greek, Thai, Arabic-Indic, and Gujarati cases among them |
| Read syntax | 4 | `#\null`, `#\alarm`, `#\backspace`, `#\return` |

Chars are Unicode scalar values (`integer->char` checks), so the procedures
are defined on all of Unicode, as R7RS 6.6 specifies, not just ASCII.

## Design

**Char names.**  One table in `src/char.c` of R7RS's nine names, `alarm`
`backspace` `delete` `escape` `newline` `null` `return` `space` `tab`, plus
Chez's `nul`, read by both the reader and the printer.  The printer writes
each char's first name, so `#\x0` prints as `#\null`, R7RS's, and reads
back in any R7RS implementation.

**Unicode tables, generated.**  A script, `tools/unicode.py` (Python 3,
standard library only), reads the Unicode Character Database files given on
its command line and writes `src/unicode_data.c`, which is checked in; the
build doesn't run the script.  The generated file names the UCD version.
`src/unicode.h` declares the queries, over sorted tables searched by binary
search, with an ASCII fast path:

| Query | Source |
|---|---|
| `minim_unicode_props(c)`: a bit set of alphabetic, upper, lower, white space | `DerivedCoreProperties.txt` (`Alphabetic`, `Uppercase`, `Lowercase`), `PropList.txt` (`White_Space`) |
| `minim_unicode_digit(c)`: 0-9, or -1 | `UnicodeData.txt`, general category `Nd` |
| `minim_unicode_upcase(c)`, `_downcase`, `_foldcase`: the simple mappings | `UnicodeData.txt` fields 12 and 13; `CaseFolding.txt` statuses `C` and `S` |

Properties are one table of ranges, each with its bit set, so one search
answers every predicate.  Case mappings are one table of `{c, upper, lower,
fold}` rows for every char with any mapping.  `Nd` chars come in runs of ten
from 0; the script checks it, so a digit's value is its offset from its
range's start, mod 10.

**The procedures** are C primitives in `src/char.c`, following R7RS 6.6:
`char-alphabetic?`, `char-numeric?` (`Nd`), `char-whitespace?`,
`char-upper-case?`, `char-lower-case?`, `digit-value` (`#f` for a non-digit),
`char-upcase`, `char-downcase`, `char-foldcase`, and `char-ci=?` `char-ci<?`
`char-ci>?` `char-ci<=?` `char-ci>=?`, which `compare` with the folded char
as the key.

**The library.**  `(scheme char)` in `scheme/standard.scm`, with these 14
names, and in `(minim)`'s list.

## Phases

### Phase 1 - Char names

- `src/char.c`, `src/char.h`: the names table.
- `src/read.c`: `read_char` reads by the table.
- `src/print.c`: `write_char` writes by the table.

First because it is independent and small.

Tests: `tests/test_read.c`, every name, and `nul`; `tests/test_print.c`,
every named char.  The read-syntax report updated.

```sh
cmake --build --preset dev && ctest --preset dev -R 'read|print|r7rs'
```

**Done.**  The table is `minim_char_names` in `src/char.c`, ending with a
NULL name.  Read syntax: 64 to 68 passed.  `#\x7f` now prints as
`#\delete`: `repl.md`'s example of a control char changes in the doc sync.

### Phase 2 - Unicode tables

- `tools/unicode.py`: the generator.
- `src/unicode_data.c` (generated), `src/unicode.h`, `src/unicode.c`: the
  tables and queries.  `CMakeLists.txt` builds them.

Separate because it is the data, with nothing using it yet; the generated
file is large and reviewed through the script.

Tests: `tests/test_unicode.c`: ASCII letters, digits, and space; `Λ` and
`λ` (case, both mappings); `ß` (lower, no simple upcase, folds to itself);
`ſ` (folds to `s`); `๐`, `٤`, `૦` (digits 0, 4, 0); U+00A0 and U+3000
(white space); an unassigned code point and U+10FFFF (nothing).

```sh
cmake --build --preset dev && ctest --preset dev -R unicode
```

**Done.**  Unicode 18.0.0.  Diverged from the design:

- The tables are `src/unicode_data.inc`, included by `src/unicode.c`, not a
  `.c` of their own: they stay `static`, with their types private and their
  lengths from `sizeof`.
- No ASCII fast path: lookups are `bsearch` over 2,115 property ranges and
  3,033 case rows, about a dozen steps.  Add one if a benchmark shows it.
- The generator emits a `static_assert` per property bit, so its numbering
  can't drift from `unicode.h`'s.

The tables are about 77 KB of read-only data.  `tests/test_unicode.c`
also covers a digit in a run of 50 (mathematical digits), `Ⅳ` (numeric, not
a digit), `ẞ` folding to `ß`, and a mapping beyond the BMP.

### Phase 3 - The procedures and `(scheme char)`

- `src/char.c`: the 14 primitives.
- `scheme/standard.scm`: `(scheme char)`, and its names in `(minim)`.
  `scheme/prelude.boot` regenerated.

Tests: `tests/repl/chars.scm`: each procedure on ASCII and non-ASCII chars,
`digit-value`'s `#f`, a `-ci` comparison of three chars, a non-char's error;
`(import (scheme char))` in `tests/repl/libraries.scm`.  The 6.6 report
updated.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'chars|libraries|r7rs' && ctest --preset stress -R chars
```

### After the last phase

Sync the docs: `primitives.md` (Chars: the new procedures, the Unicode
tables and their generator), `repl.md` (char names in printing),
`r7rs-small.md` (coverage: `(scheme char)` 14 of 22; the conformance table).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

None.  Settled: the generator is Python for now, revisited once Minim can
read the UCD comfortably; the tables are Unicode 18.0, confirmed when Phase
2 starts.

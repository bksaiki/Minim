# Reader syntax: `|...|` symbols, `#!fold-case`, and case-insensitive syntax

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

The reader (`src/read.c`) and printer (`src/print.c`) miss three parts of
R7RS's lexical syntax (2.1, 2.2, 7.1.1):

| Input | Today | R7RS |
|---|---|---|
| `'\|a b\|` | the symbols `\|a` and `b\|` | the symbol `a b` |
| `'\|H\x65;llo\|` | a symbol with the bars | `Hello` |
| `(write (string->symbol "a b"))` | `a b`, which reads back as two symbols | `\|a b\|` |
| `(write (string->symbol "1"))` | `1`, which reads back as a number | `\|1\|` |
| `#!fold-case ABC` | `bad # syntax` | `abc` |
| `#T`, `#TRUE`, `#U8(1)` | `bad # syntax` | case is insignificant outside identifiers, character names, and escapes |
| `+NAN.0`, `-Inf.0` | symbols | numbers |

`|` isn't a delimiter, though R7RS makes it one.  The Read syntax report
fails 5 tests on these, and Numeric syntax 3.  `#!fold-case` is also what
`include-ci` needs (`cond-expand.md`).

## Design

**`|...|` symbols.**  `|` becomes a delimiter, so `a|b|c` is three symbols.
A `|` starts a symbol that runs to the next unescaped `|`, with the escapes
of strings (`\|`, `\\`, `\a`, `\n`, `\x41;`, ...), interned as is, never a
number.  `read_string`'s escape handling serves both, parameterized by the
closing character.  End of input inside one raises `eof in symbol`.

**Writing symbols.**  `write` puts a symbol in bars when its name wouldn't
read back as it:

- the empty name, or `.`;
- a name that parses as a number (`minim_parse_number`): `|1|`, `|+3|`,
  `|-.4|`;
- a delimiter or whitespace anywhere, or a `|` or `\`;
- `#`, `'`, `` ` ``, or `,` first;
- a control character anywhere.

Inside bars, `|` and `\` are escaped, and control characters as `\xHH;`, as
in strings.  `display` writes names unescaped, as now.

**Case.**  `#!fold-case` and `#!no-fold-case` are directives, read where a
comment may be and followed by a delimiter.  They set or clear folding for
the rest of the port: a new port flag, `MINIM_PORT_FOLD_CASE`; for a C
string source, a flag in `source`, for the rest of that call.  While
folding, identifiers and character names (`#\SPACE`) are folded with
Unicode's full case folding, as `string-foldcase`; character literals
(`#\A`) and strings aren't.

Independently of folding, R7RS's grammar makes case insignificant except in
identifiers, character names, and escapes, so `#T`, `#F`, `#TRUE`,
`#FALSE`, and `#U8(` read, and the number parser accepts `inf.0` and
`nan.0` in any case.

## Phases

### Phase 1 - Reading `|...|` symbols

- `src/read.c`: `|` a delimiter; `|...|` read with the string escapes.

Tests: `tests/test_read.c`: `|a b|`, `||`, `|H\x65;llo|`, `|a\|b|`,
`|a\\b|`, `|1|` a symbol, `|.|`; `a|b|c` as three symbols; `(|a|)`; `eof in
symbol`.  The Read syntax report updated.

```sh
cmake --build --preset dev && ctest --preset dev -R 'read|r7rs'
```

### Phase 2 - Writing symbols readably

- `src/print.c`: the test for bars, and the escapes inside them.

After Phase 1, so every symbol written reads back.

Tests: `tests/test_print.c`: each case of the list above, and plain names
unbarred (`a`, `+`, `...`, `->x`, `λ`); `display` unbarred.  Round trips:
each written symbol reads back `eq?`.  `tests/repl/symbols.scm`: `write` of
`(string->symbol "a b")`.  The Read syntax report updated.

```sh
cmake --build --preset dev && ctest --preset dev -R 'print|read|symbols|r7rs'
```

### Phase 3 - Case

- `include/minim.h`: `MINIM_PORT_FOLD_CASE`.
- `src/read.c`: the directives, folding, and the case-insensitive `#`
  syntax.
- `src/number.c`: `inf.0` and `nan.0` in any case.

Tests: `tests/test_read.c`: `#!fold-case ABC` as `abc`; `#!fold-case
#!no-fold-case ABC` as `ABC`; `#!fold-case #\SPACE` as a space and `#\A` as
`A`; `#!fold-case "ABC"` unchanged; `#!fold-case STRASSE` with `ß` folding;
a directive not followed by a delimiter, and `#!foo`, as errors; folding
lasting across reads from one port; `#T`, `#FALSE`, `#U8(1)`, `+NAN.0`,
`-Inf.0`.  The Read syntax and Numeric syntax reports updated.

```sh
cmake --build --preset dev && ctest --preset dev -R 'read|ports|r7rs'
```

### After the last phase

Sync the docs: `repl.md` (the reader table: `|...|` symbols, directives,
case; the printer's symbols), `ports.md` (the fold-case flag),
`r7rs-small.md` (the conformance table).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

### Bar non-ASCII symbols?

R7RS 6.13.3 says `write` escapes "symbols that contain non-ASCII
characters" with vertical lines, though such names read back unbarred.
Barring them makes `(write 'λ)` print `|λ|`.  Provisional: don't, since
they read back; reopen if a conformance test needs it.

### Fold `|...|` symbols under `#!fold-case`?

R7RS says fold-case folds "identifiers", which a `|...|` symbol is, but the
bars are a way to write a name exactly.  Provisional: not folded, as
strings aren't; reopen if another implementation's behavior or a test says
otherwise.

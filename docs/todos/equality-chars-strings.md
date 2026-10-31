# Equality, chars, and strings: roadmap milestone 1

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

## Context

The primitive library (`src/prims.c`, documented in
`docs/design/primitives.md`) has `eq?` but no structural equality.  It can
test for chars but not compare or convert them, and it can read strings but
not build or compare them:

| Expression | Today |
|---|---|
| `(equal? '(1 2) (list 1 2))` | `error: unbound variable: equal?` |
| `(char->integer #\a)` | `error: unbound variable: char->integer` |
| `(char<? #\a #\b)` | `error: unbound variable: char<?` |
| `(string-append "a" "b")` | `error: unbound variable: string-append` |
| `(string=? "a" "a")` | `error: unbound variable: string=?` |
| `(string-set! (symbol->string 'a) 0 #\b)` | not defined; the C accessor `minim_string_set` would hit an `assert` on the immutable name |

Milestone 2's prelude needs these: `member` and `assoc` use `equal?`, and
string and char procedures are easier to build on these primitives.

**Decisions:**

| Decision | Choice | Reason |
|---|---|---|
| `eqv?` | the same function as `eq?` | they differ only for flonums and bignums, which don't exist until milestone 6 |
| `equal?` | pairs, vectors, and strings compared structurally; everything else by `eqv?` | R7RS |
| `equal?` traversal | an explicit stack in a static C buffer, not C recursion | deeply car-nested data would overflow the C stack; the buffer is static so it can't leak, and `equal?` never allocates on the heap, so raw values in it stay valid |
| Cycles in `equal?` | loop forever | the roadmap defers cycle detection |
| Comparison helper | generalize `compare` in `prims.c` to take a key function (fixnum value or char code) | one helper serves fixnum and char comparisons; strings compare via a three-way `string_compare` checked against 0 with the same predicates |
| `make-string` default fill | `#\space` | R7RS leaves it unspecified; a space prints readably |
| `string-set!` on an immutable string | the error `string-set!: immutable string` | symbol names are immutable; the C accessor only `assert`s |
| `substring` | `(substring s start end)`, all required | as in R7RS |
| Comparison sets | all five for chars and strings (`=?`, `<?`, `>?`, `<=?`, `>=?`) | one line each with the shared helper; matches the fixnum comparisons |
| String literals | mutable; only symbol names are immutable | simplest; revisit with the conformance suite (roadmap milestone 9) |
| Allocation inside string primitives | re-read `minim_string_chars` after every allocation | the pointer is only valid until the next allocation, but `args` itself is safe (the value stack is updated in place) |

Out of scope: case-insensitive comparisons (`char-ci=?`, `string-ci=?`),
character classes and case (`char-alphabetic?`, `char-upcase`), which need
Unicode tables; `string-copy`, `string-fill!`, and the optional `start` /
`end` arguments of `string->list`.

## Phases

### Phase 1 - Equality

- `src/prims.c`:
  - `eqv?`, registered with `eq?`'s function;
  - `equal?`, walking pairs, vectors, and strings with an explicit stack of
    `(a, b)` pairs in a static growable buffer.
- `docs/design/primitives.md`: add both to "Identity and types".

Separate because it is the only phase with a non-trivial algorithm, and
milestone 2 depends on it alone.

Tests:

- `tests/repl/equality.scm` / `.out`: `eqv?` and `equal?` on every kind of
  value; equal and unequal lists, dotted pairs, nested vectors, strings
  (including non-ASCII), and mixed structure; `(equal? 2 "2")`.
- `tests/test_prims.c` (new, also in `stress`): `equal?` on two structures
  nested 1M deep in the car, which would overflow the C stack if it recursed
  (1k under `stress`, where building them collects on every allocation).

```sh
cmake --build --preset dev && ctest --preset dev -R 'equality|prims'
cmake --build --preset stress && ctest --preset stress -R 'equality|prims'
```

### Phase 2 - Chars

- `src/prims.c`:
  - generalize `compare` to take a key function; the fixnum comparisons use a
    fixnum key;
  - `char?`, `char->integer`, `integer->char` (raising
    `integer->char: not a scalar value` outside the Unicode scalar range);
  - `char=?`, `char<?`, `char>?`, `char<=?`, `char>=?`, with a char key.
- `docs/design/primitives.md`: a "Chars" table.

Separate because the `compare` refactor touches existing fixnum comparisons;
the existing `prims` golden test guards them.

Tests (`tests/repl/chars.scm` / `.out` / `.err`): each primitive; ASCII,
non-ASCII, and `#\x10FFFF` round trips through `char->integer` and
`integer->char`; errors for a surrogate, a negative integer, and a non-char
comparison.

```sh
cmake --build --preset dev && ctest --preset dev -R 'chars|prims'
```

### Phase 3 - Strings

- `src/prims.c`:
  - `make-string`, `string-set!` (with the immutability check);
  - `string-append`, `substring`;
  - `string_compare` (three-way, by code point), and `string=?`,
    `string<?`, `string>?`, `string<=?`, `string>=?` on top of it;
  - `string->list`, `list->string`.
- `docs/design/primitives.md`: extend "Strings and symbols".

Separate because these primitives allocate, which the previous two phases
don't, so each needs checking for GC safety.

Tests (`tests/repl/strings.scm` / `.out` / `.err`, also in `stress`):

- each primitive, including the empty string and non-ASCII chars;
- `string-append` with zero, one, and many arguments;
- a round trip through `string->list` and `list->string`;
- errors: `string-set!` on `(symbol->string 'a)`, `substring` out of range
  and with `start > end`, and `list->string` on a non-char and on an improper
  list.

```sh
cmake --build --preset dev && ctest --preset dev -R strings
cmake --build --preset stress && ctest --preset stress -R strings
```

### After the last phase

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
cmake --preset stress && cmake --build --preset stress && ctest --preset stress
```

No linter is configured; the build must be warning-free.

## Open items

None.

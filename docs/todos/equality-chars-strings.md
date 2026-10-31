# Equality, hashing, chars, and strings: roadmap milestone 1

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
| `(equal-hash '(1 2))` | `error: unbound variable: equal-hash` |
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
| Hashing | `string-hash` and `equal-hash`, next to `equal?` in `prims.c` | a hash must agree with its equality (`equal?` values hash equal); defining them together keeps that checkable |
| String hash | FNV-1a over the chars, as `minim_string_hash` shared with the symbol table | the symbol table already uses it |
| `equal-hash` traversal | at most 64 nodes of pairs and vectors | O(1) per call, and terminates on cyclic data, as bounded hashes in Chez and R6RS implementations do |
| `equal-hash` of procedures | a primitive hashes its C function pointer; every closure hashes to one constant | both are stable under the moving collector; closures are `equal?` only when `eq?`, which needs the `eq?`-hash design of milestone 7 to spread well |
| Hash results | non-negative fixnums | usable as indices from Scheme |
| Allocation inside string primitives | re-read `minim_string_chars` after every allocation | the pointer is only valid until the next allocation, but `args` itself is safe (the value stack is updated in place) |

Out of scope: `eq-hash` and `eqv-hash` of heap objects, since hashing by
address must survive the moving collector (roadmap milestone 7);
case-insensitive comparisons (`char-ci=?`, `string-ci=?`),
character classes and case (`char-alphabetic?`, `char-upcase`), which need
Unicode tables; `string-copy`, `string-fill!`, and the optional `start` /
`end` arguments of `string->list`.

## Phases

### Phase 1 - Equality

**Done.**  Details settled during the phase:

- `string_equal` moved from `symbol.c` to `minim.h` as
  `minim_string_equal`, shared by the symbol table and `equal?`.
- `equal?` skips queuing pairs of identical objects, so car-nested lists with
  `'()` tails keep the stack at two entries per level.
- `tests/test_prims.c` calls the primitive's function directly rather than
  through the evaluator.  It looks the primitive up before calling, since
  interning allocates and could move the structures being compared.

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

**Done.**  Details settled during the phase:

- `compare` takes a key function `intptr_t key(minim_obj, const char *msg)`.
  `fixnum_arg` is the fixnum key; a new `char_arg` is the char key.
- `integer->char` raises `integer->char: not a fixnum` for a non-fixnum, and
  `integer->char: not a scalar value` for a surrogate or a value outside
  `[0, 0x10FFFF]`.
- `tests/repl/chars.out` is written with the raw UTF-8 for `#\x10FFFF`,
  which the printer emits as-is since it isn't a control character.

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

**Done.**  Details settled during the phase:

- `bound_arg` checks a position in `[0, max]` for `substring`'s range;
  `index_arg` still checks `[0, len)`.
- String comparisons use their own `compare_strings`, which applies the same
  `eq`/`lt`/... predicates to `string_compare(a, b)` and 0, since strings
  have no single integer key.
- `string-set!` checks the string's type, then mutability, then the index,
  then the char.
- Checked by hand: making `string-append` read an argument's chars before
  allocating makes `repl_strings` fail under `stress` with an ASan
  heap-use-after-free.

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

### Phase 4 - Hashing

- `include/minim.h`: `minim_string_hash`, moved from `symbol.c`'s `hash`.
- `src/prims.c`, next to `equal?`:
  - `string-hash`;
  - `equal-hash`, combining element hashes over at most 64 nodes of pairs
    and vectors, with symbols hashed by name, primitives by function
    pointer, and closures to a constant.
- `docs/design/primitives.md`: both, in "Identity and types" (`equal-hash`)
  and "Strings and symbols" (`string-hash`).

Separate because it builds on Phase 1's `equal?` and Phase 3's strings, and
its invariant (equal values hash equal) is checked against both.

Tests (`tests/repl/hashing.scm` / `.out`):

- `equal?` values hash equal: lists built by `list` and by quoting, nested
  vectors, strings built by `string-append` and by literals, symbols;
- `(string-hash "a")` and `(string-hash "b")` differ;
- results are fixnums (`fixnum?`) and non-negative;
- a cyclic list (built with `set-cdr!`) hashes without hanging.

```sh
cmake --build --preset dev && ctest --preset dev -R hashing
```

### After the last phase

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
cmake --preset stress && cmake --build --preset stress && ctest --preset stress
```

No linter is configured; the build must be warning-free.

## Open items

None.

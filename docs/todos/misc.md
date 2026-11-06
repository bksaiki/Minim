# Misc: five small procedures, and optional arguments by `case-lambda`

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

Five names, from three areas, that no other plan brings: `make-list` and
`list-set!` (`(scheme base)`, lists), `boolean=?` and `symbol=?` (`(scheme
base)`, equality), and `get-environment-variables` (`(scheme
process-context)`, the last of its five).

| Report | Failing | Cause |
|---|---|---|
| 6.3 Booleans | 5 | `boolean=?` |
| 6.4 Lists | 2 | `make-list`, `list-set!` |
| 6.5 Symbols | 4 | `symbol=?` |
| 6.10 Control | 1 | `vector-for-each`'s test uses `make-list` and `list-set!` |
| 6.14 System | 1 error | `get-environment-variables` |

## Design

**Lists, in Scheme** (`scheme/lists.scm`, beside `list-tail`):

- `(make-list k [fill])`: a new list of `k` elements, each `fill`, by default
  `#f`.  R7RS leaves the default unspecified.
- `(list-set! lst k x)`: `(set-car! (list-tail lst k) x)`, so a bad index
  raises as `list-ref`'s does.

**Equality, in C**, through `prims.h`'s `compare`, as the char comparisons
are: `boolean=?` (in `src/equal.c`, beside `boolean?`) and `symbol=?` (in
`src/string.c`, beside `symbol?`), each taking one or more arguments,
raising `not a boolean` or `not a symbol` on any other, and comparing by
identity.

**`get-environment-variables`**, in C (`src/system.c`): an association list
of `(name . value)` strings, decoded from UTF-8, in the environment's order,
split at each entry's first `=`.  The environment comes from a new wrapper,
`minim_environ` (`src/system.h`): POSIX's `environ`, declared there since no
standard header does, or `_get_environ` on MSVC.

## Phases

### Phase 1 - The five procedures

- `scheme/lists.scm`: `make-list`, `list-set!`.
- `src/equal.c`, `src/string.c`: `boolean=?`, `symbol=?`.
- `src/system.c`, `src/system.h`: `minim_environ`,
  `get-environment-variables`.
- `scheme/standard.scm`: the names in `(scheme base)` and `(scheme
  process-context)`.  `scheme/prelude.boot` regenerated.

One phase: each part is a few lines, and none depends on another.

Tests: `tests/repl/lists.scm`: `make-list` with and without a fill, of
length 0; `list-set!` and its bad index's error.  `tests/repl/equality.scm`:
`boolean=?` and `symbol=?` of two and three arguments, and a wrong type's
error.  `tests/scripts/environment-variable.scm`: an entry the test's
environment sets appears in `get-environment-variables`, as a pair of
strings.  The 6.3, 6.4, 6.5, 6.10, and 6.14 reports updated.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'lists|equality|environment|r7rs' && ctest --preset stress -R 'lists|equality'
```

**Done.**  6.3 and 6.5 pass completely (18, 17); 6.4: 61 to 63; 6.10: 29
to 30; 6.14: 5 to 7, its error gone.  `make-list` is a `case-lambda`, so
extra arguments are an arity error, and raises `make-list: bad length` on a
negative length, which would otherwise loop.  `get-environment-variables`
splits each entry at its first `=` after the first char, so Windows'
per-drive entries (`=C:=C:\`) keep their name.  `equality` gained a `.err`
file, since it now has error cases.

### Phase 2 - Optional arguments by `case-lambda`

The prelude's procedures with optional arguments take them by `case-lambda`,
as `make-list` now does, except four that take them through a rest
parameter, so extra arguments are ignored or blamed on an internal
primitive:

| Procedure | File | Today, with an extra argument |
|---|---|---|
| `member`, `assoc` | `scheme/lists.scm` | ignored: `(member 1 '(1) eq? 'x)` is `(1)` |
| `read-bytevector!`, `write-bytevector` | `scheme/ports.scm` | `arity mismatch: #<procedure %write-bytevector>` |

- `scheme/lists.scm`: `member` and `assoc` as `case-lambda`s, the
  two-argument clause passing `equal?`.
- `scheme/ports.scm`: `read-bytevector!` and `write-bytevector` with a clause
  per arity, as `write-string`.  `scheme/prelude.boot` regenerated.

The rest stay: `append`, `map`, `for-each`, the `-map` and `-for-each`
procedures, `error`, and `environment` take any number of arguments.

Separate because it changes existing procedures' errors, not Phase 1's.

Tests: `tests/repl/lists.scm`: `member` and `assoc` with and without
`compare`, and with an extra argument's arity error;
`tests/repl/ports.scm`: `read-bytevector!` and `write-bytevector` with an
extra argument, the error naming them.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'lists|ports|r7rs' && ctest --preset stress -R 'lists|ports'
```

### After the last phase

Sync the docs: `primitives.md` (Equality, System), `prelude.md` (the list
library), `r7rs-small.md` (coverage: `(scheme base)`, `(scheme
process-context)` 5 of 5; the conformance table; milestone 9).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

None.  Settled: `make-list`'s default fill is `#f`.

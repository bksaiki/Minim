# Vectors: `(scheme base)`'s vector procedures

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

`src/vector.c` has `vector`, `make-vector`, `vector-ref`, `vector-set!`,
`vector-length`, `vector->list` (with a range), `list->vector`, and
`vector?`.  Missing from `(scheme base)`: `vector-copy`, `vector-copy!`,
`vector-fill!`, `vector-append`, `string->vector`, `vector->string`,
`vector-map`, and `vector-for-each`.

| Report | Failing | Cause |
|---|---|---|
| 6.8 Vectors | 29 of 30 | the procedures above; the other needs `round` and `acos` |
| 6.10 Control | 4 of 5 | `vector-map`; `vector-for-each`'s test also needs `make-list` and `list-set!` |

Depends on `strings.md`: its `range_args` and `scheme/sequences.scm`.

## Design

**The procedures in C**, in `src/vector.c`, each with an optional range by
`range_args`: `vector-copy`, `vector-copy!` (overlap-safe), `vector-fill!`,
`vector-append` (any number of vectors), `string->vector`, and
`vector->string` (raising on a non-char).

Copies store element by element through `minim_vector_set`, so the write
barrier sees each store.

**`vector-map` and `vector-for-each`** take procedures, so they are Scheme,
in `scheme/sequences.scm` beside the string ones: over one or more vectors,
to the shortest's length (R7RS 6.10).

## Phases

### Phase 1 - The procedures in C

- `src/vector.c`: the six primitives.
- `scheme/standard.scm`: the names.  `scheme/prelude.boot` regenerated.

Tests: `tests/repl/vectors.scm`: each procedure with and without its range,
`vector-copy!` within one vector both ways, `vector-append` of none, one,
and three, a bad range's error, `vector->string` of a non-char.
`tests/test_gc.c`: `vector-copy!` of young elements into an old vector
survives a minor collection.  The 6.8 report updated.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'vectors|gc|r7rs' && ctest --preset stress -R 'vectors|gc'
```

### Phase 2 - `vector-map` and `vector-for-each`

- `scheme/sequences.scm`: the two procedures.
- `scheme/standard.scm`: their names.  `scheme/prelude.boot` regenerated.

Separate because it is Scheme.

Tests: `tests/repl/vectors.scm`: one vector, two of different lengths, an
empty one.  The 6.10 report updated.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'vectors|r7rs'
```

### After the last phase

Sync the docs: `primitives.md` (Vectors), `prelude.md` (`sequences.scm`),
`r7rs-small.md` (coverage: `(scheme base)`; the conformance table).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

None: the procedures are R7RS's, and the design follows `strings.md`.

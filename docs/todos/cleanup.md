# Cleanup: tests, source layout, and technical debt

Implementation plan.  The design is settled; what follows is the phase
breakdown, one phase per commit.

No behavior changes: every phase is a restructuring, verified by the suites.

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

Two reviews, of the C unit tests and of the REPL and script tests, and a look
at the sources.

**Tests.**  Each behavior should be tested once, where it fits; each file
should cover one topic, named for it.  Today:

| Problem | Examples |
|---|---|
| Files named for the implementation, mixing topics | `repl/prims.scm` (numbers, pairs, vectors, strings, output), `repl/expander.scm` (quasiquote, bodies, errors), `test_eval.c` (primitives, prelude, expander) |
| Files that mix topics | `repl/errors.scm` (handlers, C errors, `guard`), `repl/libraries.scm` (top-level environment, then libraries), `repl/records.scm` (internal primitives, then `define-record-type`), `repl/files.scm` (files, environment variables) |
| Duplicates | macro-inserted definitions in `syntax-rules.scm` and `libraries.scm`; `after`s on unhandled errors in `continuations.scm` and twice in `errors.scm`; `(lambda r r)` twice in `test_eval.c`; the value stack after an error three times in `test_eval.c`; error predicates in `test_prims.c` and two REPL files |
| Tests that prove nothing | `repl/tail.scm`: a loop of 10,000, which passes without tail calls; `test_eval`'s `test_tail_calls` checks it properly |
| Collections the stress copies already make | `errors.scm` (21-28) and `records.scm` (30-33) |
| Pinned expansions | `expansion.scm` (13-24) fixes the rewrites of `let` and friends, whose behavior `derived.scm` and `values.scm` test |
| Misnamed | `repl/prelude.scm` tests only `list?`; `repl/symbols.scm` mostly aliases; `test_object.c` tests tagging, not `object.c`; `test_procedure.c` has no module |
| Gaps | `+`, `-`, `*`, `=`, `<` and fixnum overflow have no golden test (only `test_eval`); `if` without else, `(begin)`, `quote` arity, `(5)` at the REPL; a script with a read error; `minim_utf8_getc` on invalid input; `minim_string_to_utf8`; procedure printing in `test_print` |
| Helpers copied across C tests | raise-and-check-message (`test_eval`, `test_read`, `test_stack`, `test_error`); an rtd builder (`test_print`, `test_gc`); `count_predicates` (`test_object`, `test_procedure`) |

**Sources.**  `src/` is flat: the C runtime, the Scheme sources, the boot
generator (a build tool), and the generated boot file side by side.

- `src/prims.c` is 1,109 lines: every primitive, from fixnum arithmetic to
  records and the expander's support.
- `src/prelude.scm` begins "The list library", but also holds continuations,
  exceptions, `guard`'s helper, and the record helpers.
- `src/expander.scm` is 871 lines: the expander, libraries, the derived
  forms, and the standard libraries.
- `include/minim.h`, the public API, declares continuations' layout, which
  only the evaluator uses.

## Design

**Test layout.**  REPL tests by language topic, as R7RS's chapters divide
it; C tests by `src/` module.

| REPL test | Covers |
|---|---|
| `numbers` (new) | fixnum arithmetic and comparison, overflow, `quotient`, `number->string` |
| `lists` | pairs and the list library (absorbs `prelude`, and `prims`' pairs) |
| `vectors` (new) | vectors (from `prims`) |
| `strings`, `chars`, `symbols` | as named (`symbols` keeps `gensym`, `string->symbol`; aliases move) |
| `equality`, `hashing` | as named (`eq?` from `prims`) |
| `core` (new, from `eval`, `echo`) | core forms at the REPL, printing values, recovery after read and eval errors |
| `quasiquote`, `bodies` (new, from `expander`) | quasiquote; internal definitions and body errors |
| `derived`, `values`, `local-macros`, `syntax-rules` | as named; one home each for shadowing a keyword and for macro-inserted definitions |
| `expansion` | `%expand`'s output for what behavior can't show |
| `continuations`, `exceptions`, `guard` (from `errors`) | as named; `after`s on errors, once each |
| `records` | `define-record-type`, with the record primitives' errors it can reach |
| `top-level` (from `libraries`), `libraries` | the interaction environment; libraries |
| `load`, `files`, `gc`, `boot`, `aliases` (from `symbols`) | as named; read and file errors with `load` |

Internals keep REPL tests where a golden output suits them (`boot`,
`expansion`, `aliases`, the record primitives), in files of their own.

| C test | Covers |
|---|---|
| `test_tag` (was `test_object`) | value tagging in `minim.h` |
| `test_object` (was `test_procedure`) | `object.c`'s constructors: closures, primitives, records |
| the rest | as now, one per module |

**Source layout.**

```
include/minim.h        the public API
src/                   the C runtime, one file per module
src/prims/             the primitives, one file per area, each registering its own
src/scheme/            the Scheme sources, in load order, and prelude.boot
tools/                 boot_gen.c and boot.scm, which write prelude.boot
```

`src/scheme/`, in load order: `lists.scm`, `control.scm` (continuations,
exceptions, `guard`'s helper), `records.scm`, `expander.scm`,
`libraries.scm`, `derived.scm`, `standard.scm` (the standard libraries).
Concatenated in that order they are today's two files, so the boot file is
unchanged, byte for byte.

## Phases

### Phase 1 - Test helpers and C tests

- `tests/test.h`: `last_error_is(msg)`, `make_record_type(name, fields)`,
  one complete `count_predicates`; the C tests use them.
- `tests/test_object.c` renamed `test_tag.c`; `test_procedure.c` renamed
  `test_object.c`, gaining a record constructor test.
- `test_eval.c`: `test_parameters` merged into `test_frames`;
  `test_value_stack_restored` dropped, `test_apply_restores_value_stack`
  folded into `test_handled_errors`; `test_map_long_list` moved to
  `test_prims.c`.  `test_prims.c`'s `test_error_kinds` dropped (the REPL
  tests raise real read and file errors) and its header corrected.
- `test_gc.c`: `test_record_barrier` folded into `test_older_to_younger`.
- `test_print.c`: procedure printing, from `test_procedure` and `test_eval`.
- `test_string.c`: `minim_utf8_getc` on invalid and truncated input,
  `minim_string_to_utf8`.

Separate because it is the C half, self-contained.

```sh
cmake --build --preset dev && ctest --preset dev -R 'test|tag|object|eval|prims|gc|print|string|error|read|stack' && ctest --preset stress -j 8 -R 'tag|object|eval|prims|gc|print|string'
```

### Phase 2 - REPL tests: gaps, and moves from C

- `tests/repl/numbers.scm` (new): arithmetic, comparison, overflow, from
  `test_eval.c`'s `test_primitives` and its errors, which go.
- `tests/repl/core.scm` (new): `if` without else, `(begin)`, `quote` arity,
  `(5)`, `set!` of an unbound variable, the core forms' syntax errors, from
  `test_eval.c`'s `test_special_forms`, `test_parameters` (what remains), and
  `test_errors`.
- `tests/scripts/`: a read error mid-file.

Separate because it adds coverage before the REPL files move, so Phase 3
moves only tested behavior.

```sh
cmake --build --preset dev && ctest --preset dev -R 'numbers|core|eval|script' && ctest --preset stress -R 'numbers|core'
```

### Phase 3 - REPL tests: split, merge, rename

- Split `prims` (into `numbers`, `vectors`, `lists`, `strings`, `symbols`,
  `equality`, `core`), `expander` (`quasiquote`, `bodies`), `errors`
  (`exceptions`, `guard`), `libraries` (`top-level`, `libraries`).
- Merge `prelude` into `lists`, `eval` and `echo` into `core`.  Rename the
  alias half of `symbols` to `aliases`.  Delete `tail`.
- Remove the duplicates, the explicit collections the stress copies make,
  and `expansion`'s pinned rewrites.  Move `records`' primitive checks
  behind its `define-record-type` tests, and read and file errors to `load`.

Separate because it is the bulk of the test changes, all moves, with Phase 2
in place.

Tests: every behavior a removed line tested is still tested, by the owner in
the layout above; the `.out` and `.err` files regenerated and diffed against
the old ones line by line.

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
```

### Phase 4 - Scheme sources

- `src/scheme/` with the seven files above, from `src/prelude.scm` and
  `src/expander.scm`, each with an accurate header; `src/prelude.boot` moved
  there.
- `tools/boot_gen.c` (was `src/boot.c`) and `tools/boot.scm`.
- `CMakeLists.txt`, `tests/CMakeLists.txt` (the `boot` test), and
  `tests/repl/boot.scm`'s path to `boot.scm`.

Separate because it moves files only: the regenerated boot file must equal
the old one.

```sh
cmake --preset dev && cmake --build --preset dev --target boot && git diff --exit-code -- src/scheme/prelude.boot && ctest --preset dev -R 'boot|expan|libraries'
```

### Phase 5 - Primitives by area

- `src/prims/`: `number.c`, `char.c`, `pair.c`, `equal.c` (equality and
  hashing), `vector.c`, `string.c` (strings and symbols), `control.c`
  (`apply`, `values`, `call/cc`, winders, handlers, `load`), `error.c`,
  `record.c`, `syntax.c` (aliases, `gensym`, `%syntax-error`), `system.c`
  (files, environment variables, storage).  Each defines its primitives and
  an init; `src/prims.c` keeps the shared argument checks and calls them.
- `minim.h`: continuations' constructor and accessors move to an internal
  header.

Separate because it is the largest C move, with no change in behavior.

```sh
cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
```

### After the last phase

Sync the docs: `testing.md` (the test layout), `prelude.md` and `macros.md`
(the Scheme files and `tools/`), `primitives.md` (`src/prims/`), `README.md`
of `docs/design` if a document's scope changed; the roadmap
(`beyond-r7rs.md`: the reorg done); and the parked plans
(`docs/todos/compile.md`, `docs/todos/interaction.md`) for the new paths and
test names.

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

### Should internals be tested from the REPL?

`boot`, `expansion`, `aliases`, and the record primitives' checks call
`%`-prefixed internals.  A golden file shows their output plainly; C tests
calling primitives through `minim_primitive_fn` are clumsier.  Once internals
are hidden (`docs/todos/interaction.md`), these files import
`(minim system)`.

Provisionally they stay REPL tests, each in its own file.  Reopen if hiding
internals makes them awkward.

### Does `gc.scm` duplicate the stress suite?

The review said so; but it runs on a radix-2 schedule, collecting old
generations far more often than the stress copies' default.

Provisionally it stays, minus nothing.  Reopen if the stress suite gains a
radix-2 run.

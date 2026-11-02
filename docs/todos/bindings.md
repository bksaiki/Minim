# Bindings: top-level bindings on symbols' property lists

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

A top-level environment (`scheme/expander.scm`, `macros.md`) is
`#(kind bindings parent own)`: its bindings an association list of
`(identifier . binding)`, searched for every global identifier the expander
resolves, and rebuilt by `%top-bind!`.

- Today the interaction environment's list is short: a user's definitions,
  then the system environment's macros.
- The interaction work (`docs/todos/interaction.md`, on its own branch) gives
  the REPL about 150 imported names.  There, `bench/expand.scm`, 400 small
  definitions and calls, expanded about 25% slower; binding imports in their
  listed order instead, about 10%.
- A library imports `(scheme base)`, 128 names, into a list the same way.

The cost grows with the environment's size, though a lookup concerns one
name.

## Design

After Chez Scheme (`s/syntax.ss`): an environment's bindings live on the
symbols they bind, not in the environment.  Each symbol has a property list
mapping an environment to what the symbol means there; Chez's
`get-global-substs` reads `($sgetprop symbol token #f)`.  Resolving a name in
an environment reads one entry of that name's list, usually of one or two,
whatever the environment's size.  No table, no hashing.

**Symbols gain a property list**: `[name | value | plist]`, 24 B, rounded to
32.  `plist` is an association list of `(key . value)`, keys compared by
`eq?`.  Setting it goes through the write barrier, as setting a value does.
C API, `minim.h`: `minim_getprop(sym, key, default)`,
`minim_putprop(sym, key, value)`, `minim_remprop(sym, key)`.  Internal
primitives, after Chez's `getprop`, `putprop`, and `remprop`: `(%getprop sym
key default)`, `(%putprop! sym key value)`, `(%remprop! sym key)`.

**Top-level environments** become `#(kind parent own names)`.  An
environment is its own key on the property lists: the binding of `id` in
`top` is `(%getprop id top #f)`.  `names` lists the identifiers it binds, for
what must see them all (`tools/boot.scm`'s count of fresh globals).

- `%top-lookup` reads `id`'s property for `top`, then for its parents.
- `%top-bind!` puts the property, or, for the system's names bound to
  themselves, removes it; and adds `id` to `names` the first time.
- Everything above them, `%resolve`, `%top-define!`, the libraries, is
  unchanged.

A property outlives its environment as long as the symbol does: an interned
symbol is never freed, so a discarded environment's entries stay, unread.

## Phases

### Phase 1 - Symbols' property lists

- `include/minim.h`: `MINIM_SYMBOL_SIZE` 3 words; `minim_getprop`,
  `minim_putprop`, `minim_remprop`.
- `src/symbol.c`: a new symbol's empty list; the three operations.
- `src/gc.c`: a symbol's size, and its pointer fields, now three.

Separate because it changes a heap layout, with nothing using the lists yet.

Tests (`tests/test_symbol.c`): get, put, replace, and remove properties,
keyed by symbols and by vectors; one symbol's properties don't show on
another, interned or uninterned of one name; properties survive collections;
an old symbol's new property (the barrier).  The tests that count live bytes
(`test_gc`, `test_string`) updated for the larger symbols.

```sh
cmake --build --preset dev && ctest --preset dev -R 'symbol|gc|string|heap' && ctest --preset stress -R 'symbol|gc'
```

**Done.**

- `test_gc` and `test_string` count bytes above a baseline taken at the
  start, so the larger symbols needed no change there.
- `minim_putprop` roots `key` as well as `sym` and `value`, because a heap
  key can move while the new entry is allocated.
- Removing the barrier from the plist setter fails `test_props_gc`, so the
  test covers it.
- The whole dev suite passes too.

### Phase 2 - Primitives

- `src/prims/string.c`, with the other symbol primitives: `%getprop`,
  `%putprop!`, `%remprop!`.

Separate because it is the expander's interface, testable at the REPL.

Tests (`tests/repl/symbols.scm`): each primitive, and a symbol that isn't one.

```sh
cmake --build --preset dev && ctest --preset dev -R 'symbols' && ctest --preset stress -R 'symbols'
```

**Done.**

- `%putprop!` and `%remprop!` return void.
- Only the symbol is checked: any object can be a key or a value.

### Phase 3 - Environments on property lists

- `scheme/expander.scm`, `scheme/libraries.scm`: top-level environments as
  `#(kind parent own names)`, their bindings on property lists.
  `scheme/prelude.boot` regenerated twice.
- `tools/boot.scm`: `%boot-labels` from the system environment's `names`.
- `bench/expand.scm` (new): 400 small top-level definitions and calls.

Separate because it changes the expander, on Phases 1 and 2; behavior is
unchanged, so the full suite is its net.

Tests: both full suites.  Measure `bench/run.sh` against `main`, with
aligned builds (`testing.md`): `expand` and startup; again after the
interaction branch is rebased onto it.

```sh
cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
```

### After the last phase

Sync the docs: `gc.md` (the symbol layout), `macros.md` (environments'
bindings on property lists), `primitives.md` (the property primitives), and
the roadmap (`beyond-r7rs.md`: environments' bindings; a weak symbol table
would also drop stale properties).  Update `docs/todos/interaction.md`'s
open item on expansion cost.

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

### Should property lists be public?

Chez's `getprop`, `putprop`, and `remprop` are user procedures, with the
system's on a separate list (`$sgetprop`).  One list, internal, is enough for
environments.

Provisionally internal; tests use the `%` primitives, or `minim_getprop` and
friends from C.  Reopen if a program needs property lists.  Public accessors
could then share the one list: an environment's key is the environment
itself, which user code can't obtain, and with no accessor for a symbol's
whole list, user code can't see or disturb bindings.

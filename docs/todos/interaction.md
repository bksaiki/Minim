# Interaction: hide internal names, and programs

Implementation plan.  The design is settled; what follows is the phase
breakdown, one phase per commit.

This moves toward R7RS-small compliance (`docs/roadmaps/r7rs-small.md`):
user code sees only what libraries export, and a program only what it imports.

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

The REPL and files expand in the interaction environment, whose parent is
the system environment (`macros.md`).  So user code sees every system
binding: 222 names, of which 98 are internal (`%`-prefixed), 113 exported by
standard libraries, and 11 neither.

| Input | Today |
|---|---|
| `(%set-handlers! 5)`, then `(guard (e (#t 0)) (raise 1))` | exception handling broken |
| `%expand` | `#<procedure>`: an internal, reachable |
| `minim prog.scm`, `prog.scm` starting `(import (scheme base))` | also sees every system binding, and the REPL's |

Defining a name that is a system binding already gives it a fresh label, so
user definitions can't replace internals; only references reach them.

The REPL tests use 16 internals (records, aliases, `%boot-form`, handlers,
winders, `%read-file`, `%expand`), in six files.

## Design

After Chez Scheme (CSUG, "Using Chez Scheme"; `s/syntax.ss`): its
interaction environment holds the bindings of `(chezscheme)`, every public
name, standard or not; internals live only in a separate system environment,
`$system`, in which only the system is expanded; and a top-level program,
run with `--program`, sees only what its `import` names.

**`(minim)`**, after `(chezscheme)`: every public system binding, the
standard libraries' names and all of Minim's own: `collect`,
`collect-generation-radix`, `collect-maximum-generation`,
`collect-nursery-bytes`, `equal-hash`, `filter`, `fixnum?`, `fold`,
`fold-right`, `gensym`, `string-hash`, and the core form `letrec-values`.
A system library like the standard ones.

**`(minim system)`**, after `$system`: every system binding, internals
included.  Its exports aren't a list: importing it makes the importing
environment fall back to the system environment, as the interaction
environment does today.  So it can only be imported whole; `only`, `except`,
`prefix`, and `rename` of it are syntax errors.  For tests and for code that
extends Minim.

**The interaction environment** loses its parent and starts with `(minim)`
imported.  An identifier it doesn't bind is its own label, as now, unless
that label is a hidden system binding: then a fresh label, never bound, which
the environment records the first time, as Chez keeps one label per name and
environment.  So referring to `%expand` raises `unbound variable`, and a
reference and a later definition of the same name agree.

- System macros still expand into internals: their template identifiers are
  aliases resolved in the system environment, where the macro was defined.
- C still finds `%expand` and `%raise-c-error` by symbol value.
- A user's definition of a hidden internal's name gets that recorded label.

**Programs**, after Chez's `--program`: `minim --program file.scm` runs an
R7RS program (R7RS 5.1) in its own environment, of the library kind, holding
only what its leading `import` names.  `minim file.scm`, and every file
`load` runs, keeps running in the interaction environment, as a script, like
Chez's default and `--script`.

## Phases

### Phase 1 - (minim) and (minim system)

- `src/expander.scm`: `(minim)` as a system library; `(minim system)`, which
  `%import!` handles by setting the environment's parent, refusing import
  sets around it.  `src/prelude.boot` regenerated.
- `tests/repl/`: the six files that use internals start with
  `(import (minim system))`, a no-op for now.

Separate because it adds the libraries and readies the tests, changing
nothing yet, so Phase 2's effect is visible in the tests alone.

Tests (`tests/repl/libraries.scm`): a library importing `(minim)` uses
`gensym` and `fold`; one importing `(minim system)` uses `%bound?`;
`(only (minim system) car)` is a syntax error.

```sh
cmake --build --preset dev && ctest --preset dev -R 'libraries|boot|records|errors|expan|symbols|files'
```

### Phase 2 - Hide internal names

- `src/expander.scm`: the interaction environment without a parent, its
  imports made at startup; resolution of a hidden system binding to a fresh
  label.  `src/prelude.boot` regenerated.
- `bench/expand.scm` (new): a file of many small definitions and calls,
  for the cost of looking up names in the larger interaction environment.

Separate because it is the change in what user code can see.

Tests (`tests/repl/libraries.scm`): `%expand`, `%set-handlers!`, and
`%record-ref` are unbound at the REPL; `let`, `guard`, `define-record-type`,
and `case` still work; defining `%expand` and calling it calls the user's;
every other REPL test unchanged.  Measure `bench/run.sh` against `main`,
especially `expand` and startup.

```sh
cmake --build --preset dev && ctest --preset dev && ctest --preset stress -R 'libraries|boot|errors|records'
```

### Phase 3 - Programs

- `src/main.c`: `minim --program file.scm`; the usage message.
- `src/expander.scm`: `%run-program`, which reads the file, requires a
  leading `import`, and expands the rest in a new environment of the library
  kind holding its imports.
- `tests/scripts/`: `program.scm` runs with `--program`, and new ones.

Separate because it adds a way to run a file, on Phase 2.

Tests (`tests/scripts/`): a program sees only what it imports (`car`
without `(scheme base)` is unbound); a program without a leading `import` is
an error; its definitions don't reach the REPL's environment; a file run
without `--program` runs as before (`hello.scm`).

```sh
cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
```

### After the last phase

Sync the docs: `macros.md` (the interaction environment's imports, hidden
names, `(minim)`, `(minim system)`, programs), `repl.md` (what the REPL and
files see; `--program`), `prelude.md` (the libraries), `testing.md` (tests
import `(minim system)` for internals), and the roadmap (`r7rs-small.md`:
programs; the coverage table unchanged).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

### Does the larger interaction environment slow expansion?

Its association list grows from the user's definitions to about 130
imported names, walked for every identifier at the REPL and in loaded
files.

Provisionally an association list; Phase 2 measures it.  Reopen if
`bench/expand.scm` slows by more than about 10%; then a hash table.

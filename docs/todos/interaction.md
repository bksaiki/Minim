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

The REPL tests use 13 internals (records, aliases, `%boot-form`,
`%read-file`, `%expand`), in five files: `aliases`, `boot`, `expansion`,
`files`, and `records`.

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

**The interaction environment** binds exactly its imports and its own
definitions, as most implementations' top levels do (Guile's modules,
Racket's namespaces, Gauche's modules, Chez's environments): it loses its
parent, starts with `(minim)` imported, and never falls back to symbol
values.  An identifier it doesn't bind gets the environment's own label, a
fresh symbol named like it, made the first time the environment sees it and
recorded, as Chez keeps one label per name and environment.  A definition of
it uses that label.  So:

- `%expand` is in no library the REPL imports, so it gets the REPL's own
  label, which is unbound: internals are unreachable without any rule about
  their names.
- A reference and a later definition of the same name agree, at the REPL and
  across loaded files.
- A user's definition no longer lives in the interned symbol of its name:
  `(define foo 1)` binds the REPL's `foo`, leaving the interned `foo` unbound.
  Nothing in C reads a user's global that way today.
- System macros still expand into internals: their template identifiers are
  aliases resolved in the system environment, where the macro was defined.
- C still finds `%expand` and `%raise-c-error` by symbol value: they are the
  system's.
- A library's environment records its unknown names' labels the same way,
  instead of making a new one at each reference.

**`minim_define(name, value)`**, in `minim.h`: defines `name` in the
interaction environment, as `scm_c_define` does in Guile's current module and
`define-top-level-value` in Chez.  It is how C gives user code a global, now
that setting a symbol's value no longer reaches the REPL; it evaluates
`(define name 'value)`, so the expander picks the label.

**Programs**, after Chez's `--program`: `minim --program file.scm` runs an
R7RS program (R7RS 5.1) in its own environment, of the library kind, holding
only what its leading `import` names.  `minim file.scm`, and every file
`load` runs, keeps running in the interaction environment, as a script, like
Chez's default and `--script`.

## Phases

### Phase 1 - (minim) and (minim system)

- `scheme/standard.scm`: `(minim)` as a system library;
  `scheme/libraries.scm`: `(minim system)`, which
  `%import!` handles by setting the environment's parent, refusing import
  sets around it.  `scheme/prelude.boot` regenerated.
- `include/minim.h`, `src/eval.c`: `minim_define`.  `test_eval`'s
  `stack-height` and `collect-then-second` are defined with it.
- `tests/repl/`: the five files that use internals start with
  `(import (minim system))`, a no-op for now.

Separate because it adds the libraries, the C call, and readies the tests,
changing nothing yet, so Phase 2's effect is visible in the tests alone.

Tests (`tests/repl/libraries.scm`): a library importing `(minim)` uses
`gensym` and `fold`; one importing `(minim system)` uses `%bound?`;
`(only (minim system) car)` is a syntax error.  `test_eval`: a value defined
with `minim_define` is visible to evaluated code, and redefining it replaces
it.

```sh
cmake --build --preset dev && ctest --preset dev -R 'libraries|boot|records|expan|aliases|files'
```

**Done.**
- `(minim)` is defined last in `scheme/standard.scm`, from the names every
  library defined before it exports (`%exported-names`), plus Minim's own.
- `%import!` turns `(minim system)` into the system environment as the
  parent; `%import-set` refuses it inside an import set.  An unknown name now
  gets a fresh label only when the environment's parents end in a library
  (`%root-kind`), so a library importing `(minim system)` resolves internals
  as the REPL does.
- `minim_define` (`src/eval.c`) evaluates `(%interaction-label! 'name)`
  unexpanded, so the expander picks the label, and sets its value; before
  the expander is loaded, the label is the name.  `%interaction-label!` is
  `%top-define!` on the interaction environment.
- The five files' imports change nothing yet: their output is as before.
- Ran `ctest -R 'libraries|boot|records|expan|aliases|files|eval'`, and
  stress `libraries|boot|records|aliases|eval`.

### Phase 2 - Hide internal names

- `scheme/expander.scm`: the interaction environment without a parent, its
  imports made at startup; an unknown name's label made once and recorded,
  in the interaction environment and in libraries; a definition takes it.
  `scheme/prelude.boot` regenerated.
- `bench/expand.scm` (new): a file of many small definitions and calls,
  for the cost of looking up names in the larger interaction environment.

Separate because it is the change in what user code can see.

Tests (`tests/repl/top-level.scm`): `%expand`, `%set-handlers!`, and
`%record-ref` are unbound at the REPL; `let`, `guard`, `define-record-type`,
and `case` still work; defining `%expand` and calling it calls the user's; a
procedure referring to a name defined after it.  `test_eval`: after the REPL
defines `foo`, the interned `foo` is unbound; `minim_define`'s globals still
reach evaluated code.  Every other test unchanged.  Measure `bench/run.sh`
against `main`, especially `expand` and startup.

```sh
cmake --build --preset dev && ctest --preset dev && ctest --preset stress -R 'libraries|boot|exceptions|records'
```

**Done.**
- The interaction environment is made without a parent and imports `(minim)`
  at the end of `scheme/standard.scm`.  An unknown name gets a fresh label,
  kept as the environment's own (`%top-own!`), wherever the environment's
  parents don't end in the system's.
- `import` and `define-library` are in `(minim)`: they were recognized as
  themselves through the system fallback.  `tools/boot.scm` imports
  `(minim system)`.
- A macro-inserted definition reuses an own label only if it is bound: an
  unknown name's label, recorded when the alias's original was resolved, is
  not the user's variable.
- A library body using `define` without importing it now fails with
  `unbound variable: define` when it runs, not `define-library: not
  defined`: the unknown names get labels, so the export is found.
- Bootstrapping: the first regeneration hid internals from `boot.scm` before
  it imported `(minim system)`; recovered from the committed boot file.
- `(minim)`'s names come from the R7RS libraries other than `(scheme r5rs)`,
  which don't overlap.
- With association lists, `bench/expand.scm` was about 25% slower than
  Phase 1, since every identifier the REPL expands walked about 150 imported
  names.  So this branch was rebased onto property lists (#89):
  environments' parent moved to slot 1, and `tests/repl/symbols.scm`, whose
  property-list tests use internals, imports `(minim system)`.
- Measured after the rebase, aligned builds against `main`, 11 runs, median:
  `bench/expand.scm` 0.53 s to 0.42 s, startup 4 ms to 5 ms, runtime
  benchmarks unchanged.
- Ran both full suites.

### Phase 3 - Programs

- `src/main.c`: `minim --program file.scm`; the usage message.
- `scheme/libraries.scm`: `%run-program`, which reads the file, requires a
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

**Done.**

- `%expand-program` (`scheme/libraries.scm`) returns the program's core
  forms, and `main.c` evaluates them with `minim_eval_expanded`.  The call
  is a core form, so `%expand-program` stays hidden from the REPL.
- Added at the user's request: a fuller command line, with `--help` and
  `--version`, and usage errors for an unknown option, a missing file after
  `--program`, and an extra argument.  The version, 0.1.0, is set once, in
  `CMakeLists.txt`'s `project`, and passed to `main.c` as `MINIM_VERSION`.
- Tests: `program` now runs with `--program`; `program-unbound`,
  `program-no-import`, `program-own` (defines `car`, which it didn't
  import); `help`, `version`, `usage-option`, `usage-program`.  "Its
  definitions don't reach the REPL" can't be observed from the command line,
  since no REPL follows a program; `program-own` covers its environment
  being its own.
- Ran both full suites.

### After the last phase

Sync the docs: `macros.md` (the interaction environment's imports, its own
names, `(minim)`, `(minim system)`, programs), `repl.md` (what the REPL and
files see; the command line: `--program`, `--help`, `--version`),
`prelude.md` (the libraries), `testing.md` (tests
import `(minim system)` for internals), and the roadmap (`r7rs-small.md`:
programs; the coverage table unchanged).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

### Does the larger interaction environment slow expansion?

No: its bindings are on symbols' property lists (`macros.md`), so a lookup
doesn't depend on its size.  `bench/expand.scm` is about 20% faster than on
`main` (Phase 2).

### Should C be able to read a user's global?

With the REPL's definitions in labels of their own, `minim_symbol_value` of
the interned name no longer finds them.  Guile has `scm_c_lookup`, Gauche
`Scm_GlobalVariableRef`.

Provisionally no such call until something needs it.  Reopen with the first
embedding that reads user globals.

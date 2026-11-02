# Libraries: top-level environments, define-library, and import

Implementation plan.  The design is settled; what follows is the phase
breakdown, one phase per commit.

This is roadmap milestone 5.

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

Minim has one implicit top-level environment, the expander's
(`src/expander.scm`, `macros.md`).  A syntactic environment is a list of
local frames ending in `'()`; past them, `%resolve` tries `%top-renames`
(fresh globals for macro-inserted definitions), then an alias's original, and
otherwise takes the name itself as the global.  Macros are one global alist,
`%macros`.  Globals are symbol value slots.

| Input | Today |
|---|---|
| `(import (scheme base))` | `unbound variable: import` |
| `(define-library (foo) (export f) (begin (define (f) 1)))` | `unbound variable: define-library` |
| `(define (car x) 0)`, then `(let ((a 1)) a)` | the prelude and expander call the user's `car`; expansion breaks |
| `(set! map 5)` | allowed, and breaks the expander |

## Design

After Chez Scheme (`s/syntax.ss`): an environment maps each name to a
*label*, a symbol whose value slot is the variable's location, and the
expander rewrites each reference to its label, so the runtime sees only
symbols and a global reference stays one load.

**Top-level environments.**  A syntactic environment's chain of local frames
ends in a top-level environment instead of `'()`.  It maps names to bindings,
a label or a macro transformer, in an association list; expansion isn't hot,
so a hash table waits for milestone 8 or a measurement.  `%top-renames` and
`%macros` become its entries.  Aliases carry their environment already, so a
macro's free identifiers resolve where the macro was defined, in its library.

- The *system environment*: every name is its own label, and every binding
  is mutable.  The prelude and expander are expanded in it, so the boot file
  is unchanged.
- The *interaction environment*, for the REPL and `minim file.scm`: a name
  with no entry is the system's, its own label, so every built-in is visible
  before any `import`, as today.  `define` of a name that is a system binding
  gives it a fresh label, so the prelude and expander keep the original; a
  name defined by the user is recorded, so redefining it reuses its label.
  `set!` of a system binding is a syntax error: imported bindings are
  immutable (R7RS 5.6.1).
- A *library environment*: only its imports and its own definitions; an
  unknown name is a syntax error.

**`(define-library name decl ...)`**, with `export` (including `rename`),
`import`, and `begin`, and later `include`.  The library's definitions get
fresh labels, and its body is scanned for definitions before any expression
is expanded, so a definition may refer to one further down.  It expands to
the definitions and expressions, run once, in order, when the form runs; its
exports go into a registry keyed by the name, `(foo bar)`, compared with
`equal?`.

**`(import set ...)`** at the top level of the REPL, a file, or a library,
with `only`, `except`, `prefix`, and `rename`.  Importing a name binds it, in
the importer, to the exporting library's binding: the same label, so the same
location, or the same transformer.

**Standard libraries**, `(scheme base)`, `(scheme char)`, `(scheme cxr)`,
`(scheme write)`, `(scheme load)`, and others Minim has names for: export
lists of system bindings, each its own label.

**Library files.**  Importing an unknown library `(foo bar)` loads
`foo/bar.sld` from the current directory, then from each directory in
`MINIM_LIBRARY_PATH`, colon-separated, then requires that it defined the
library.

## Phases

### Phase 1 - Procedures for loading libraries

- `src/prims.c`: `(file-exists? path)` (R7RS `(scheme file)`),
  `(get-environment-variable name)` (`(scheme process-context)`), and
  `(%read-file path)`, the forms of a file, for `include`.

Separate because they are ordinary procedures, useful alone; Phase 5 needs
them.

Tests: `tests/repl/prims.scm` or a new `files.scm`: each on an existing and
a missing file or variable, and their errors; `tests/scripts/` for a variable
set by the test's environment.

```sh
cmake --build --preset dev && ctest --preset dev -R 'prims|files|script'
```

### Phase 2 - Top-level environments

- `src/expander.scm`: the environment object; `%resolve`, `%macro`, and the
  top-level definitions through it; `%top-renames` and `%macros` gone; the
  system and interaction environments; `set!` of a system binding in the
  interaction environment is a syntax error.
- `src/boot.scm`: boot forms expand in the system environment;
  `%boot-form`'s check for fresh globals reads its entries.  The evaluator's
  `%expand`, after startup, expands in the interaction environment.
- `src/prelude.boot` regenerated twice.

Separate because it is the refactor the rest builds on, and it fixes the
`car` row of the table with no new syntax.

Tests: every existing expander, macro, and REPL test unchanged (the net);
`tests/repl/libraries.scm` (new): redefining `car` and `map`, then `let`,
`cond`, and `map` still work and the user's `car` is called; redefining a
user's own definition keeps closures over it current; `(set! car 5)` is a
syntax error; a macro-inserted definition still gets a fresh global.

```sh
cmake --build --preset dev && ctest --preset dev -R 'boot|expan|macro|syntax|derived|libraries|repl' && ctest --preset stress -R 'libraries|expan'
```

### Phase 3 - define-library and import

- `src/expander.scm`: `define-library` with `export`, `import`, and `begin`;
  the library registry; `import` with `only`, `except`, `prefix`, and
  `rename`, in the interaction environment and in libraries.
- `src/prelude.boot` regenerated twice.

Separate because it is the new syntax, on Phase 2's environments, with
libraries defined in the same session.

Tests (`tests/repl/libraries.scm`): a library exporting procedures, a
variable, and a macro, imported and used; each import set; export `rename`;
a macro from a library inserting a reference to the library's private
definition (hygiene across libraries); a definition referring to a later
one; a library importing another; importing an undefined library, an
unexported name, an unknown name in a library body (syntax errors).

```sh
cmake --build --preset dev && ctest --preset dev -R 'libraries|expan|boot' && ctest --preset stress -R 'libraries'
```

### Phase 4 - Standard libraries

- `src/expander.scm`: `(scheme base)`, `(scheme char)`, `(scheme cxr)`,
  `(scheme write)`, `(scheme load)`, `(scheme file)`,
  `(scheme process-context)`, each the names Minim has.
- `src/prelude.boot` regenerated.

Separate because it is data, reviewed against R7RS's lists.

Tests (`tests/repl/libraries.scm`): a library importing `(scheme base)` and
using a procedure and a derived form; one importing only `(scheme write)`
can't see `car`; `(import (scheme base))` at the REPL changes nothing;
`tests/scripts/`: an R7RS program starting with `import`.

```sh
cmake --build --preset dev && ctest --preset dev -R 'libraries|script|boot' && ctest --preset stress -R 'libraries'
```

### Phase 5 - Library files and include

- `src/expander.scm`: importing an unknown library searches for its `.sld`
  file and loads it; `include` in `define-library`, through `%read-file`.
- `tests/repl/lib/`: library files for the tests.

Separate because it touches the file system and the environment variable,
on Phases 1 and 3.

Tests (`tests/repl/libraries.scm`, `tests/scripts/`): importing a library
from a file, then again (loaded once); one library file importing another;
a library on `MINIM_LIBRARY_PATH`; `include`; a missing library's error.

```sh
cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
```

### After the last phase

Sync the docs: `macros.md` (top-level environments, labels, the three kinds,
`define-library`, `import`, library files), `prelude.md` (the standard
libraries), `primitives.md` (Phase 1's procedures), `environment.md`
(`MINIM_LIBRARY_PATH`), `repl.md` (the interaction environment), and the
roadmap (milestone 5 done; the Limitations entry in `macros.md` about
redefining globals resolved).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

### Does the interaction environment's association list grow too long?

Every user definition is recorded, so redefinitions reuse their labels, and
a long session or a large file makes every lookup of a built-in walk them
all.

Provisionally an association list.  Reopen if loading a large file slows
measurably; then a hash table (milestone 8).

### What does importing a name the user already defined do?

R7RS leaves it to the implementation at the REPL.  Rebinding it to the
import replaces the user's binding for later forms, and closures already
made keep the old label.

Provisionally the import wins.  Reopen if it surprises in practice.

### Which library declarations wait?

`cond-expand`, `include-ci`, and `include-library-declarations` are R7RS
library declarations too.  `cond-expand` needs a feature list.

Provisionally none of them.  Reopen when a library Minim should load uses
one.

# Prelude and running files: roadmap milestone 2

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

Everything in Minim's library is a C primitive (`src/prims.c`), and the only
way to run code is to type or pipe it into the REPL (`src/main.c`):

| Input | Today |
|---|---|
| `minim script.scm` | ignores the argument and starts the REPL |
| `(load "lib.scm")` | `error: unbound variable: load` |
| `(map car '((1) (2)))` | `error: unbound variable: map` |
| `(append '(1) '(2))` | `error: unbound variable: append` |

Higher-order procedures like `map` can't be primitives: a primitive is C
code that can't call a closure (see `docs/design/primitives.md`).  So this
milestone adds a **prelude**, a Scheme library compiled into the binary and
evaluated by `minim_init`, plus `load` and running files.

The reader only reads from a `FILE *`, so evaluating an embedded prelude also
needs the reader to read from a string.

**Decisions:**

| Decision | Choice | Reason |
|---|---|---|
| Embedding | `src/prelude.scm` → a generated `prelude_scm.c` holding `const unsigned char minim_prelude_scm[]`, produced at build time by `cmake/embed.cmake` | rebuilds when the `.scm` changes; a byte array avoids MSVC's string-literal length limit; C17 has no `#embed` |
| Loading the prelude | `minim_prelude_init`, called last by `minim_init`, reads and evaluates each form | a prelude error is a bug in Minim, so it aborts with the message (no catch installed) |
| Reading from a string | the reader reads from an internal source, either a `FILE *` or a string; `minim_read(FILE *)` keeps its signature | needed for the prelude; ports (milestone 5) later replace the source |
| Prelude language | only the core forms (`quote`, `if`, `define`, `set!`, `lambda`, `begin`) | derived forms and macros arrive in milestone 3; the prelude can be rewritten with them then |
| `load` | handled by the evaluator, like `apply`: it reads the whole file into a list of forms, closes it, then evaluates them as a body in the global environment | no re-entry into `minim_eval` from C, so `load` in tail position is a tail call and `call/cc` (milestone 4) won't cross a C frame |
| `load` result | void | as in Chez; the REPL prints nothing |
| `load` paths | relative to the current directory | simplest; R7RS leaves it to the implementation |
| `minim file.scm` | evaluates `(load "file.scm")`; prints nothing; on an error, prints it and exits with status 1 | the evaluator's `load` is the only file-running code |
| Library scope | the R7RS list procedures plus SRFI-1's `filter`, `fold`, and `fold-right` | a few lines each; matches what Scheme programs expect |
| `fold` | SRFI-1: `(kons elem acc)` | the most common in Scheme code |
| Script output | top-level values aren't printed | like `chez --script` and `python file.py`; piping into the REPL prints them |

## The prelude library

| Procedure | Notes |
|---|---|
| `(list? x)` | whether `x` is a proper list |
| `(append lst ...)` | the last argument is shared, not copied |
| `(reverse lst)` | |
| `(list-tail lst k)`, `(list-ref lst k)` | |
| `(list-copy lst)` | |
| `(memq x lst)`, `(memv x lst)`, `(member x lst [compare])` | `member` defaults to `equal?` |
| `(assq x alist)`, `(assv x alist)`, `(assoc x alist [compare])` | `assoc` defaults to `equal?` |
| `(map f lst ...)`, `(for-each f lst ...)` | one or more lists; stop at the shortest |
| `(filter pred lst)` | |
| `(fold kons knil lst)`, `(fold-right kons knil lst)` | SRFI-1 argument order: `(kons elem acc)` |

Errors inside these come from the primitives they call, so `(map car 5)`
reports `car: not a pair: 5` (or `cdr`), not `map`.  Prelude procedures print
as `#<procedure>`, since closures have no names.

## Phases

### Phase 1 - Reading from strings

**Done.**  Diverged from the plan:

- `src/char.c` is unchanged.  A string source decodes with the existing
  `minim_utf8_decode`; only a file source needs `minim_utf8_getc`.
- Pushing back into a string steps its position back one byte, which is
  right because only ASCII is ever pushed back.
- On a read error, `minim_read_string` leaves `*s` unchanged (checked by
  hand).
- `tests/test_read.c` became an `INTERNAL` test, to include `src/read.h`.
- `docs/design/repl.md` mentions the source and `minim_read_string`.

- `src/read.c`: an internal `source` (a `FILE *` or a string with a
  position), with `getc` and `ungetc` operations; every reader function takes
  a `source *` instead of a `FILE *`.  `minim_read(FILE *)` wraps a file
  source.
- `src/char.c`: decoding UTF-8 from a source rather than only from a
  `FILE *`.
- `src/read.h` (new, internal): `minim_read_string(const char **s)`, which
  reads the next datum from `*s` and advances it, returning `MINIM_EOF` at the
  end.

Separate because it refactors the tested reader with no behavior change; the
existing reader tests guard it.

Tests (`tests/test_read.c`): every existing `reads_as` and `fails_with` case
also runs through `minim_read_string`, plus reading several data in turn from
one string.

```sh
cmake --build --preset dev && ctest --preset dev -R read
```

### Phase 2 - Embedding the prelude

- `cmake/embed.cmake`: reads a file as hex and writes a C array of its
  bytes, NUL-terminated.
- `CMakeLists.txt`: an `add_custom_command` generating
  `${CMAKE_CURRENT_BINARY_DIR}/prelude_scm.c` from `src/prelude.scm`, added to
  `libminim`.
- `src/prelude.h`, `src/prelude.c`: `minim_prelude_init`.
- `src/object.c`: `minim_init` calls it last.
- `src/prelude.scm`: only `list?`, to exercise the pipeline.
- `docs/design/prelude.md` (new, linked from `docs/design/README.md`): how the
  prelude is built and loaded.

Separate because it is build infrastructure, reviewable on its own before the
library grows on top of it.

Tests:

- `tests/repl/prelude.scm`: `list?` on proper, improper, and empty lists.
- By hand: editing `src/prelude.scm` triggers a rebuild of `libminim`.

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev -R prelude
```

### Phase 3 - List library

- `src/prelude.scm`: the procedures in the table above.
- `docs/design/prelude.md`: the library table.

Separate because it is pure Scheme on top of Phase 2's mechanism.

Tests (`tests/repl/lists.scm` / `.out` / `.err`, also in `stress`):

- each procedure, including empty lists, `map` and `for-each` over several
  lists of different lengths, `member` and `assoc` with a custom `compare`,
  and `fold` versus `fold-right` order;
- `map` over a 100k-element list (no C stack involved; checks the heap
  stays sane);
- errors: `list-ref` past the end, `map` of a non-procedure.

```sh
cmake --build --preset dev && ctest --preset dev -R lists
cmake --build --preset stress && ctest --preset stress -R lists
```

### Phase 4 - `load`

- `src/read.c`, `src/read.h`: `minim_read_file(const char *path)` returns the
  file's forms as a list.  It reads inside a `minim_catch`, so the file is
  closed before a read error is re-raised.  A missing file raises
  `load: cannot open file`.
- `src/prims.c`, `src/prims.h`: `load` registered like `apply`, with a
  never-called `minim_prim_load`.
- `src/eval.c`: in the apply step, `load` checks its argument is a string,
  reads the forms, pops the call, and evaluates the forms followed by void as
  a body with `env = '()`.
- `tests/run_repl.cmake`: runs `minim` from `tests/repl/`, so tests can load
  fixtures by relative path.
- `docs/design/eval.md`: `load` next to `apply`.

Separate because it touches the evaluator's apply step.

Tests (`tests/repl/load.scm` / `.out` / `.err`, with fixtures in
`tests/repl/load/`):

- a loaded file's definitions are visible afterward, and `load` returns void;
- a loaded file can load another;
- errors: a missing file, a non-string argument, a read error in the file,
  and an evaluation error in the file; the REPL continues after each.

```sh
cmake --build --preset dev && ctest --preset dev -R 'load|repl'
```

### Phase 5 - Running files

- `src/main.c`: with one argument, evaluates `(load "<path>")` inside a
  `minim_catch`; on an error, prints it to stderr and exits with status 1.
  With no arguments, the REPL as before.  More than one argument prints a
  usage message and exits with status 2.
- `tests/run_repl.cmake`: optional `ARGS` (passed to `minim`, with no stdin)
  and `STATUS` (the expected exit status, default 0).
- `tests/CMakeLists.txt`: `minim_add_script_test(name [STATUS n])`.
- `README.md`: `minim file.scm`.

Separate because it changes the program's command line, which is
user-facing.

Tests (`tests/scripts/`):

- `hello.scm`: output with `display` and `newline`, exit 0;
- `fail.scm`: output, then an error; checks stdout, stderr, and exit 1;
- two arguments: usage message, exit 2.

```sh
cmake --build --preset dev && ctest --preset dev -R script
```

### After the last phase

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
cmake --preset stress && cmake --build --preset stress && ctest --preset stress
```

The CI build treats warnings as errors.

## Open items

None.

# Cond-expand: `features`, `cond-expand`, and `include`

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

`(scheme base)` lacks four names, all for writing portable code:

| Form | Today | R7RS |
|---|---|---|
| `(features)` | unbound | the feature identifiers `cond-expand` treats as true (6.14) |
| `(cond-expand (r7rs 'yes) (else 'no))` | unbound | `'yes`: the body of the first clause whose requirement holds (4.2.1) |
| `(include "f.scm")`, `(include-ci "f.scm")` | unbound | the file's forms in a `begin`; `-ci` reads as if after `#!fold-case` (4.1.7) |

`define-library` (`scheme/libraries.scm`, `%expand-define-library`) takes
`export`, `import`, `begin`, and `include` declarations; R7RS 5.6.1 adds
`include-ci`, `include-library-declarations`, and `cond-expand`, which
raise `bad syntax` today.  The 6.14 report fails 2 tests on `features`.

Every macro today is `syntax-rules`.  A macro's binding is its transformer,
a procedure of the form and its environment (`%macro`), so a transformer
written in Scheme can be bound directly with `%top-bind!`; `cond-expand`
and `include` need that, since they consult the features and read files.

`include` in a library reads files relative to the library file's directory
(`%library-dir`, `%include`), or to the current directory outside one.

## Design

**`features`** is a C primitive in `src/system.c` returning a fresh list,
chosen at compile time from R7RS Appendix B and the platform:

- `r7rs`, `ieee-float`, `full-unicode`;
- the OS: `posix` and `unix` off Windows, `windows` on it, and `gnu-linux`,
  `darwin`, or `freebsd` and `bsd` when the compiler says so;
- the CPU: `x86-64` or `aarch64`;
- `lp64` or `ilp32`, and `little-endian` or `big-endian`;
- `minim`, the implementation's name, without a version.

Not `exact-closed` or `ratios`: without bignums and rationals, exact
operations raise where those features promise exact results.

**`cond-expand`** is a transformer in `scheme/libraries.scm`, bound in the
system environment.  A requirement holds by R7RS's rules: an identifier if
`(features)` has it; `(library name)` if the library is defined or its file
is found on the library path (`%library-file-path`, factored out of
`%load-library`, which doesn't load it); `and`, `or`, `not` as usual.  The
first clause that holds, or `else`, expands to `(begin body ...)`, its
`begin` an alias to the system's, so it splices definitions at top level
and at the start of a body.  The same clause choice serves the library
declaration, whose chosen declarations are spliced in place of it.  A
malformed requirement is `bad syntax`.

**`include` and `include-ci`** are transformers too: `(begin form ...)` of
the files' forms, read by `%include`, so relative to the library file's
directory inside one, else the current directory.  `include-ci` reads with
fold-case set from the start: `%read-file` takes an optional second
argument, whether to fold, which sets `MINIM_PORT_FOLD_CASE`
(`minim_port_set_fold_case`, `ports.md`).  As library declarations, `include-ci` joins
`include`, and `include-library-declarations` splices the files' forms as
declarations.

## Phases

### Phase 1 - `features`

- `src/system.c`: `features`.
- `scheme/standard.scm`: `features` in `(scheme base)`.
  `scheme/prelude.boot` regenerated.

First, because `cond-expand` reads it.

Tests: `tests/repl/cond-expand.scm` (new): `(features)` a list of symbols,
holding `r7rs`, `minim`, and one of `little-endian` and `big-endian`; two
calls not `eq?`.  The 6.14 report updated.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'cond-expand|r7rs'
```

**Done.**  `features` is a static table of names, the platform's chosen
by the preprocessor, interned into a fresh list per call.  `gnu-linux`
tests `__gnu_linux__`, not `__linux__`, so Android doesn't claim it; the
size model is `lp64` by `__LP64__`/`_LP64`, `ilp32` for 32-bit pointers,
and none on 64-bit Windows, which is LLP64, outside Appendix B's list;
the byte order by `__BYTE_ORDER__`, else little-endian, as MSVC's targets
are.  On this Linux machine: `(r7rs ieee-float full-unicode posix unix
gnu-linux x86-64 lp64 little-endian minim)`.  The 6.14 report: 13 passed,
0 failed.

### Phase 2 - `cond-expand`

- `scheme/libraries.scm`: the requirement test, the clause choice, the
  transformer, `%library-file-path`, and the declaration in
  `%expand-define-library`.
- `scheme/standard.scm`: `cond-expand` in `(scheme base)`.
  `scheme/prelude.boot` regenerated.

Tests: `tests/repl/cond-expand.scm`: an identifier, `and`, `or`, `not`;
`(library (scheme base))` true and `(library (no such))` false, without
loading anything; `else`; the first match winning; definitions spliced at
top level and in a `lambda` body; a shadowed `begin` not capturing; a
`define-library` choosing an `import` by feature; a malformed requirement;
no clause matching.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'cond-expand|libraries|r7rs'
```

### Phase 3 - `include`, `include-ci`, and `include-library-declarations`

- `src/read.c`, `src/system.c`: `%read-file`'s fold argument.
- `scheme/libraries.scm`: the `include` and `include-ci` transformers; the
  `include-ci` and `include-library-declarations` declarations.
- `scheme/standard.scm`: `include`, `include-ci` in `(scheme base)`.
  `scheme/prelude.boot` regenerated.

Builds on the reader's `MINIM_PORT_FOLD_CASE`, already in place.

Tests: files under `tests/repl/lib/`.  `tests/repl/cond-expand.scm`:
`include` at top level and in a body, with definitions; `include-ci` of a
file in upper case; a missing file.  `tests/repl/libraries.scm`: a library
with `include-ci` and with `include-library-declarations` of an
`export`/`import` file, relative to its directory.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'cond-expand|libraries|r7rs'
```

### After the last phase

Sync the docs: `macros.md` (transformers written in Scheme; `cond-expand`
and `include`), `primitives.md` (`features`), `prelude.md` (what
`libraries.scm` holds), `r7rs-small.md` (coverage: `(scheme base)`; the
conformance table).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

None; see Settled in review.

## Settled in review

- No clause matching, with no `else`: raises `cond-expand: no matching
  clause` with the form, so a missing branch shows at once rather than
  silently losing definitions.  R7RS leaves it unspecified.
- A top-level `include` resolves against the current directory, as
  `include` declarations outside a library do today.  Looking beside the
  including file would need `load` to record its file's directory.
- `bsd` joins `freebsd` on FreeBSD, as R7RS Appendix B lists it.
- The implementation is `minim` alone, as Chibi's is `chibi`.  A version
  feature is either one release exactly (Gauche's `gauche-0.9.15`) or
  "this release or later" (Guile's `guile-2.2`, kept by every later
  release); nothing depends on a Minim version yet.  Reopen when a release
  adds something code would check for, with Guile's kind.

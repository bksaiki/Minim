# Roadmap: R7RS-small

Where Minim goes after the basic REPL.  Each milestone gets its own
implementation plan in `docs/todos/` when it starts; this document only orders
the work and records the decisions each one needs.

## Where things stand

Minim has fixnums, chars, booleans, pairs, vectors, strings, symbols,
closures, 83 primitives, and a Scheme prelude with the list library
(`map`, `append`, `member`, `fold`, ...).  It has a precise generational
collector, a reader, a printer, and an explicit-stack evaluator with proper
tail calls, multiple values, and the special forms `quote`, `if`, `define`,
`set!`, `lambda`, `begin`, `define-values`, `let-values`, and
`letrec-values`.  A hygienic expander in the prelude adds quasiquote,
`syntax-rules` macros, and the derived forms.  `minim file.scm` runs a file,
and `load` loads one (see `docs/design/`).

Missing, roughly in order of how much it limits writing programs:

| Gap | Example |
|---|---|
| Control | `call/cc`, `dynamic-wind`, `error`, `guard` |
| Libraries | `import`, `define-library` |
| Numbers | flonums, bignums, rationals |
| I/O | ports, `read`, `read-char`, string and file ports |
| Data types | records, hashtables, bytevectors |

**Target:** a practical subset of R7RS-small, simple before complete (see
Open items).

## Milestones

Ordered by dependency, then by how much each unblocks.  Sizes are rough:
S is one or two phases, M three to five, L more.

### 1. Equality, hashing, chars, and strings (S) — done

Primitives that later milestones assume:

- `eqv?`, `equal?`, and the content hashes `string-hash` and `equal-hash`;
- `char?`, `char->integer`, `integer->char`, `char=?`, `char<?`;
- `make-string`, `string-set!`, `string-append`, `substring`, `string=?`,
  `string<?`, `string->list`, `list->string`.

Mostly mechanical additions to `prims.c`.  `equal?` must handle deep
structure without recursing on the C stack, using an explicit stack.  Cycles
can loop for now.

### 2. Prelude and running files (M) — done

A Scheme-level library, compiled into the binary and loaded at
`minim_init`.

- `minim file.scm` runs a file; `(load "file.scm")` loads one.
- `src/prelude.scm` (the library) and `src/expander.scm` are expanded into
  `src/prelude.boot`, which is embedded at build time and evaluated at
  startup.
- The prelude defines what is easiest in Scheme, above all higher-order
  procedures, which primitives can't express because they can't call
  closures: `map`, `for-each`, `append`, `reverse`, `list-tail`, `assq`,
  `assoc`, `memq`, `member`, `filter`, `fold`.

This comes early because it multiplies everything after it: most of the
remaining library can be written in Scheme.

Depends on 1 (`equal?` for `member` and `assoc`).

### 3. Derived forms and macros (M–L) — done

- `quasiquote`, `unquote`, and `unquote-splicing` in the reader and expander.
- An expansion pass between `minim_read` and `minim_eval`, which rewrites
  macro uses into the six core forms.
- `define-syntax` with `syntax-rules`.
- The derived forms, defined as macros in the prelude: `let`, `let*`,
  `letrec`, named `let`, `cond`, `case`, `and`, `or`, `when`, `unless`, `do`.

Depends on 2, since the derived forms live in the prelude.  Macros are
hygienic, as R7RS requires (`docs/design/macros.md`).

### 4. Control and errors (M)

- Done: `values`, `call-with-values`, and the `-values` binding forms.
- `error`, `raise`, `with-exception-handler`, and `guard`, with condition
  objects (message and irritants).  `minim_error` raises one, so C and Scheme
  errors are the same kind of thing.
- `call/cc`.  Heap continuation frames make capture cheap.  The value stack
  must be copied on capture and restored on reentry.
- `dynamic-wind`, which `call/cc` and `guard` must respect.

Depends on 3 (`guard` is a macro).

### 5. Libraries (M–L)

Every R7RS program starts with `(import (scheme base) ...)`, so without
libraries Minim can't run a portable program.

- `import` of the standard libraries, `(scheme base)`, `(scheme write)`,
  `(scheme cxr)`, and so on, each exporting the identifiers Minim has.
- `define-library`, with `export`, `import`, `begin`, and `include`.
- Each library's bindings in its own environment, so the prelude and the
  expander no longer share the user's top level: redefining `car` or `map`
  stops breaking expansion (`docs/design/macros.md`, Limitations).

Depends on 3: identifiers and environments are the expander's.  The main
design question is how global bindings are represented, since today they are
symbol value slots, one per symbol.

### 6. Ports and I/O (M)

- Port objects (a new typed object): input and output, file and string.
- `current-input-port`, `current-output-port`, `open-input-file`,
  `open-output-file`, `close-port`, `open-input-string`,
  `open-output-string`, `get-output-string`.
- `read`, `read-char`, `peek-char`, `read-line`, `char-ready?`, plus `write`,
  `display`, `write-char`, and `newline` taking an optional port.
- The reader and printer take ports instead of `FILE *`; `FILE *` becomes
  one kind of port.

Depends on 4, so I/O errors are conditions.  Only the exports wait on 5: the
port objects, the reader and printer on ports, and the procedures in
`(scheme base)` don't need libraries, but file ports belong to
`(scheme file)`, `read` to `(scheme read)`, and `write` and `display` to
`(scheme write)`, so those are implemented first and exported once the
libraries exist.

### 7. Numbers (L)

- Flonums: boxed doubles on primary tag `010` (reserved for this).  Reading,
  printing (shortest round-trip), arithmetic, and `exact` / `inexact`.
- Bignums: typed objects holding digits.  Fixnum overflow promotes instead of
  raising.
- Rationals: optional, after bignums.
- Generic arithmetic dispatches on the operand types; the fixnum fast path
  stays inline.

Can start any time after 1; the order is flexible.  The main decision is
hand-written bignums or GMP (see Open items).

### 8. More data types (M)

- Records: `define-record-type`, as typed objects with a type descriptor.
- Hashtables: `eq?`-based and `equal?`-based (`equal-hash` exists from
  milestone 1).  `eq?` hashing must survive the moving collector, by rehashing
  after a collection or by hashing a stable per-object value.
- Bytevectors (an unscanned typed object), and boxes.

Depends on 3 (`define-record-type` is a macro).

### 9. Runtime maturity (M, ongoing)

Not features, but needed as programs grow:

- A weak symbol table, so unused symbols are collected, as Chez does.
- Done: a generational collector on Chez's radix schedule, with a write
  barrier, reuse of freed chunks, and run-time collector settings
  (`docs/design/gc.md`).
- Done: startup loads the prelude already expanded, from a checked-in boot
  file (`docs/design/prelude.md`), so it expands nothing and runs under the
  environment's collector settings throughout.
- Done: the expander is written in normal Scheme and regenerates its own
  boot file; the evaluator binds definitions only globally.
- Done: expansion into the `-values` binding forms, so `let` allocates no
  closure and each frame's variables are known before it runs.
- Environments: a local frame is now created only by a procedure call,
  `let-values`, or `letrec-values`, and its variables are known when it is
  created, so every environment is either the global one or a chain of
  fixed-size frames.  Frames can be vectors, and a pass after expansion can
  resolve each local variable to a lexical address (frames up, slot), so a
  lookup is a few loads instead of a search by name.  `src/env.h` already
  hides the representation.
- Benchmarks, so performance changes are measured.

Can interleave with any milestone.

### 10. Tooling and conformance (S–M, ongoing)

- A conformance suite: a subset of the R7RS tests (for example Chibi's
  `r7rs-tests.scm`) as golden tests, grown with each milestone.
- Source locations in error messages.
- Line editing in the REPL, if it can be done without a dependency.

## Out of scope for now

Threads, a foreign function interface, a native or bytecode compiler, and
Unicode case folding and normalization.

## Open items

### What is the target: R7RS-small, a subset, or something else?

- **R7RS-small** is a clear checklist, with an existing test suite.
- **A pragmatic subset** keeps the implementation small, in line with "simple
  over complete", but needs a written list of what's in and out.
- **Provisional:** a subset of R7RS-small, extended milestone by milestone.
  The conformance suite records exactly what passes.

### Library in C or in Scheme?

- **Scheme (the prelude)** is shorter and handles higher-order procedures
  naturally, but runs slower and grows startup time.
- **C** is fast but verbose, and can't call closures.
- **Provisional:** higher-order and list-processing procedures in Scheme;
  type predicates, accessors, and arithmetic in C.  Move hot procedures to C
  when benchmarks show it.

### Hand-written bignums or GMP?

- **Hand-written** keeps Minim dependency-free and portable (including MSVC),
  but is a lot of code to make fast.
- **GMP** is fast and complete, but adds a dependency, and its memory isn't
  managed by Minim's collector.
- **Provisional:** hand-written schoolbook arithmetic, simple and slow.
  Reopen if bignum speed matters.

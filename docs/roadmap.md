# Roadmap

Where Minim goes after the basic REPL.  Each milestone gets its own
implementation plan in `docs/todos/` when it starts; this document only orders
the work and records the decisions each one needs.

## Where things stand

Minim has fixnums, chars, booleans, pairs, vectors, strings, symbols,
closures, 65 primitives, and a Scheme prelude with the list library
(`map`, `append`, `member`, `fold`, ...).  It has a precise copying
collector, a reader, a printer, and an explicit-stack evaluator with proper
tail calls and the special forms `quote`, `if`, `define`, `set!`, `lambda`,
and `begin`.  `minim file.scm` runs a file, and `load` loads one (see
`docs/design/`).

Missing, roughly in order of how much it limits writing programs:

| Gap | Example |
|---|---|
| Derived forms | `let`, `cond`, `and`, `or`, `case`, `do`, named `let` |
| Macros | `define-syntax`, `syntax-rules`, `quasiquote` |
| Control | `call/cc`, `dynamic-wind`, `values`, `error`, `guard` |
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
- `src/prelude.scm` is embedded at build time (CMake turns it into a C
  string) and evaluated at startup.
- The prelude defines what is easiest in Scheme, above all higher-order
  procedures, which primitives can't express because they can't call
  closures: `map`, `for-each`, `append`, `reverse`, `list-tail`, `assq`,
  `assoc`, `memq`, `member`, `filter`, `fold`.

This comes early because it multiplies everything after it: most of the
remaining library can be written in Scheme.

Depends on 1 (`equal?` for `member` and `assoc`).

### 3. Derived forms and macros (M–L)

- `quasiquote`, `unquote`, and `unquote-splicing` in the reader and expander.
- An expansion pass between `minim_read` and `minim_eval`, which rewrites
  macro uses into the six core forms.
- `define-syntax` with `syntax-rules`.
- The derived forms, defined as macros in the prelude: `let`, `let*`,
  `letrec`, named `let`, `cond`, `case`, `and`, `or`, `when`, `unless`, `do`.

Depends on 2, since the derived forms live in the prelude.  The main
decision is hygiene (see Open items).

### 4. Control and errors (M)

- `values` and `call-with-values`.
- `error`, `raise`, `with-exception-handler`, and `guard`, with condition
  objects (message and irritants).  `minim_error` raises one, so C and Scheme
  errors are the same kind of thing.
- `call/cc`.  Heap continuation frames make capture cheap.  The value stack
  must be copied on capture and restored on reentry.
- `dynamic-wind`, which `call/cc` and `guard` must respect.

Depends on 3 (`guard` is a macro).

### 5. Ports and I/O (M)

- Port objects (a new typed object): input and output, file and string.
- `current-input-port`, `current-output-port`, `open-input-file`,
  `open-output-file`, `close-port`, `open-input-string`,
  `open-output-string`, `get-output-string`.
- `read`, `read-char`, `peek-char`, `read-line`, `char-ready?`, plus `write`,
  `display`, `write-char`, and `newline` taking an optional port.
- The reader and printer take ports instead of `FILE *`; `FILE *` becomes
  one kind of port.

Depends on 4, so I/O errors are conditions.

### 6. Numbers (L)

- Flonums: boxed doubles on primary tag `010` (reserved for this).  Reading,
  printing (shortest round-trip), arithmetic, and `exact` / `inexact`.
- Bignums: typed objects holding digits.  Fixnum overflow promotes instead of
  raising.
- Rationals: optional, after bignums.
- Generic arithmetic dispatches on the operand types; the fixnum fast path
  stays inline.

Can start any time after 1; the order is flexible.  The main decision is
hand-written bignums or GMP (see Open items).

### 7. More data types (M)

- Records: `define-record-type`, as typed objects with a type descriptor.
- Hashtables: `eq?`-based and `equal?`-based (`equal-hash` exists from
  milestone 1).  `eq?` hashing must survive the moving collector, by rehashing
  after a collection or by hashing a stable per-object value.
- Bytevectors (an unscanned typed object), and boxes.

Depends on 3 (`define-record-type` is a macro).

### 8. Runtime maturity (M, ongoing)

Not features, but needed as programs grow:

- A weak symbol table, so unused symbols are collected, as Chez does.
- Chunk reuse in the collector; this also speeds up stress mode.
- A static space for objects created at startup (the prelude), never copied
  again, as in Chez's static generation.  Stress mode now copies the prelude
  on every allocation, which tripled the stress suite's time.
- Evaluator speed: a pre-pass that resolves variables to lexical addresses,
  with environments as vectors.  `src/env.h` already hides the
  representation.
- Benchmarks, so performance changes are measured.

Can interleave with any milestone.

### 9. Tooling and conformance (S–M, ongoing)

- A conformance suite: a subset of the R7RS tests (for example Chibi's
  `r7rs-tests.scm`) as golden tests, grown with each milestone.
- Source locations in error messages.
- Line editing in the REPL, if it can be done without a dependency.

## Out of scope for now

Threads, a foreign function interface, a native or bytecode compiler, full
R7RS libraries (`define-library`), and Unicode case folding and
normalization.

## Open items

### What is the target: R7RS-small, a subset, or something else?

- **R7RS-small** is a clear checklist, with an existing test suite.
- **A pragmatic subset** keeps the implementation small, in line with "simple
  over complete", but needs a written list of what's in and out.
- **Provisional:** a subset of R7RS-small, extended milestone by milestone.
  The conformance suite records exactly what passes.

### Hygienic or non-hygienic macros?

- **`syntax-rules` with hygiene** is the standard, but it needs renaming and
  environments of syntax, the largest single piece of this roadmap.
- **Non-hygienic `define-macro`** (like Common Lisp's `defmacro`) is a few
  dozen lines, but users can capture identifiers by accident.
- **Middle ground:** non-hygienic `syntax-rules`, using its pattern language
  without renaming.  It covers the derived forms at low cost; hygiene can come
  later.
- **Provisional:** the middle ground first.  Reopen before user macros
  matter.

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

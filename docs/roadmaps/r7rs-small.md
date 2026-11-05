# Roadmap: R7RS-small

Where Minim goes after the basic REPL, toward R7RS-small.  Each milestone gets
its own implementation plan in `docs/todos/` when it starts; this document
only orders the work and records the decisions each one needs.  Work R7RS-small
doesn't require is in `beyond-r7rs.md`.

## Where things stand

Minim has fixnums, chars, booleans, pairs, vectors, strings, symbols,
closures, continuations, error objects, records, textual ports, and a Scheme
prelude with the list library (`map`, `append`, `member`, `fold`, ...),
`dynamic-wind`, exceptions, and parameters.  It has a precise generational
collector, a reader and printer on ports, and an explicit-stack evaluator
with proper tail calls, multiple values, and the special forms `quote`,
`if`, `define`, `set!`, `lambda`, `begin`, `define-values`, `let-values`,
and `letrec-values`.  A hygienic expander in the prelude adds quasiquote,
`syntax-rules` macros, the derived forms, and R7RS libraries.  `minim
file.scm` runs a script, `minim --program file.scm` an R7RS program, and
`load` loads a file (see `docs/design/`).

Missing, roughly in order of how much it limits writing programs:

| Gap | Example |
|---|---|
| Numbers | flonums, bignums, rationals |
| Data types | bytevectors |

**Coverage:** the names of each standard library (R7RS Appendix A) Minim
provides.  Update it as names are added.

| Library | Provided | R7RS | Notes |
|---|---|---|---|
| `(scheme base)` | 163 | 238 | |
| `(scheme case-lambda)` | 1 | 1 | |
| `(scheme char)` | 22 | 22 | |
| `(scheme complex)` | 0 | 6 | not defined |
| `(scheme cxr)` | 24 | 24 | |
| `(scheme eval)` | 2 | 2 | |
| `(scheme file)` | 8 | 10 | binary files wait for bytevectors |
| `(scheme inexact)` | 0 | 12 | not defined |
| `(scheme lazy)` | 0 | 5 | not defined |
| `(scheme load)` | 1 | 1 | |
| `(scheme process-context)` | 4 | 5 | |
| `(scheme r5rs)` | 168 | 217 | plus R5RS's auxiliary syntax, which R7RS's list omits |
| `(scheme read)` | 1 | 1 | |
| `(scheme repl)` | 1 | 1 | |
| `(scheme time)` | 0 | 3 | not defined |
| `(scheme write)` | 3 | 4 | `write-shared` waits for datum labels |

**Target:** R7RS-small compliance: every standard library complete, as the
coverage table measures, and the conformance suite passing.  Where R7RS lets
an implementation omit something, such as rationals or `(scheme complex)`,
Minim may.

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
- The Scheme sources in `scheme/` are expanded into `scheme/prelude.boot`,
  which is embedded at build time and evaluated at startup.
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

### 4. Control and errors (M) — done

- `values`, `call-with-values`, and the `-values` binding forms.
- `error`, `raise`, `raise-continuable`, `with-exception-handler`, and
  `guard`, with error objects (message and irritants).  An error from C
  raises one, so C and Scheme errors are the same kind of thing
  (`docs/design/errors.md`).
- `call/cc` and `dynamic-wind`, on a segmented value stack, so a capture
  copies at most one segment (`docs/design/eval.md`).

Depends on 3 (`guard` is a macro).

### 5. Libraries (M–L) — done

- `define-library`, with `export`, `import`, `begin`, and `include`; `import`
  with `only`, `except`, `prefix`, and `rename`; library files, `foo/bar.sld`
  on `MINIM_LIBRARY_PATH`.
- The standard libraries Minim has names for: `(scheme base)`, `(scheme
  file)`, `(scheme load)`, `(scheme process-context)`, `(scheme write)`, and
  `(scheme r5rs)`.  `(scheme char)` came with its procedures (9).
- Top-level environments in the expander, after Chez Scheme: names map to
  labels, symbols whose value slots are the variables, so a global reference
  stays one load (`docs/design/macros.md`).
- The REPL sees `(minim)`, every public name, and no internals; `(minim
  system)` has them all.  Programs, `minim --program`, see only what they
  import (R7RS 5.1).
- Later: `cond-expand`, `include-ci`, and `include-library-declarations`.

### 6. Ports and I/O (M) — done

Textual ports over files and strings, the current ports as parameters, and
the reader and printer on ports (`docs/design/ports.md`).

Gaps:

- Binary ports: `read-u8`, `peek-u8`, `u8-ready?`, `write-u8`,
  `read-bytevector`, `write-bytevector`, bytevector ports, and
  `open-binary-input-file` and `open-binary-output-file`.  They wait for
  bytevectors (8).
- Cycles: `write` prints cyclic data forever, where R7RS labels cycles
  (`#0=`, `#0#`); `write-shared` and reading labels are missing too (9).
- `char-ready?` is always true: stdio can't tell whether input is waiting,
  so a read from a terminal after it may block.  Fixing it needs Minim's own
  buffers, as Chez has.
- UTF-8 only, with no transcoders; a malformed sequence reads as U+FFFD.
- No custom ports, which would need a handler record per port, as Chez's.

The milestone as planned:

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

- Done: records, `define-record-type`, as typed objects whose first word is
  their type descriptor (`docs/design/gc.md`).
- Bytevectors (an unscanned typed object), and binary ports over them and
  over files (`docs/design/ports.md`).

Depends on 3 (`define-record-type` is a macro).

### 9. The rest of the standard libraries (M, ongoing)

The names the coverage table above lacks that no other milestone brings:
`(scheme char)`, `(scheme lazy)`, and the rest of `(scheme base)`'s lists,
strings, vectors, and control.  Mostly prelude procedures.

- Done: `make-parameter` and `parameterize`, and `interaction-environment`
  a parameter (`docs/design/prelude.md`, `docs/design/macros.md`).
- Done: `(scheme eval)`, `(scheme repl)`, and R5RS's environments
  (`docs/design/macros.md`).
- Done: `command-line`, with scripts and programs taking arguments
  (`docs/design/repl.md`).
- Done: `exit` and `emergency-exit`; `case-lambda`, a core form after Chez
  (`docs/design/eval.md`); datum and block comments (`docs/design/repl.md`).
- Done: `(scheme cxr)`, primitives beside `caar` and the like
  (`docs/design/primitives.md`).
- Done: `(scheme char)`, on generated Unicode tables with full case
  mapping, and R7RS's char names and string escapes
  (`docs/design/primitives.md`).
- Done: `(scheme base)`'s strings: `string`, `string-copy`, `string-copy!`,
  `string-fill!`, `string-map`, `string-for-each`, and ranges.  Next:
  vectors (`docs/todos/vectors.md`).
- Cycles: `write` with datum labels for cycles, `write-shared` for all
  sharing, the reader's `#n=` and `#n#`, and `equal?` terminating on cyclic
  data.  The printer and `equal?` loop on cycles today.

### 10. Conformance (S–M, ongoing)

Done: Chibi Scheme's R7RS test suite runs section by section, each a golden
test of its report (`docs/design/testing.md`).  Ongoing: a section's report
changes as milestones land, until every test passes.  Where Minim stands:

| Section | Passed | Failed | Errors |
|---|---|---|---|
| 4.1 Primitive expression types | 27 | 0 | 0 |
| 4.2 Derived expression types | 45 | 18 | 9 |
| 4.3 Macros | 23 | 2 | 1 |
| 5 Program structure | 15 | 0 | 0 |
| 6.1 Equivalence Predicates | 25 | 0 | 0 |
| 6.2 Numbers | 24 | 120 | 76 |
| 6.3 Booleans | 13 | 5 | 0 |
| 6.4 Lists | 61 | 4 | 0 |
| 6.5 Symbols | 13 | 4 | 0 |
| 6.6 Characters | 79 | 0 | 0 |
| 6.7 Strings | 130 | 0 | 0 |
| 6.8 Vectors | 13 | 30 | 0 |
| 6.9 Bytevectors | 0 | 11 | 28 |
| 6.10 Control Features | 26 | 8 | 0 |
| 6.11 Exceptions | 24 | 6 | 0 |
| 6.12 Environments and evaluation | 1 | 1 | 2 |
| 6.13 Input and output | 42 | 2 | 18 |
| Read syntax | 76 | 6 | 11 |
| Numeric syntax | 8 | 2 | 105 |
| 6.14 System interface | 5 | 6 | 1 |
| **Total** | 490 | 378 | 255 |

The reports name each failing test by line; the gaps are those of the
coverage table and of milestones 6 to 9.  Update the table with the reports.

## Open items

### Hand-written bignums or GMP?

- **Hand-written** keeps Minim dependency-free and portable (including MSVC),
  but is a lot of code to make fast.
- **GMP** is fast and complete, but adds a dependency, and its memory isn't
  managed by Minim's collector.
- **Provisional:** hand-written schoolbook arithmetic, simple and slow.
  Reopen if bignum speed matters.

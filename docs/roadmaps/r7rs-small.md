# Roadmap: R7RS-small

Where Minim goes after the basic REPL, toward R7RS-small.  Each milestone gets
its own implementation plan in `docs/todos/` when it starts; this document
only orders the work and records the decisions each one needs.  Work R7RS-small
doesn't require is in `beyond-r7rs.md`.

## Where things stand

Minim has fixnums, chars, booleans, pairs, vectors, strings, symbols,
closures, continuations, error objects, records, 105 primitives, and a Scheme
prelude with the list library (`map`, `append`, `member`, `fold`, ...),
`dynamic-wind`, and exceptions.  It has a precise generational collector, a
reader, a printer, and an explicit-stack evaluator with proper
tail calls, multiple values, and the special forms `quote`, `if`, `define`,
`set!`, `lambda`, `begin`, `define-values`, `let-values`, and
`letrec-values`.  A hygienic expander in the prelude adds quasiquote,
`syntax-rules` macros, the derived forms, and R7RS libraries.  `minim
file.scm` runs a file, and `load` loads one (see `docs/design/`).

Missing, roughly in order of how much it limits writing programs:

| Gap | Example |
|---|---|
| Numbers | flonums, bignums, rationals |
| I/O | ports, `read`, `read-char`, string and file ports |
| Data types | bytevectors |

**Coverage:** the names of each standard library (R7RS Appendix A) Minim
provides.  Update it as names are added.

| Library | Provided | R7RS | Notes |
|---|---|---|---|
| `(scheme base)` | 128 | 238 | |
| `(scheme case-lambda)` | 0 | 1 | not defined |
| `(scheme char)` | 0 | 22 | not defined |
| `(scheme complex)` | 0 | 6 | not defined |
| `(scheme cxr)` | 0 | 24 | not defined |
| `(scheme eval)` | 0 | 2 | not defined |
| `(scheme file)` | 1 | 10 | |
| `(scheme inexact)` | 0 | 12 | not defined |
| `(scheme lazy)` | 0 | 5 | not defined |
| `(scheme load)` | 1 | 1 | |
| `(scheme process-context)` | 1 | 5 | |
| `(scheme r5rs)` | 104 | 217 | plus R5RS's auxiliary syntax, which R7RS's list omits |
| `(scheme read)` | 0 | 1 | not defined |
| `(scheme repl)` | 0 | 1 | not defined |
| `(scheme time)` | 0 | 3 | not defined |
| `(scheme write)` | 2 | 4 | |

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
  `(scheme r5rs)`.  `(scheme char)` and `(scheme cxr)` wait for their
  procedures.
- Top-level environments in the expander, after Chez Scheme: names map to
  labels, symbols whose value slots are the variables, so a global reference
  stays one load.  The REPL's definitions shadow system bindings instead of
  replacing them (`docs/design/macros.md`).
- Later: `cond-expand`, `include-ci`, and `include-library-declarations`.

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

- Done: records, `define-record-type`, as typed objects whose first word is
  their type descriptor (`docs/design/gc.md`).
- Bytevectors (an unscanned typed object).

Depends on 3 (`define-record-type` is a macro).

### 9. The rest of the standard libraries (M, ongoing)

The names the coverage table above lacks that no other milestone brings:
`(scheme char)`, `(scheme cxr)`, `case-lambda`, `(scheme lazy)`, `eval` and
`environment`, `exit`, and the rest of `(scheme base)`'s lists, strings,
vectors, and control.  Mostly prelude procedures.

### 10. Conformance (S–M, ongoing)

A conformance suite: the R7RS tests (for example Chibi's `r7rs-tests.scm`) as
golden tests, enabled as each milestone makes them pass, until all do.

## Open items

### Hand-written bignums or GMP?

- **Hand-written** keeps Minim dependency-free and portable (including MSVC),
  but is a lot of code to make fast.
- **GMP** is fast and complete, but adds a dependency, and its memory isn't
  managed by Minim's collector.
- **Provisional:** hand-written schoolbook arithmetic, simple and slow.
  Reopen if bignum speed matters.

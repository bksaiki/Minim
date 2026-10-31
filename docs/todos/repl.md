# REPL: printer, reader, and a read-print loop

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

The runtime has tagged values (fixnums, chars, booleans, `'()`, void, eof),
four heap kinds (pairs, vectors, strings, symbols), a precise copying
collector, and symbol interning (see `docs/design/gc.md`).  `src/main.c` still
prints "Hello, world!".  Nothing can print or read Scheme.

This plan adds a printer, error recovery, and a reader, ending with a REPL
that echoes data back:

```
> (1 2 . 3)
(1 2 . 3)
> )
error: unexpected )
> 
```

Evaluation comes next, in `docs/todos/eval.md`.

Out of scope: evaluation, port objects, file loading, line editing, and
condition objects.

**Decisions:**

| Decision | Choice | Reason |
|---|---|---|
| Errors | `longjmp` to the innermost `minim_catch` | unwinds C code and the reader; restores the root stack |
| Input / output | `FILE *`; no port objects | ports can come later |
| REPL prompt | on stderr, only when stdin is a terminal (`isatty`) | the Unix convention (bash, Python); piped runs print only results |

## Printer

`minim_write(FILE *out, minim_obj x)` prints `write` syntax and never
allocates, so it is safe in error paths.

| Value | Printed |
|---|---|
| fixnum | decimal |
| `#t` / `#f`, `'()` | `#t` / `#f`, `()` |
| void / eof / unbound | `#<void>` / `#<eof>` / `#<unbound>` |
| char | `#\a`, `#\space`, `#\newline`, `#\tab`, `#\nul`, otherwise `#\xHH` if not printable |
| string | `"..."` with `\"`, `\\`, `\n`, `\t` escapes, UTF-8 encoded |
| symbol | name, UTF-8 encoded |
| pair | `(a b . c)`; `(quote x)` as `'x` |
| vector | `#(a b)` |

Cyclic data loops forever; datum labels are out of scope.

## Errors

```c
_Noreturn void minim_error(const char *msg, minim_obj irritant);
bool minim_catch(minim_obj (*fn)(void *), void *arg, minim_obj *result);
```

`minim_catch` saves the root-stack height and a `jmp_buf`, then calls `fn`.
`minim_error` records the message and irritant (a global root) and jumps to
the innermost catch, which restores the root-stack height and returns
`false`.  `minim_last_error(const char **msg, minim_obj *irritant)` retrieves
them.  Catches nest.  With no catch installed, `minim_error` prints and
aborts.

## Reader

`minim_read(FILE *in)` returns the next datum, or `MINIM_EOF`.  It is
recursive descent and reports errors with `minim_error`.

- Lists, including dotted pairs; vectors `#(...)`.
- Fixnums with an optional sign; out-of-range literals are errors.  `+`, `-`,
  and `...` are symbols.
- Strings with `\"`, `\\`, `\n`, `\t`, and `\xHH;` escapes.
- Chars: `#\a`, the names the printer uses, and `#\xHH`.
- `#t`, `#f`, `#true`, `#false`.
- Symbols are case-sensitive.
- `'x` reads as `(quote x)`.
- `;` comments.
- Input is UTF-8; the decoder in `object.c` becomes shared.

## Phases

### Phase 1 - Printer

**Done.**  Details settled during the phase:

- Well-known symbols live in `src/symbol.h`, interned by
  `minim_symbol_init` and registered as roots.  `minim_sym_quote` is the
  first; the evaluator's special-form symbols go there too.  Interning on first use
  would allocate during printing and leave a stale pointer after
  `minim_shutdown`.
- The UTF-8 codec is `src/char.c` (`minim_utf8_decode`, `minim_utf8_encode`).
- Control characters (`< 0x20`, `0x7F`–`0x9F`) print as `#\x7f` in chars and
  `\x1;` in strings, matching the reader's planned escapes.  Unrecognized
  values print as `#<unknown>`.

- `include/minim.h`, `src/print.c`: `minim_write`.
- `src/object.c`: split out a UTF-8 encoder (`minim_utf8_encode`, internal,
  in a new `src/char.h` with the existing decoder).

Separate because every later phase uses it for tests and debugging.

Tests (`tests/test_print.c`, writing to `tmpfile()` and reading it back):

- every row of the printer table, including dotted pairs, nested vectors,
  `'x`, string escapes, and non-ASCII chars;
- printing performs no allocation (`minim_heap.allocated` unchanged).

```sh
cmake --build --preset dev && ctest --preset dev -R print
```

### Phase 2 - Errors

**Done.**  Details settled during the phase:

- `src/error.h` has `minim_error_init`, called from `minim_init`; it
  registers the irritant as a root.
- `MINIM_VOID` means no irritant; the no-catch fallback then prints only the
  message.
- `msg` is stored as a pointer, so it must outlive the catch (in practice, a
  literal).
- On error, `*result` is left untouched.
- Checked by hand: an error with no catch prints `error: msg: irritant` and
  aborts.

- `include/minim.h`, `src/error.c`: `minim_error`, `minim_catch`,
  `minim_last_error`.

Separate because the reader, evaluator, and primitives all report errors
through it.

Tests (`tests/test_error.c`):

- `minim_catch` returns `true` and the result when `fn` returns;
- an error returns `false`, with the message and irritant retrievable;
- the root-stack height is restored after an error raised inside a frame;
- nested catches: the inner one catches; after it returns, the outer one is
  active again;
- the irritant survives a collection.

```sh
cmake --build --preset dev && ctest --preset dev -R error
```

### Phase 3 - Reader and read-print loop

**Done.**  Details settled during the phase:

- `minim_utf8_getc(FILE *)` in `src/char.c` reads one code point.  The reader
  only ever pushes back ASCII delimiters, so `ungetc`'s one-byte pushback is
  enough and no state persists between `minim_read` calls.
- Token characters accumulate in a static buffer, so an error can't leak it.
- Tokens that aren't fixnums, like `1.5`, read as symbols.
- `minim_write_error(FILE *)` prints the last error; the REPL and the no-catch
  fallback share it.
- `main.c` defines `_POSIX_C_SOURCE` for `isatty`/`fileno`, since the build
  uses strict C17.
- The golden runner captures minim's stderr and shows it only on failure.
- After an error, the REPL continues from the rest of the line rather than
  discarding it.

- `include/minim.h`, `src/read.c`: `minim_read`.
- `src/main.c`: loop: prompt `> ` on stderr if stdin is a terminal
  (`isatty`, or `_isatty` on Windows), `minim_read` from stdin,
  `minim_write` to stdout, newline.  Exits on eof.  Errors print `error: msg:
  irritant` to stderr and the loop continues.
- `tests/run_repl.cmake`, `tests/repl/*.scm` with `.out` files: a golden-test
  runner that pipes each `.scm` file into `minim` and compares stdout.
  `minim_add_repl_test(name)` in `tests/CMakeLists.txt`.

Separate because it gives an end-to-end harness before the evaluator exists.

Tests:

- `tests/test_read.c` (reading from `tmpfile()`): each datum kind,
  round-tripped through the printer; errors for `)`, `(1 . )`, an eof inside
  a list or string, a bad `#` syntax, and an out-of-range fixnum.
- `tests/repl/echo.scm`: data echo back unchanged, and an error line is
  followed by further output.

```sh
cmake --build --preset dev && ctest --preset dev -R 'read|repl'
```

### After the last phase

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
cmake --preset stress && cmake --build --preset stress && ctest --preset stress
```

No linter is configured; the build must be warning-free.

## Open items

None.

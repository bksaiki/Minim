# Ports: textual ports and I/O

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

Milestone 6 (`r7rs-small.md`).  All I/O is through `FILE *` (`repl.md`):

- `minim_read(FILE *)` and `minim_read_string` read through the reader's
  `source`, a file or a C string, with one ASCII character of `ungetc`
  pushback (`src/read.c`).
- `minim_write` and `minim_display` print to a `FILE *`, never allocating,
  so they are safe on error paths (`src/print.c`).
- `write`, `display`, and `newline` print to `stdout` (`src/system.c`);
  `load` reads a file through `minim_read_file`; the REPL reads `stdin`
  (`src/main.c`).

| Input | Today |
|---|---|
| `(read)` | `unbound variable: read` |
| `(open-input-string "1 2")` | `unbound variable: open-input-string` |
| `(read-char)` | `unbound variable: read-char` |
| `(display "x" (current-error-port))` | `unbound variable: current-error-port` |
| `(parameterize ((current-output-port p)) (display 1))` | nothing to redirect |

Parameters (`prelude.md`) now exist for the current ports, and conditions
(`errors.md`) for `file-error?` and `read-error?`.

## Design

R7RS 6.13, textual ports only: binary ports wait for bytevectors
(milestone 8).

**A port is a typed object**, on a free type code, after Chez's
(`s/cmacros.ss`): `[header | buffer | name | index | lookahead | file]`.

- The header holds the flags, as Chez's type word does: input or output,
  file or string, open or closed, in the length bits a port's fixed size
  leaves unused.  So the predicates read only the header.
- `name`: a string, the file's path, `stdin`, `stdout`, `stderr`, or
  `string`; traced.
- `file`: the `FILE *` of a file port, unscanned, like a primitive's
  function pointer.  File ports use stdio's buffering underneath, not
  Minim's own as Chez's do: little code, and C's own `stdout` writes stay in
  order with the ports'.  So `char-ready?` can't ask whether input is
  waiting: it is true unless a string port or the lookahead says otherwise,
  and a terminal `stdin` may then block.
- `buffer`: a string port's string, traced.  An input string port reads it
  from `index`, as Chez's input buffer is its string; an output one appends
  to it, at `index`, the length so far, growing it by doubling.
  `get-output-string` copies what was written so far and, unlike Chez's,
  doesn't reset the port (R7RS).
- `lookahead`: a code point read but not consumed, or none: what
  `peek-char` and the reader's one-character pushback use, for both kinds,
  in place of `ungetc`.
- It prints as Chez's do: `#<input port load/defs.scm>`, `#<output port
  stdout>`.

Unlike Chez's, a port has no handler, a record of operations shared by every
kind: file and string ports are two cases of a C `switch`.  Nor separate
input and output buffers, since R7RS has no input/output ports, nor an
`info` record, since `index` and `file` are all the two kinds need.

**Standard ports**: input, output, and error ports over `stdin`, `stdout`,
and `stderr`, made at startup and rooted.  `current-input-port`,
`current-output-port`, and `current-error-port` are parameters (R7RS),
initially those, their converters requiring a port of the right direction.
The REPL reads through the standard input port, so it and `(read-char)`
share the lookahead, and prints its results to the standard output port,
not the current one, as Chez's prints to the console: parameterizing
`current-output-port` in a REPL form captures what it displays, not its
printed result.

**The reader** reads from a port: its `source` becomes a port's state, a
`FILE *` or a string through a rooted reference, plus the lookahead, written
back to the port after each datum.  `minim_read` takes a port;
`minim_read_string` stays, on C strings, for the boot file.

**The printer** still never allocates while it walks a datum: it writes to a
sink, a `FILE *` or a C scratch buffer of code points, and a string port
gets the buffer appended after the walk.  `minim_write` and `minim_display`
keep their `FILE *` forms for C and error paths.  `write` doesn't label
cycles yet, so it is `write-simple`, and `write-shared` waits: datum labels,
reading them, and `equal?` on cycles are one later piece of work.

**Files** open through `minim_fopen` (`src/system.c`), `fopen_s` on MSVC,
the first of the wrappers that replace `_CRT_SECURE_NO_WARNINGS`.  Failing
to open raises a file error (`file-error?`).  A port closed twice is fine;
reading or writing a closed port, or one of the wrong direction, is an
error.

**A file port is closed when it is collected.**  The collector keeps a list
of objects owning outside resources, each with a C destructor.  After a
collection, an entry in a collected generation that was copied is updated
to its new address; one that wasn't is dead, and its destructor runs on the
old copy, still readable until its chunk is freed: for a port, `fclose`,
which flushes it.  Destructors never touch the heap or revive an object,
unlike Chez's guardians, which need another tracing pass and a Scheme
interface.  `minim_shutdown` runs the destructors of what is still alive.

## Phases

### Phase 1 - Port objects

- `src/port.c`, `src/port.h` (new): the type, its flags in the header, its
  layout in `include/minim.h` and `src/gc.c` (size, traced fields), printing
  as `#<input port name>` and `#<output port name>`; the standard ports;
  file and string ports; closing.
- `src/system.c`: `minim_fopen`; `load` and `file-exists?` open through it.
- Primitives: `port?`, `input-port?`, `output-port?`, `textual-port?`,
  `binary-port?`, `input-port-open?`, `output-port-open?`, `close-port`,
  `close-input-port`, `close-output-port`, `open-input-string`,
  `open-output-string`, `open-input-file`, `open-output-file`,
  `eof-object`, `eof-object?`, and the internal `%standard-port`.
- `scheme/standard.scm`: their exports, to `(scheme base)` and
  `(scheme file)`.  `scheme/prelude.boot` regenerated.

Separate because it adds the object and its life cycle, with nothing reading
or writing through it yet.

Tests: `tests/test_port.c` (new): a string port's buffer and name survive
collections, old and young; a port's `FILE *` is not traced; the flags
survive being copied with the header.
`tests/repl/ports.scm` (new): each predicate on each kind of port and on
non-ports; closing, twice; `open-input-file` of a missing file is a file
error; each kind's printed form; `(eof-object? (eof-object))`.

```sh
cmake --build --preset dev && ctest --preset dev -R 'port|gc|libraries' && ctest --preset stress -R 'port|gc'
```

**Done.**

- Ports are public API, like pairs and error objects: `include/minim.h` has
  the type (`MINIM_TYPE_PORT`, 3), size, flags, accessors,
  `minim_standard_port`, `minim_open_input_string`,
  `minim_open_output_string`, `minim_open_input_file`,
  `minim_open_output_file`, and `minim_port_close`, since `minim_read` will
  take a port (Phase 4).  `src/port.h` declares only `minim_port_init`.
  `test_port` uses only the public API.
- `open-input-string` reads a copy, so changing the string doesn't change
  the port.  A port's name is a copy of its path too.
- Closing a standard port flushes it, if output, but leaves C's stream open:
  the REPL and error messages still use it.
- A non-string path is `not a string`, a general error; only a failed open
  is a file error.
- `(scheme r5rs)` also exports this phase's names that R5RS has.
- `count_predicates` (`tests/test.h`) counts `minim_is_port`.
- Ran `ctest -R 'port|gc|libraries|object|tag|boot|files|load|print'`, and
  stress `port|gc`.

### Phase 2 - Closing collected file ports

- `src/gc.c`, `src/gc.h`: the destructor list, `minim_gc_add_destructor(x,
  fn)`; after each collection, entries in the collected generations are
  updated or, if dead, destroyed; `minim_shutdown` destroys the rest.
- `src/port.c`: a file port registers `fclose` when opened; closing it
  explicitly leaves the destructor nothing to do.

Separate because it changes the collector, on Phase 1.

Tests (`tests/test_port.c`): an output file port written to and dropped is
flushed and closed by a collection, its file then read back whole; a live
one survives collections, young and promoted, still open; one closed
explicitly, then collected, isn't closed again; a port alive at
`minim_shutdown` is flushed.

```sh
cmake --build --preset dev && ctest --preset dev -R 'port|gc' && ctest --preset stress -R 'port|gc'
```

**Done.**

- `destroy_dead` runs at the end of `collect`, after scanning and before the
  from-space chunks are freed: an entry in a collected generation whose old
  copy holds the forward marker gets the new address; any other is dead, and
  its destructor runs on the old copy.
- `minim_destructor` is internal (`src/gc.h`); only file ports register one.
  `minim_port_close` clears the `FILE *`, so the destructor of a port closed
  explicitly does nothing.
- Checked by breaking the code: without `destroy_dead`, both dropped-port
  checks fail; with `minim_port_close` keeping the `FILE *`, ASan reports a
  double free.
- Ran `ctest -R 'port|gc'`, dev and stress.

### Phase 3 - Characters

- `src/port.c`: reading and writing code points through a port, with the
  lookahead; UTF-8 for file ports (`minim_utf8_getc` and `write_utf8`).
- Primitives: `read-char`, `peek-char`, `read-line`, `read-string`,
  `char-ready?`, `write-char`, `write-string`, `flush-output-port`,
  `get-output-string`; `newline` takes a port.
- `scheme/ports.scm` (new, after `eval.scm` in `MINIM_SCHEME_SOURCES`):
  `current-input-port`, `current-output-port`, `current-error-port`, and
  the procedures' default port arguments.  Exports.  `scheme/prelude.boot`
  regenerated.

Separate because it is the character interface, on Phases 1 and 2, that
the reader and printer then build on.

Tests (`tests/repl/ports.scm`): each procedure on string ports, including
non-ASCII; `peek-char` then `read-char`; `read-line` with and without a
final newline; end of input gives the eof object; `char-ready?`; writing to
an input port, reading from an output port, and either on a closed port are
errors; `parameterize` of `current-output-port` to a string port captures
`write-char` and `newline`.  `tests/scripts/`: reading a file's characters
and writing a file, `stdin` through `read-line`.

```sh
cmake --build --preset dev && ctest --preset dev -R 'port|script' && ctest --preset stress -R 'port'
```

**Done.**

- `minim_port_read_char`, `minim_port_peek_char`, and
  `minim_port_write_char` are public, in `minim.h`, for the reader and
  printer and for C.  Peeking the end is kept (`EOF_LOOKAHEAD`), so a
  terminal's end of input is read once.  A string port's buffer doubles,
  through `minim_gc_write`, since the port may be old.
- The primitives take their port as a required argument (`%read-char`, ...)
  and `scheme/ports.scm` wraps them, since a primitive can't call a
  parameter.  `newline` moved there from `src/system.c`, as `write-char` of
  a linefeed.  `get-output-string` is a primitive itself.
- `read-line` ends a line at a linefeed, a carriage return, or both.
- The reader has no `\r` escape, so the tests write `\x0D;`.
- UBSan caught `memcpy` from a NULL buffer for an empty result
  (`(read-string 0 ...)`); fixed.
- Script tests take an `INPUT` file for stdin (`tests/CMakeLists.txt`);
  `tests/scripts/stdin.scm` copies numbered lines.  Writing a file waits
  for `delete-file` (Phase 6), so the test leaves nothing behind.
- Ran `ctest -R 'port|script|boot|libraries'`, and stress `port|stdin`.

### Phase 4 - The reader on ports

- `src/read.c`: the source as a port's state; `minim_read(port)`;
  `minim_read_file` reads through a file port, closed on error.
- `src/main.c`, `tests/test_read.c`, `tests/test_eval.c`: reading through
  ports.
- Primitive `read`, in `(scheme read)`.  `scheme/prelude.boot` regenerated.

Separate because it changes the reader's interface and the REPL's input,
on Phase 3.

Tests (`tests/repl/ports.scm`): `read` of several data from a string port,
then the eof object; `read` then `read-char` sees what follows the datum;
bad syntax is a read error (`read-error?`).  Every REPL and script test
unchanged, the REPL now reading through the standard input port; `load`
and `include` unchanged.

```sh
cmake --build --preset dev && ctest --preset dev && ctest --preset stress -R 'read|port|load|files'
```

**Done.**

- The reader's `source` holds a reference to a port its caller roots, or a
  C string for the boot file (`minim_read_string`); pushback goes to the
  port's lookahead (`minim_port_unread_char`, in `src/port.h`), so any code
  point can be pushed back, and the end is kept.
- `minim_read(port)` raises its errors as read errors, so `read` gets
  `read-error?` with no code of its own, and `minim_read_file` just closes
  its port and re-raises.  It opens through `minim_port_open_file`
  (`src/port.h`), keeping `load: cannot open file`.
- The tests read through string ports: `tests/test.h`'s `input` makes one,
  and `eval_source` evaluates a source text through one, rooted; the
  `tmpfile` helper is `input_file`, for `test_string`'s UTF-8 decoding.
- `tests/repl/ports.scm` checks the REPL and `read-line` and `read` share
  the standard input port: each reads what follows it on its own line.
- Ran both the dev suite and stress `read|port|load|files|eval|stdin`.

### Phase 5 - The printer on ports

- `src/print.c`: printing to a sink, a `FILE *` or a scratch buffer
  appended to a string port afterward.
- `src/system.c`: `write`, `display`, and the new `write-simple` take an
  optional port, the current output port by default.  Exports to
  `(scheme write)`.  `scheme/prelude.boot` regenerated.

Separate because it changes the printer, on Phase 3.

Tests (`tests/repl/ports.scm`): `write` and `display` of nested data,
strings, and chars to a string port, read back with `get-output-string`;
`parameterize` of `current-output-port` captures `display`; printing to a
closed port is an error.  `tests/test_print.c`: printing to a string port
collects garbage only after the walk.

```sh
cmake --build --preset dev && ctest --preset dev -R 'print|port|repl' && ctest --preset stress -R 'print|port'
```

**Done.**

- `src/print.c` prints to a `sink`, a `FILE *` or a malloc'd buffer of code
  points.  `minim_port_write` and `minim_port_display` (public) print a file
  port straight to its `FILE *`, and a string port to the buffer, appended
  after the walk with `minim_port_write_chars` (`src/port.h`), which grows
  the port's string once.  `minim_port_write_char` is that with one char.
- `minim_get_output_string` is public, for C; `get-output-string` calls it.
- `write`, `display`, and `write-simple` moved from `src/system.c`'s
  primitives to `scheme/ports.scm`, over `%write` and `%display`.
- The REPL still prints its results with `minim_write(stdout, ...)`: the
  standard output port's stream, so the same order, and still working if
  that port is closed.
- `test_print` checks that printing to a string port gives exactly what
  printing to a file does, `write` and `display`, for data up to 1000
  chars; under stress, the append collects.
- Ran `ctest -R 'print|port|repl|script'`, and stress `print|port`.

### Phase 6 - File helpers

- `scheme/ports.scm`: `call-with-port`, `call-with-input-file`,
  `call-with-output-file`, `with-input-from-file`, `with-output-to-file`
  (the last two by `parameterize`), and the primitive `delete-file`.
  Exports to `(scheme base)` and `(scheme file)`.  `scheme/prelude.boot`
  regenerated.

Separate because they are conveniences over Phases 1 to 5.

Tests (`tests/scripts/`): writing a file with `with-output-to-file`,
reading it back with `call-with-input-file`, then `delete-file`;
`call-with-port` closes the port after its procedure returns.

```sh
cmake --build --preset dev && ctest --preset dev -R 'port|script' && ctest --preset stress -R 'port'
```

**Done.**

- `call-with-port` returns its procedure's values, all of them, after
  closing the port; if the procedure escapes, the port stays open (R7RS).
  The other four are `call-with-port` over a new file port, the `with-`
  ones parameterizing the current port inside it.
- `delete-file` is a primitive in `src/system.c`, with `file-exists?`:
  `remove`, raising a file error when it fails.
- `tests/scripts/file-ports.scm` writes, reads back, and deletes a file in
  `tests/scripts/`; its stress copy, which `ctest -j` may run at the same
  time, uses its own file, chosen by `MINIM_GC_NURSERY_BYTES`.
- Ran `ctest -R 'port|script'`, and stress `port`.

### After the last phase

Sync the docs: `ports.md` (new, listed in `docs/design/README.md`: the port
object, standard and current ports, the reader's source, the printer's
sink), `repl.md` (the REPL on ports), `gc.md` (the layout, destructors), `primitives.md`
(the port primitives), `prelude.md` (`ports.scm`: the current ports, the
helpers), and the roadmaps (`r7rs-small.md`:
milestone 6, the coverage table for `(scheme base)`, `(scheme file)`,
`(scheme read)`, and `(scheme write)`, and cycles, `write-shared`, and
datum labels under milestone 9; `beyond-r7rs.md`: binary ports with
bytevectors).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

None: the discussion settled them.  File ports are closed when collected
(Phase 2), stdio stays underneath, `write` waits for cycle detection, and the
REPL prints to the standard output port.

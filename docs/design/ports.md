# Ports

Textual ports (R7RS 6.13) over files and strings, in `src/port.c`, with the
procedures over them in `scheme/ports.scm`.  Binary ports wait for
bytevectors.

## The object

A port is a typed object (`gc.md`), after Chez Scheme's:

```
port  [ header | buffer | name | index | lookahead | file ]  48 B
```

- The header's length bits hold the flags, as Chez's type word does: input
  or output, file or string, and closed.
- `buffer`: a string port's string.  An input port reads it from `index`; an
  output one appends at `index`, the length so far, doubling the string when
  it is full.
- `name`: the path, `stdin`, `stdout`, `stderr`, or `string`, for printing:
  `#<input port load/defs.scm>`, `#<output port stdout>`.
- `lookahead`: a code point read but not consumed, or the end of input: for
  `peek-char` and the reader's pushback.
- `file`: a file port's `FILE *`, not traced, NULL once closed.

`buffer` and `name` are traced.  Unlike Chez's, a port has no handler
record: file and string ports are two cases in C, and R7RS has no
input/output ports, so one buffer suffices.

File ports use stdio's buffering, so C's own writes to `stdout` stay in
order with theirs.  So `char-ready?` can't tell whether input is waiting:
it is always true, and a read from a terminal may block.  Files open through
`minim_fopen` (`fopen_s` on MSVC); failing to open is a file error.

A file port is closed when the collection that finds it dead runs its
destructor (`gc.md`), as Chez's are by a guardian; closing it explicitly
first leaves the destructor nothing to do.  Closing a standard port flushes
it but leaves C's stream open.

The C interface, in `minim.h`: the predicates and accessors,
`minim_standard_port`, the `minim_open_*` constructors, `minim_port_close`,
`minim_port_read_char`, `minim_port_peek_char`, `minim_port_write_char`,
`minim_get_output_string`, `minim_read`, `minim_port_write`, and
`minim_port_display`.

## Standard and current ports

The standard input, output, and error ports, over `stdin`, `stdout`, and
`stderr`, are made at startup.  `current-input-port`, `current-output-port`,
and `current-error-port` are parameters, initially those, accepting only
ports of their direction.  Procedures whose port argument is optional are
Scheme wrappers defaulting it to the current port, over primitives taking it
(`%read-char`, `%write`, ...), since a primitive can't call a parameter.

The REPL reads through the standard input port, so it shares its lookahead
with `read` and `read-char`: `(read-line)` at the REPL reads the rest of its
own line.  It prints results to `stdout` itself, as Chez's REPL prints to the
console, so parameterizing `current-output-port` captures what a form
displays, not its printed value.

## Reading and printing

The reader reads from a source: a port, through a reference its caller
roots, or a C string, for the boot file.  It pushes back into the port's
lookahead.  `minim_read` raises its errors as read errors (`read-error?`).

The printer never allocates while it walks a datum.  It writes to a sink: a
file port's `FILE *` directly, or, for a string port, a malloc'd buffer of
code points appended to the port after the walk.  `write` is `write-simple`:
cyclic data loops forever.

## Procedures

`(scheme base)`: `port?`, `input-port?`, `output-port?`, `textual-port?`,
`binary-port?` (always false), `input-port-open?`, `output-port-open?`,
`close-port`, `close-input-port`, `close-output-port`, `open-input-string`
(reading a copy), `open-output-string`, `get-output-string` (which, unlike
Chez's, doesn't reset the port), the current ports, `read-char`,
`peek-char`, `read-line` (ending at a linefeed, a carriage return, or both),
`read-string`, `char-ready?`, `write-char`, `write-string`, `newline`,
`flush-output-port`, `eof-object`, `eof-object?`, and `call-with-port`,
which closes the port when its procedure returns.

`(scheme file)`: `open-input-file`, `open-output-file`,
`call-with-input-file`, `call-with-output-file`, `with-input-from-file`,
`with-output-to-file` (by `parameterize`), `delete-file`, and
`file-exists?`.  `(scheme read)`: `read`.  `(scheme write)`: `write`,
`display`, and `write-simple`.

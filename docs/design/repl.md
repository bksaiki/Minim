# REPL: reader and printer

`minim` reads expressions from stdin, evaluates them, and prints the
results (see `eval.md`).  It reads through ports (`ports.md`).

## Reader

`minim_read(port)` returns the next datum from an input port, or `MINIM_EOF`
at the end of input.  It is recursive descent over UTF-8 input and reports
bad syntax as a read error.  Internally the reader reads from a *source*, a
port or a C string; `minim_read_string` (`src/read.h`) reads from a C string
and advances it.

| Syntax | Reads as |
|---|---|
| `(a b)`, `(a . b)`, `()` | lists, dotted pairs, the empty list |
| `#(a b)` | vector |
| `42`, `-7`, `+5` | fixnum; out-of-range literals are errors |
| `"..."` | string, with `\"`, `\\`, `\n`, `\t`, and `\xHH;` escapes |
| `#\a`, `#\space`, `#\newline`, `#\tab`, `#\nul`, `#\xHH` | char |
| `#t`, `#f`, `#true`, `#false` | boolean |
| `'x`, `` `x ``, `,x`, `,@x` | `(quote x)`, `(quasiquote x)`, `(unquote x)`, `(unquote-splicing x)`; whitespace may follow, but `,@` is one token |
| anything else up to a delimiter | symbol (case-sensitive), including `+`, `-`, `...`, and `1.5` |

Delimiters are whitespace, `(`, `)`, `"`, and `;`.  `;` starts a comment that
runs to the end of the line, `#|` one to the matching `|#`, nesting, and
`#;` comments out the next datum.  Comments may stand wherever whitespace
may, before a `)` too.

The reader keeps no state between calls: it only ever pushes back one
character, into a port's lookahead, or, in a C string, an ASCII delimiter.

## Printer

`minim_write(FILE *out, minim_obj x)` prints `write` syntax, and
`minim_port_write(port, x)` likewise to a port.  Neither allocates while it
walks `x`, so `minim_write` is safe on error paths.

| Value | Printed |
|---|---|
| fixnum | decimal |
| `#t` / `#f`, `'()` | `#t` / `#f`, `()` |
| void / eof / unbound | `#<void>` / `#<eof>` / `#<unbound>` |
| char | `#\a`; named chars by name; other control chars as `#\x7f` |
| string | `"..."` with `\"`, `\\`, `\n`, `\t`; other control chars as `\x1;` |
| symbol | its name |
| pair | `(a b . c)`; `(quote x)` as `'x`, and likewise `` ` ``, `,`, `,@`, also as a tail: `(a . ,x)`.  `(unquote @x)` isn't abbreviated, since `,@x` would read back differently |
| vector | `#(a b)` |
| procedure | `#<procedure car>` with its name; `#<procedure>` for an unnamed closure |

Output is UTF-8.  Everything except the `#<...>` forms reads back as equal
data.  Cyclic data loops forever.

`minim_display` is the same, except that strings and chars, including nested
ones, print as their characters: `(1 "a" #\b)` displays as `(1 a b)`.

## REPL

`src/main.c`'s command line:

| Command | Runs |
|---|---|
| `minim` | the REPL |
| `minim file.scm arg ...` | the script `file.scm`, by `(load "file.scm")` |
| `minim --program file.scm arg ...` | the R7RS program `file.scm` |
| `minim --help`, `minim --version` | prints the usage or the version (`project` in `CMakeLists.txt`) |

Options come before the file; the arguments after it are the script's or
program's, even those that look like options, for `command-line`, which
gives the file's path and then them.  A script prints nothing else; an error
is printed to stderr with exit status 1.  A bad command line prints the
problem and the usage to stderr and exits with status 2.

The REPL and scripts expand forms in the interaction environment, which sees
`(minim)`; a program sees only what it imports (`macros.md`).

The REPL loops: read from the standard input port, `minim_eval_values`, then
write each value and a newline to `stdout`, skipping void ones.  A read or
evaluation error prints `error: msg: irritant` to stderr (`errors.md`), after
the `after`s of the `dynamic-wind`s it left run, and reading continues with
the rest of the input.  It ends at the end of its input, or at `(exit)`.

The `> ` prompt goes to stderr, and only when stdin is a terminal, as in
bash and Python, so piped runs print only results to stdout, as the REPL
tests expect (`testing.md`).

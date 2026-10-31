# REPL: reader and printer

`minim` reads expressions from stdin, evaluates them, and prints the
results (see `eval.md`).  All I/O is through `FILE *`; there are no port
objects yet.

## Reader

`minim_read(FILE *in)` returns the next datum, or `MINIM_EOF` at the end of
input.  It is recursive descent over UTF-8 input and reports bad syntax with
`minim_error`.  Internally the reader reads from a *source*, a file or a
string; `minim_read_string` (`src/read.h`) reads from a string and advances
it.

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
runs to the end of the line.

The reader keeps no state between calls: it only ever pushes back a single
ASCII delimiter, which `ungetc` supports.

## Printer

`minim_write(FILE *out, minim_obj x)` prints `write` syntax.  It never
allocates, so it is safe on error paths.

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
| procedure | `#<procedure>`, or `#<procedure car>` for a primitive |

Output is UTF-8.  Everything except the `#<...>` forms reads back as equal
data.  Cyclic data loops forever.

`minim_display` is the same, except that strings and chars, including nested
ones, print as their characters: `(1 "a" #\b)` displays as `(1 a b)`.

## REPL

With a file argument, `src/main.c` evaluates `(load "file")`.  It prints
nothing else, and an error is printed to stderr with exit status 1.  More
than one argument prints a usage message and exits with status 2.

Otherwise it runs the REPL, which loops: read, `minim_eval`, then write the
value and a newline unless it is void.  A read or evaluation error prints
`error: msg: irritant` to stderr (`errors.md`), and reading continues with the
rest of the input.

The `> ` prompt goes to stderr, and only when stdin is a terminal, as in
bash and Python, so piped runs print only results to stdout, as the REPL
tests expect (`testing.md`).

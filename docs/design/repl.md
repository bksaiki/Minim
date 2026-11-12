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
| `#u8(0 255)` | bytevector, of fixnums 0 to 255 |
| `42`, `-7`, `+5`, `#xff`, `#e1e3` | fixnum; out-of-range literals are errors |
| `1.5`, `.5`, `1e10`, `#i3`, `+inf.0`, `-inf.0`, `+nan.0` (in any case) | flonum |
| `"..."` | string, with `\"`, `\\`, `\|`, `\a`, `\b`, `\n`, `\r`, `\t`, and `\xHH;` escapes, and line continuations (`\`, then a line ending, between intraline white space) |
| `#\a`, `#\space`, `#\xHH`; R7RS's names (`#\null`, `#\alarm`, ...) and Chez's `#\nul` | char |
| `#t`, `#f`, `#true`, `#false`, in any case | boolean |
| `#0=(a . #0#)`, `(#1="s" #1#)` | datum labels (R7RS 2.4): `#n=` labels the datum after it, which `#n#` refers to, within one outermost datum.  An undefined, duplicate, or self-referring label is an error |
| `'x`, `` `x ``, `,x`, `,@x` | `(quote x)`, `(quasiquote x)`, `(unquote x)`, `(unquote-splicing x)`; whitespace may follow, but `,@` is one token |
| `\|a b\|` | symbol, named exactly, with the string escapes; never a number |
| anything else up to a delimiter | symbol, including `+`, `-`, `...`, and `1/2`; case-sensitive unless folding |

A number may start with a radix prefix, `#x` `#o` `#b` `#d`, and an
exactness prefix, `#e` `#i`, in either order; decimals are radix 10 only, and
`#e` of a non-integer is an error, there being no rationals.
`minim_parse_number` (`src/number.h`) parses numbers for the reader and
`string->number`; decimals go through `strtod`, which rounds correctly.

Delimiters are whitespace, `(`, `)`, `"`, `;`, and `|`
(`minim_is_delimiter`).  `;` starts a comment that runs to the end of the
line, `#|` one to the matching `|#`, nesting, and `#;` comments out the next
datum.  Comments may stand wherever whitespace may, before a `)` too, and so
may the directives `#!fold-case` and `#!no-fold-case`.  They turn folding on
and off for the rest of a port (`MINIM_PORT_FOLD_CASE`, `ports.md`), or of
one `minim_read_string`.  While folding, identifiers and character names
(`#\SPACE`) get Unicode's full case folding, as `string-foldcase`;
`|...|` symbols, strings, and character literals (`#\A`) don't.

Besides the fold flag, the reader keeps no state between calls: it only ever
pushes back one character, into a port's lookahead, or, in a C string, an
ASCII delimiter.

## Printer

`minim_write(FILE *out, minim_obj x)` prints `write` syntax, and
`minim_port_write(port, x)` likewise to a port.  Neither allocates while it
walks `x`, so `minim_write` is safe on error paths.

| Value | Printed |
|---|---|
| fixnum | decimal |
| flonum | the fewest digits that read back as it: `100.0`, `0.001`, `1e21`, `1.5e-7` (positional for exponents -6 to 20), `-0.0`, `+inf.0`, `+nan.0` |
| `#t` / `#f`, `'()` | `#t` / `#f`, `()` |
| void / eof / unbound | `#<void>` / `#<eof>` / `#<unbound>` |
| char | `#\a`; R7RS's named chars by name (`#\null`, `#\delete`, ...); other control chars as `#\x1` |
| string | `"..."` with `\"`, `\\`, `\a`, `\b`, `\n`, `\r`, `\t`; other control chars as `\x1;` |
| symbol | its name, or, if that wouldn't read back as it, in bars with the strings' escapes and `\|`: the empty name, `.`, a name that parses as a number, `+i`, `-i`, one with a delimiter, `\`, or a control character, or starting with `#`, `'`, `` ` ``, or `,`.  Non-ASCII names are bare |
| pair | `(a b . c)`; `(quote x)` as `'x`, and likewise `` ` ``, `,`, `,@`, also as a tail: `(a . ,x)`.  `(unquote @x)` isn't abbreviated, since `,@x` would read back differently, nor is a form whose second pair is labeled |
| vector | `#(a b)` |
| bytevector | `#u8(0 255)` |
| procedure | `#<procedure car>` with its name; `#<procedure>` for an unnamed closure |

Output is UTF-8.  Everything except the `#<...>` forms reads back as equal
data.

Datum labels: before printing a pair or vector, the printer walks it once,
with an explicit stack, marking each object while its contents are walked
(`minim_marks`).  One reached again while marked is on a cycle, and gets a
label: printed `#n=` before its datum the first time, numbered from 0 in
print order, and `#n#` after.  A labeled tail prints dotted, `(1 .
#0=(2 . #0#))`.  Every cycle has such an object, so printing terminates.
`write-shared` labels every pair and vector reached twice, and
`write-simple` skips the walk.

`minim_display` is the same, labels included, except that strings, chars,
and symbols, including nested ones, print as their characters:
`(1 "a" #\b |c d|)` displays as `(1 a b c d)`.

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

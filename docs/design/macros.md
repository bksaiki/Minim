# Macros and expansion

The expander is Scheme code in `scheme/expander.scm`, applied through
`%expand` (`scheme/eval.scm`).  It rewrites
each top-level form into the core forms the evaluator knows: `quote`, `if`,
`define`, `set!`, `lambda`, `case-lambda`, `begin`, `define-values`,
`let-values`, and
`letrec-values` (`eval.md`).  The evaluator calls it on every top-level form
once it is defined.  Startup loads it already expanded (`prelude.md`), so
it is written in ordinary Scheme, using the derived forms
(`scheme/derived.scm`) as macros and the procedures of the files that load
before it.  Libraries are in `scheme/libraries.scm`, the standard ones in
`scheme/standard.scm`.

It is written in Scheme because expansion is list processing, which in C
would need `MINIM_FRAME` around every allocation.  Each top-level form is
expanded just before it runs, so a `define-syntax` applies to the forms after
it.  The expander's procedures and variables start with `%`, out of the way
of user names.

Hygiene uses renaming (Clinger and Rees, "Macros That Work"): every local
variable gets a fresh uninterned name, and every identifier a macro inserts
is an *alias* (`primitives.md`), which remembers its original identifier and
the environment where the macro was defined.

## Syntactic environments

A *syntactic environment* says what identifiers mean.  It is a list of frames,
innermost first, ending in a top-level environment; a frame is a one-element
list holding an association list of `(identifier . meaning)`, so a body can
add to its frame as it scans.  A variable's meaning is its fresh name
(`(gensym "x")`, printing as `x`); a local macro's is its transformer.

A *top-level environment* is `#(kind parent own names)`, after Chez Scheme.
An identifier's binding in it is the identifier's property keyed by the
environment (`(%getprop id top #f)`): a *label*, the global whose value slot
is the variable, or a macro's transformer.  So a lookup reads one short list,
whatever the environment's size.  `own` lists the labels the environment
made; `names`, the identifiers it binds.  Expanded code names only labels, so
the runtime knows no environments and a global reference stays one load.

| Kind | For | Parent |
|---|---|---|
| `system` | the prelude and the expander | none |
| `interaction` | the REPL and scripts (`%expand`) | none; it imports `(minim)` |
| `library` | a `define-library` body, or a program | none |

Importing `(minim system)` makes the system environment the parent.

**Resolving an identifier:**

1. If a frame binds it, that meaning.
2. Otherwise, if the top-level environment, or its parent, binds it, that.
3. Otherwise, for an alias, its original resolved in the alias's environment.
4. Otherwise, if the environment's parents end in the system's, its own
   label.
5. Otherwise the environment's own label for it: a fresh one, made and
   recorded the first time, as Chez keeps one label per name and
   environment.  So a reference and a later definition agree, and a
   reference raises `unbound variable` if it runs first.

A form is a special form only when its head resolves to that label, so a
local variable named `if` is just a variable, and a macro's `if` is the
special form even where the user has bound `if`.

## Expansion

| Form | Expansion |
|---|---|
| identifier | its resolution; `bad syntax` if that is a macro |
| constant | itself, with aliases stripped from vectors |
| `(quote d)` | `d` with every alias replaced by its original (`%strip`), so cyclic literals work |
| `(quasiquote t)` | code building `t`; see below |
| `(lambda params body ...)` | fresh names for `params`, then the body in a new frame |
| `(case-lambda (params body ...) ...)` | each clause as `lambda`'s parameters and body |
| `(let-values ((formals init) ...) body ...)` | fresh names for every formal in a new frame, the inits expanded outside it, then the body |
| `(letrec-values ...)` | likewise, the inits expanded in the new frame |
| `(if ...)`, `(set! x e)`, `(begin ...)`, `(f arg ...)` | subforms expanded |
| `(let-syntax ((k spec) ...) body ...)` | `(let-values () body ...)`, with each `k` bound in a new frame to a transformer closed over the outer environment |
| `(letrec-syntax ...)` | likewise, the transformers closed over the new frame |
| `(syntax-error msg arg ...)` | raises `msg` with the arguments, at expansion time |
| `(k ...)`, `k` a macro | the transformer's result, expanded again |
| `(define ...)`, `(define-values ...)`, `(define-syntax ...)` | only at top level or at the start of a body; elsewhere `bad syntax` |

**Procedure definitions.**  `(define (f . formals) body ...)` is `(define f
(lambda formals body ...))`, at top level and in bodies (R7RS 5.3); the
`lambda` is an alias for the special form, so a local `lambda` can't capture
it.

**Top level.**  A `begin` is spliced and scanned as a library body is
(`%expand-top-forms`): every definition and macro is bound before any
right-hand side or expression is expanded, so a macro's definitions may
refer to ones after them.  `import` and `define-library` run as the scan
meets them.  The forms then run in order.

- `(define x e)` defines a label for `x`: the environment's own, if `x`
  already has one (for an alias, only one already defined); otherwise `x`
  itself, but a fresh global if `x` would shadow an imported binding, if the
  environment is a library's, or if `x` is an alias inserted by a macro, so
  only that use's insertions of `x` resolve to it (R7RS 5.3.1).
  The system environment's own labels are those holding a value.
  `(define-values formals e)` defines each of its formals the same way.
- `(define-syntax k spec)` binds `k` to its transformer and expands to
  `(begin)`; `%define-macro!` binds a system macro.
- `(set! x e)` of an imported binding is a syntax error, `set!: immutable
  variable`: the system's bindings can't be changed from the REPL.

**Bodies** (R7RS 5.4): the leading forms are scanned first.  Each is expanded
at its head until its head isn't a macro use; a `begin` is spliced; a
`define` or `define-values` binds its names to fresh names in the body's
frame, and a `define-syntax` binds its macro there at once.  The first other
form ends the scan.  Then each definition's expression and the remaining
forms are expanded in the body's frame, so they all see every definition.
The definitions become one `letrec-values` around the remaining forms, a
`define` a clause with one name.  A body with no other form is `bad
syntax`.

## `syntax-rules`

The only transformer a program can write: `(syntax-rules (literal ...)
rule ...)`, or `(syntax-rules ellipsis (literal ...) rule ...)` with a
custom ellipsis (R7RS 4.3.2).  It becomes a procedure from a use and its environment to the
use's expansion, closed over the definition environment.

**Matching.**  Rules are tried in order, ignoring the keyword position; none
matching is `bad syntax`.

- The ellipsis, `_`, and literals are recognized by binding, not name: a
  literal matches an input identifier that resolves, in the use's
  environment, as the literal does in the definition's.  An ellipsis among
  the literals is a literal, in patterns and templates alike.
- An ellipsis may be nested and followed by more patterns.  Dotted and vector
  patterns match accordingly; other data match by `equal?`.
- A pattern variable is bound to its depth and match; at depth n > 0 the
  match is a list of depth n - 1 matches.

**Instantiation.**  A pattern variable is replaced by its match.  Every other
identifier becomes an alias, one per name per use, so a binding the template
introduces can't capture the user's identifiers, and a free one means what it
meant where the macro was defined.  `x ...` repeats over `x`'s match, `x ...
...` flattens a level, and `(... template)` escapes the ellipsis.  A variable
at the wrong depth, or ellipsis variables of different lengths, are `bad
syntax`.

## Derived forms

`let` (with named `let`), `let*`, `let*-values`, `letrec`, `letrec*`, `and`,
`or`, `when`, `unless`, `cond` (with `else` and `=>`), `case` (with `else`
and `=>`), `do`, and `parameterize`, as `syntax-rules` macros after R7RS
7.3.  `let` expands to `let-values`, and `letrec` and `letrec*` both to
`letrec-values`, so both have `letrec*` semantics.  `case` calls the
prelude's `memv`, and `parameterize` its `%parameterize` (`prelude.md`).

`guard` (R7RS 4.2.7) expands into a call of the prelude's `%guard`
(`prelude.md`) on `(lambda () body ...)` and `(lambda (var reraise) (cond
clause ... (else (reraise))))`, without the added `else` when the last clause
is one.  The clauses run in the `guard`'s continuation, so a call from one is
a tail call.

`define-record-type` (R7RS 5.5) binds the type's name to a new rtd
(`gc.md`), as Chez does, and the constructor, predicate, accessors, and
modifiers to procedures from the prelude's helpers (`prelude.md`).  The
constructor is `(lambda (field ...) ...)`, so a wrong count is `arity
mismatch`; the fields it doesn't name are `#f`.  A record prints as `#<name>`
and an rtd as `#<record-type name>`, without one pair of angle brackets
around `name`.

## Quasiquote

`` `t `` (R7RS 4.2.8) becomes calls to `cons`, `append`, and `list->vector`,
which build `t`, with `quote` for its constant parts; a template with nothing
to evaluate becomes one `quote`.  Unquoted expressions are evaluated left to
right.  Each nested quasiquote raises the depth by one and each `unquote`
lowers it; only depth 0 is evaluated.  `,@` outside a list element is `bad
syntax`.  The code refers to the global `cons`, `append`, `apply`, and
`vector`.

## Errors

Malformed forms raise `bad syntax` when expanded, before any of the form
runs, with the smallest enclosing form as the irritant.  Both they and
`syntax-error` raise through the primitive `%syntax-error`.

## Transformers in Scheme

A macro's binding is a procedure of the form and its environment, so the
system binds some directly, with `%top-bind!` in `libraries.scm`, where
they must consult the platform or read files.  Each expands to the
system's `begin` around forms, which splices definitions at top level and
at the start of a body.

- `(cond-expand clause ...)` (R7RS 4.2.1): the forms of the first clause
  whose requirement holds, or of a last `else` clause; none matching raises
  `cond-expand: no matching clause`.  A requirement, stripped of aliases, is
  a feature identifier, in `(features)` (`primitives.md`); `(library
  name)`, the library defined or its file found, without loading it; or
  `and`, `or`, or `not` of requirements.
- `(include file ...)` and `(include-ci file ...)` (R7RS 4.1.7): the files'
  forms, `include-ci` reading as if after `#!fold-case`.  Paths are relative
  to the library file's directory while one loads, else to the current
  directory.

## Libraries

`(define-library name decl ...)` (R7RS 5.6) makes a library environment.
Its declarations: `export`, of a name or `(rename name external)`; `import`;
`begin`; `include` and `include-ci`, of files read relative to the library
file's directory; and `include-library-declarations` and `cond-expand`,
whose files' forms, or chosen clause's, are processed as declarations in
their place.  Its body is scanned as a top-level `begin`, every definition
bound to a fresh label, except that `import` and `define-library` in it are
expressions, not declarations.  The form registers the exports, `(name .
binding)`, in `%libraries`, and expands to its body, which runs in order
when the form does.

`(import set ...)`, at top level or in a library, binds each name the import
sets name to the exporting library's binding: the same label, so the same
variable, or the same transformer.  The sets are a library's name, `only`,
`except`, `prefix`, and `rename`.

A library not yet defined is loaded from its file, `(foo bar)` from
`foo/bar.sld`, in the current directory or one `MINIM_LIBRARY_PATH` lists
(`environment.md`), once.

The standard libraries, `(scheme base)`, `(scheme file)`, `(scheme load)`,
`(scheme process-context)`, `(scheme write)`, and `(scheme r5rs)`, export the
system's bindings of R7RS's names that Minim has, each its own label
(`%define-system-library`).  `(scheme r5rs)` adds the auxiliary syntax
R7RS's list omits.

After Chez Scheme, two system libraries cover the rest:

- `(minim)`, after `(chezscheme)`: every public system binding, the
  standard libraries' names and Minim's own (`collect`, `gensym`, `fold`,
  `import`, `define-library`, ...).
- `(minim system)`, after Chez's `$system`: every system binding, internals
  included.  It isn't a list of exports: importing it makes the system
  environment the parent, so it is imported only whole; inside `only`,
  `except`, `prefix`, or `rename` it is a syntax error.  For tests and code
  that extends Minim.

## The REPL, scripts, and programs

The interaction environment binds exactly `(minim)`, its other imports, and
its own definitions.  So internals are unreachable from the REPL: `%expand`
gets the REPL's own label, which is unbound.  System macros still expand into
internals, since their aliases resolve in the system environment.  A user's
definition lives in the REPL's label, not necessarily the interned symbol of
its name; C defines a global user code sees with `minim_define(name,
value)`, which lets the expander pick the label.  C itself still finds
`%expand` and `%raise-c-error` by symbol value.

A script, `minim file.scm` or `load`, runs in the interaction environment.
A program (R7RS 5.1), `minim --program file.scm`, after Chez's `--program`,
must begin with `import`; `%expand-program` expands the rest as a library
body, in a new environment of the library kind holding only what it
imports.

## Eval

`eval` and its environments (R7RS 6.12) are in `scheme/eval.scm`.  An
environment specifier is a record, `#<environment>`, holding a top-level
environment out of user code's reach, and whether it is mutable.

- `interaction-environment` is a parameter, as in Chez and Chibi, initially
  the REPL's specifier, the only mutable one.  The REPL, `load`, and
  `minim_define` expand in its current environment, through `%expand`, so
  `parameterize` redirects them.  It may be set to any specifier, as in
  Chez; set to an immutable one at the REPL, outside `parameterize`, it
  strands the REPL there, since nothing in it reaches the REPL's specifier.
- `(environment set ...)` is a new environment of the library kind holding
  what the import sets name.  Its bindings are immutable, so specifiers are
  cached by their sets (`equal?`): a new one costs about 2 ms and leaves its
  bindings on interned symbols' property lists for good, slowing every later
  lookup of those names.
- `(scheme-report-environment 5)` imports `(scheme r5rs)`;
  `(null-environment 5)`, only its syntax: the exports bound to macros or to
  labels holding no value.
- `(eval x env)` expands `x` in `env` (`%expand-in`), as a top-level form if
  it is mutable and as an expression otherwise, so a definition in an
  immutable environment is `bad syntax`.  Then it calls `%eval-expanded` on
  the result, in tail position (`primitives.md`).

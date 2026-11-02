# Macros and expansion

The expander, `%expand`, is Scheme code in `src/expander.scm`.  It rewrites
each top-level form into the core forms the evaluator knows: `quote`, `if`,
`define`, `set!`, `lambda`, `begin`, `define-values`, `let-values`, and
`letrec-values` (`eval.md`).  The evaluator calls it on every top-level form
once it is defined.  Startup loads it already expanded (`prelude.md`), so
it is written in ordinary Scheme, using the derived forms that follow it in
the file as macros, and the list library (`src/prelude.scm`), which loads
before it.

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

A *top-level environment* is `#(kind bindings parent own)`, after Chez
Scheme: `bindings` maps identifiers to a *label*, the global whose value slot
is the variable, or to a macro's transformer; `own` lists the labels its
definitions made.  Expanded code names only labels, so the runtime knows no
environments and a global reference stays one load.

| Kind | For | An identifier it doesn't bind |
|---|---|---|
| `system` | the prelude and the expander | is its own label |
| `interaction` | the REPL and files (`%expand`) | means what it does in the system environment |
| `library` | a `define-library` body | gets a fresh label, never bound |

**Resolving an identifier:**

1. If a frame binds it, that meaning.
2. Otherwise, if the top-level environment, or its parent, binds it, that.
3. Otherwise, for an alias, its original resolved in the alias's environment.
4. Otherwise its own label; in a library, a fresh one, so a reference raises
   `unbound variable` when it runs.

A form is a special form only when its head resolves to that label, so a
local variable named `if` is just a variable, and a macro's `if` is the
special form even where the user has bound `if`.

## Expansion

| Form | Expansion |
|---|---|
| identifier | its resolution; `bad syntax` if that is a macro |
| constant | itself, with aliases stripped from vectors |
| `(quote d)` | `d` with every alias replaced by its original |
| `(quasiquote t)` | code building `t`; see below |
| `(lambda params body ...)` | fresh names for `params`, then the body in a new frame |
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

**Top level.**  A `begin` is spliced: each of its forms is expanded as a
top-level form, in order.

- `(define x e)` defines a label for `x`: the environment's own, if `x`
  already has one; otherwise `x` itself, but a fresh global if `x` would
  shadow an imported binding (in the interaction environment, a system one),
  if the environment is a library's, or if `x` is an alias inserted by a
  macro, so only that use's insertions of `x` resolve to it (R7RS 5.3.1).
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

The only transformer: `(syntax-rules (literal ...) rule ...)`, or
`(syntax-rules ellipsis (literal ...) rule ...)` with a custom ellipsis
(R7RS 4.3.2).  It becomes a procedure from a use and its environment to the
use's expansion, closed over the definition environment.

**Matching.**  Rules are tried in order, ignoring the keyword position; none
matching is `bad syntax`.

- The ellipsis, `_`, and literals are recognized by binding, not name: a
  literal matches an input identifier that resolves, in the use's
  environment, as the literal does in the definition's.
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
and `=>`), and `do`, as `syntax-rules` macros after R7RS 7.3.  `let` expands
to `let-values`, and `letrec` and `letrec*` both to `letrec-values`, so both
have `letrec*` semantics.  `case` calls the prelude's `memv`.

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

## Libraries

`(define-library name decl ...)` (R7RS 5.6) makes a library environment.
Its declarations: `export`, of a name or `(rename name external)`; `import`;
`begin`; and `include`, of files read relative to the library file's
directory.  Its body is scanned like a body: every definition is bound, to a
fresh label, before any form is expanded, so one may refer to one further
down.  The form registers the exports, `(name . binding)`, in `%libraries`,
and expands to its body, which runs in order when the form does.

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
R7RS's list omits.  The REPL and files see every system binding before any
`import`.

# Macros and expansion

The expander, `%expand`, is Scheme code at the start of `src/prelude.scm`.  It
rewrites each top-level form into the core forms the evaluator knows:
`quote`, `if`, `define`, `set!`, `lambda`, `begin`, `define-values`,
`let-values`, and `letrec-values` (`eval.md`).  The
evaluator calls it on every top-level form once it is defined; it is defined
last in its section, so its own code runs unexpanded.  The derived forms come
right after it, as macros.

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
innermost first; a frame is a one-element list holding an association list of
`(identifier . meaning)`, so a body can add to its frame as it scans.  A
variable's meaning is its fresh name (`(gensym "x")`, printing as `x`); a
local macro's is its transformer.  The global environment is `'()`.

**Resolving an identifier:**

1. If the environment binds it, that meaning.
2. Otherwise, for an alias defined at top level as a fresh global (below),
   that global.
3. Otherwise, for an alias, its original resolved in the alias's environment.
4. Otherwise the symbol itself: a global variable, special form, or global
   macro (`%macros`, an association list).

A form is a special form only when its head resolves to that global, so a
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

**Top level.**  A `begin` is spliced: each of its forms is expanded as a
top-level form, in order.

- `(define x e)` defines `x`.  An alias `x`, inserted by a macro, defines its
  original if that already holds a value; otherwise a fresh global, recorded
  so that the same macro use's other insertions of `x` resolve to it while
  user code still sees `x` as its original (R7RS 5.3.1).  A definition removes
  a global macro of the same name.  `(define-values formals e)` defines each
  of its formals the same way.
- `(define-syntax k spec)` binds the global macro `k` (an alias's original)
  and expands to `(begin)`.

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

## Quasiquote

`` `t `` (R7RS 4.2.8) becomes calls to `cons`, `append`, and `apply vector`,
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

## Limitations

The expander calls ordinary globals (`car`, `list`, `not`, `gensym`, ...), so
redefining one at top level breaks expansion of every later form.  The
generated code for quasiquote and `case` likewise refers to the global
`cons`, `append`, `apply`, `vector`, and `memv`.

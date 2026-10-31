# Macros and expansion

The expander, `%expand`, is Scheme code at the start of `src/prelude.scm`. It
rewrites each top-level form into the core forms the evaluator knows:
`quote`, `if`, `define`, `set!`, `lambda`, and `begin` (see `eval.md`). The
evaluator calls it on every top-level form once it is defined. Because it is
defined last in its section, the expander's own code runs unexpanded, and
everything after it in the prelude is expanded.

Hygiene uses renaming (Clinger and Rees, "Macros That Work"):

- every local variable gets a fresh uninterned name;
- identifiers that a macro inserts are aliases (see `primitives.md`), which
  remember where the macro was defined.

## Syntactic environments

The expander tracks what each identifier means with a *syntactic
environment*:

- It is a list of frames, innermost first.
- Each frame is a one-element list holding an association list of
  `(identifier . meaning)`, so a body can add bindings to its frame as it
  finds them.
- A variable's meaning is its fresh name, `(gensym "x")`, which prints as `x`.
- The global environment is `'()`.

**Resolving an identifier** in an environment:

1. If the environment binds it, that binding's meaning.
2. Otherwise, if it is an alias, its original, resolved in the alias's
   environment.
3. Otherwise the symbol itself: a global variable, a special form, or a
   global macro.

A form is a special form or a macro use only when its head resolves to that
global. So a local variable named `if` is just a variable.

## Expansion

| Form | Expansion |
|---|---|
| identifier | its resolution; `bad syntax` if that is a macro |
| constant | itself, with aliases stripped from vectors |
| `(quote d)` | `d` with every alias replaced by its original |
| `` (quasiquote t) `` | code building `t`; see below |
| `(lambda params body ...)` | fresh names for `params`, then the body in a new frame |
| `(if ...)`, `(set! x e)`, `(begin ...)`, `(f arg ...)` | subforms expanded |
| `(define x e)` | only at top level or at the start of a body; elsewhere `bad syntax` |
| `(k ...)`, `k` a macro | the transformer's result, expanded again |

**Top level.** A `begin` is spliced: each of its forms is expanded as a
top-level form, in order. `(define x e)` keeps `x` and expands `e`.

**Bodies** (R7RS 5.4): first, the body's leading forms are scanned.

- Each form is expanded at its head until its head isn't a macro use.
- A `begin` is spliced into the rest of the body.
- Each `define` binds its name to a fresh name in the body's frame.
- The first other form ends the scan; the rest of the body is expressions.

Then every definition's expression and every remaining form is expanded in
the body's frame. So each definition, and the expressions after it, can see
all of the body's definitions. The definitions stay `define` forms, which the
evaluator binds in the procedure's frame.

**Quasiquote** (R7RS 4.2.8):

- **Building code.** `` `t `` becomes calls to `cons`, `append`, and
  `apply vector`, which build `t` at run time, and `quote` for its constant
  parts. Unquoted expressions are evaluated left to right.
- **Nesting.** Each nested quasiquote raises the nesting depth by one, and
  each `unquote` lowers it. Only parts at depth 0 are evaluated.
- **Constant folding.** A template with nothing to evaluate becomes a single
  `quote`.
- **Errors.** `,@` outside a list element (`` `,@x `` or `` `(a . ,@x) ``) is
  `bad syntax`. `,@` of a non-list raises in `append`.

The generated code refers to the global names `cons`, `append`, `apply`, and
`vector`, so redefining one of them at top level breaks quasiquote, as it
already breaks the prelude.

## Errors

Malformed forms raise `bad syntax`, with the smallest enclosing form as the
irritant, through the `%bad-syntax` primitive. These errors are now raised
when the form is expanded, before any of it runs. For example, defining a
procedure with a malformed body fails immediately, not when the procedure is
called.

## Cost

Release startup, which includes expanding the prelude, went from about 2.4 ms
to 10 ms. Expanding the prelude also non-tail recurses, so the value stack's
capacity starts at 128 rather than 64.

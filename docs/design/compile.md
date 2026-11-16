# Compiler: the core to the simplified core

The compiler (`scheme/compile.scm`) turns each top-level form, after
expansion, from the core forms (`macros.md`) into the simplified core, which
the evaluator (`eval.md`) runs unchecked.  It is Scheme, in small passes,
each from one language to the next; the roadmap (`roadmaps/compiler.md`) adds
passes between them.

## Languages

- **The core**: the expander's output: `quote`, `if`, `define`, `set!`,
  `lambda`, `case-lambda`, `begin`, `define-values`, `let-values`,
  `letrec-values`, applications, variables, and self-evaluating data.
- **Parsed**, `%parse`'s output: the core checked and normalized.

  ```
  e ::= x | (quote d) | (if e e e) | (begin e e e ...) | (set! x e)
      | (define x e) | (define-values formals e)
      | (let-values ((formals e) ...) e) | (letrec-values ((formals e) ...) e)
      | (lambda (formals e) ...) | (call e e ...)
  ```

  Every constant is quoted, every `if` has an else, `(begin)` is void and a
  one-form `begin` its form, a body is one expression, `lambda` and
  `case-lambda` are one form of clauses, and an application is `call`.
- **The simplified core**, `%emit`'s output: parsed, each `lambda` a
  `clambda`, each clause `(formals required rest? e)`, its arity computed
  once.

Nodes are lists with symbol heads.  `%parsed?` and `%simplified?` check a
form against each language, for tests.

## Passes

- `%parse` makes every check the evaluator makes on the core, raising `bad
  syntax` with the malformed form, and rejects definitions outside top level
  or a top-level `begin`.
- `%emit` computes each clause's `required` and `rest?`.

`%compile-form` is `(%emit (%parse x #t))`.

## The hook

`%compile` is a parameter, after Racket's `current-compile`: its value is the
compiler, `%compile-form` initially, or `#f`.  The evaluator applies it to
each top-level form (`eval.md`); with `#f` the form runs uncompiled, so
`(parameterize ((%compile #f)) ...)` scopes it and `(%compile #f)` switches
it off.  It is defined by `compile.scm`'s last form, so the prelude's own
forms run uncompiled; `minim --interpret` sets it to `#f` before anything
runs (`repl.md`).

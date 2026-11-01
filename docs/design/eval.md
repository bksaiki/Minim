# Evaluator: an explicit-stack machine

`minim_eval(expr)` evaluates `expr` in the global environment with an
explicit-stack machine, so Scheme recursion never uses the C stack and tail
calls run in constant space.  It returns the one value; `minim_eval_values`
returns the list of any number of values.  Primitive procedures are in
`primitives.md`.

## Closures

A closure (layout in `gc.md`) holds its lambda's `(params . body)` as `code`,
and the environment it was created in.

## Machine

The registers `expr`, `env`, `val`, and `k` are locals of `minim_eval`,
protected by `MINIM_FRAME`, so a catch restores them with the root stack.

`k` is the continuation: a chain of heap frames, vectors
`#(kind next code env base)`.  `'()` returns from `minim_eval`.

| Kind | `code` holds | On return |
|---|---|---|
| `IF` | branches `(then [else])` | evaluate a branch (tail position) |
| `SEQ` | remaining body | evaluate the next expression |
| `DEFINE` | symbol | bind it to the value |
| `SET` | symbol | set it to the value |
| `ARGS` | remaining operands | push the value; evaluate the next operand or apply |
| `EXPAND` | unused | evaluate the value, an expanded form, in the global environment |
| `TOPLEVEL` | remaining top-level forms | run the next form, or return void |
| `CALL_VALUES` | unused | apply the consumer, at `base`, to the values |
| `DEFINE_VALUES` | formals | bind them to the values |
| `LET_VALUES` | `(clauses . body)` | bind the first clause's formals in the new frame (`base`); evaluate the next init, or the body |

**Multiple values.**  A register, `nvals`, counts the values being returned:
one is in `val`, any other count is atop the value stack.  `SEQ` and
`TOPLEVEL` frames (a non-final form) discard any count; `CALL_VALUES`,
`DEFINE_VALUES`, and `LET_VALUES` take any count; every other frame takes
exactly one, as does `minim_eval`, otherwise `wrong number of values`, with
the list of values as the irritant.

**Value stack.**  `minim_gc.values` (in `gc.h`) holds the operator and
evaluated operands of calls in progress, and multiple values being returned.
An `ARGS` frame records `base`, the height where its call's values start.
The collector scans `[0, len)` and updates entries in place, so a pointer
into the stack survives allocation, but not a push, which may `realloc`.
`minim_catch` restores the height after an error (`errors.md`).  The stack
keeps its peak capacity: non-tail recursion like the prelude's `map` leaves
it large for the rest of the session.

## Top-level forms

`minim_eval`'s argument and each form of a loaded file are *top-level forms*.
If the global `%expand` holds a value, a top-level form is first passed to it
and the result is evaluated instead; otherwise the form is evaluated as is.
The call goes through the machine, so `%expand` may be a closure.  A file's
forms are read up front but expanded one at a time, each after the previous
one has run, so a `%expand` defined by one form applies to the next.

## Environments

`src/env.h` hides the representation: `minim_env_extend`, `minim_env_lookup`,
`minim_env_define`, and `minim_env_set`.

- `'()` is the global environment, which uses symbol value slots;
  `MINIM_UNBOUND` means unbound.
- A local environment is a pair `(bindings . parent)` whose bindings are an
  association list.

## Special forms

`quote`, `if`, `define`, `set!`, `lambda`, `begin`, `define-values`,
`let-values`, and `letrec-values`, recognized by the operator symbol itself.
The expander (`macros.md`) renames local variables, so a local named `if`
never reaches the evaluator as `if`, and it rejects malformed forms and
misplaced definitions before they run.  The checks below remain for forms
evaluated before the expander is defined.

- `define` takes only `(define name expr)`.  At top level it binds globally;
  in a procedure body it prepends to the innermost frame.  It is accepted
  anywhere an expression is.  Expanded code defines only at top level, since
  the expander turns bodies into `letrec-values`; internal definitions are
  for the expander's own code, which runs unexpanded.
- `lambda` takes fixed parameters, a rest parameter `(a . rest)`, or a single
  symbol for all arguments, and at least one body expression.  The `formals`
  of the `-values` forms take the same shapes.
- `(define-values formals expr)` binds the formals to `expr`'s values as
  `define` binds a name.
- `(let-values ((formals init) ...) body ...)` evaluates the inits left to
  right in the outer environment, binding each clause's formals in a new
  frame as its values arrive, then the body in that frame.  `letrec-values`
  evaluates the inits in the new frame, so each sees the clauses before it
  (`letrec*` semantics).
- Everything except `#f` is true.

Malformed forms raise `bad syntax`.

## Application

The machine pushes the operator, then each operand left to right.  With `n`
operands:

- A primitive gets `fn(&stack[base + 1], n)` after an arity check.  `apply` and
  `load` are the exceptions; see `primitives.md`.
- A closure binds its parameters from the same slots with
  `minim_env_extend`, then evaluates its body.  The call's values are popped
  first, and a call in tail position pushes no frame.

Errors: `unbound variable`, `not a procedure`, `arity mismatch`,
`wrong number of values`, and `bad syntax`.

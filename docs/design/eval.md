# Evaluator: an explicit-stack machine

`minim_eval(expr)` evaluates `expr` in the global environment with an
explicit-stack machine, so Scheme recursion never uses the C stack and tail
calls run in constant space.  Primitive procedures are in `primitives.md`.

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

**Value stack.**  `minim_gc.values` (in `gc.h`) holds the operator and evaluated
operands of calls in progress.  An `ARGS` frame records `base`, the height
where its call's values start.  The collector scans `[0, len)` and updates
entries in place, so a pointer into the stack survives allocation, but not a
push, which may `realloc`.  `minim_catch` restores the height after an error (`errors.md`).
The stack keeps its peak capacity: non-tail recursion like the prelude's `map`
leaves it large for the rest of the session.

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

`quote`, `if`, `define`, `set!`, `lambda`, and `begin`, recognized by the
operator symbol itself.  The expander (`macros.md`) renames local variables,
so a local named `if` never reaches the evaluator as `if`, and it rejects
malformed forms and misplaced `define`s before they run.  The checks below
remain for forms evaluated before the expander is defined.

- `define` takes only `(define name expr)`.  At top level it binds globally;
  in a procedure body it prepends to the innermost frame.  It is accepted
  anywhere an expression is.
- `lambda` takes fixed parameters, a rest parameter `(a . rest)`, or a single
  symbol for all arguments, and at least one body expression.
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

Errors: `unbound variable`, `not a procedure`, `arity mismatch`, and
`bad syntax`.

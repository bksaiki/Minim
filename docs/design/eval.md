# Evaluator: an explicit-stack machine

`minim_eval(expr)` evaluates `expr` in the global environment with an
explicit-stack machine, so Scheme recursion never uses the C stack and tail
calls run in constant space.  Primitive procedures are in `primitives.md`.

## Closures

```
closure    (tag 101)   [ code : (params . body) | env ]       16 B
```

A closure's `code` is its lambda's `(params . body)`.  Closures share the pair
to-space, since both are two traced words.

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

**Value stack.**  `minim_values` (in `gc.c`) holds the operator and evaluated
operands of calls in progress.  An `ARGS` frame records `base`, the height
where its call's values start.  The collector scans `[0, len)` and updates
entries in place, so a pointer into the stack survives allocation, but not a
push, which may `realloc`.  `minim_catch` restores the height after an error.
The stack keeps its peak capacity: non-tail recursion like the prelude's `map`
leaves it large for the rest of the session.

## Environments

`src/env.h` hides the representation: `minim_env_extend`, `minim_env_lookup`,
`minim_env_define`, and `minim_env_set`.

- `'()` is the global environment, which uses symbol value slots;
  `MINIM_UNBOUND` means unbound.
- A local environment is a pair `(bindings . parent)` whose bindings are an
  association list.

## Special forms

`quote`, `if`, `define`, `set!`, `lambda`, and `begin`, recognized by the
operator symbol itself, so local bindings can't shadow them.

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

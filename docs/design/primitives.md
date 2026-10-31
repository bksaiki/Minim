# Primitives

Primitive procedures are C functions, bound as global symbol values by
`minim_prims_init` (`src/prims.c`).  The evaluator calls them; see `eval.md`.

## Representation

```
primitive  (tag 111)   [ header | fn | name | min | max ]     40 B, rounded up to 48
```

- `fn` is `minim_obj fn(minim_obj *args, size_t n)` and isn't traced.  `args`
  points into the value stack, which the collector updates in place, so it
  stays valid across allocation.  Primitives must not push onto the value
  stack.
- `name` is a symbol, used for printing (`#<procedure car>`).
- `min` and `max` are fixnum arities; `max` is `MINIM_VARIADIC` (-1) for any
  number of arguments.  The evaluator checks them before calling `fn`.

## `apply`

`apply` is registered as a primitive, but the evaluator recognizes its
function pointer (`minim_prim_apply`) and never calls it.  Instead, it rewrites
the value stack `[apply f a ... lst]` into `[f a ... elements of lst]` and
applies `f`, so `apply` in tail position is a tail call.  An ordinary
primitive couldn't call a closure without re-entering `minim_eval`.

## Library

### Fixnums

| Call | Result |
|---|---|
| `(+ n ...)` | sum; `0` with no arguments |
| `(- n)`, `(- n m ...)` | negation, or `n` minus the rest |
| `(* n ...)` | product; `1` with no arguments |
| `(quotient n m)` | `n / m`, truncated toward zero |
| `(remainder n m)` | remainder with the sign of `n` |
| `(= n ...)` | whether all are equal |
| `(< n ...)`, `(> ...)`, `(<= ...)`, `(>= ...)` | whether each adjacent pair is in order |
| `(fixnum? x)` | whether `x` is a fixnum |
| `(zero? n)` | whether `n` is 0 |

### Pairs and lists

| Call | Result |
|---|---|
| `(cons a d)` | new pair |
| `(car p)`, `(cdr p)` | first or second field of the pair `p` |
| `(set-car! p x)`, `(set-cdr! p x)` | sets a field of `p`; void |
| `(pair? x)` | whether `x` is a pair |
| `(null? x)` | whether `x` is `'()` |
| `(list x ...)` | new list of the arguments |
| `(length lst)` | number of elements in the proper list `lst` |
| `(apply f x ... lst)` | `f` applied to the `x`s and the elements of `lst` |

### Identity and types

| Call | Result |
|---|---|
| `(eq? a b)` | whether `a` and `b` are the same object |
| `(not x)` | whether `x` is `#f` |
| `(symbol? x)`, `(procedure? x)`, `(boolean? x)` | whether `x` is of that type |

### Vectors

| Call | Result |
|---|---|
| `(vector x ...)` | new vector of the arguments |
| `(make-vector k [fill])` | new vector of length `k`, filled with `fill` (default 0) |
| `(vector-ref v i)` | element `i` of `v` |
| `(vector-set! v i x)` | sets element `i` of `v`; void |
| `(vector-length v)` | length of `v` |
| `(vector? x)` | whether `x` is a vector |

### Strings and symbols

| Call | Result |
|---|---|
| `(string-length s)` | length of `s` |
| `(string-ref s i)` | char `i` of `s` |
| `(string? x)` | whether `x` is a string |
| `(symbol->string sym)` | the name of `sym`, an immutable string (not a copy) |
| `(string->symbol s)` | the symbol named `s` |

### Output

| Call | Result |
|---|---|
| `(write x)` | prints `x` in `write` syntax to stdout; void |
| `(display x)` | prints `x` with strings and chars unescaped; void |
| `(newline)` | prints a newline; void |

## Conventions

- Arithmetic that leaves the fixnum range raises `<name>: fixnum overflow`;
  dividing by zero raises `<name>: division by zero`.
- Type errors are `<name>: not a <type>`.
- Index errors are `<name>: bad index`, covering non-fixnum, negative, and
  out-of-range indices.
- Void results print nothing at the REPL.

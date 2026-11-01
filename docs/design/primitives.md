# Primitives

Primitive procedures are C functions, bound as global symbol values by
`minim_prims_init` (`src/prims.c`).  The evaluator calls them; see `eval.md`.

## `apply` and `load`

Both need to evaluate code, which an ordinary primitive can't do without
re-entering `minim_eval`.  So both are registered as primitives, but the
evaluator recognizes their function pointers (`minim_prim_apply`,
`minim_prim_load`) and never calls them:

- `apply` rewrites the value stack `[apply f a ... lst]` into
  `[f a ... elements of lst]` and applies `f`, so `apply` in tail position is
  a tail call.
- `load` reads the whole file with `minim_read_file` (`src/read.h`), which
  always closes it, even on a read error.  It then runs the forms as top-level
  forms (see `eval.md`) and returns void.  A read error therefore runs
  none of the file.

## Equality and hashing

- `equal?` walks pairs and vectors with an explicit stack, never C
  recursion, so arbitrarily deep structure is fine.  The stack is on the C
  stack until it outgrows 64 entries.
- A hash must agree with its equality, so `equal-hash` mirrors `equal?`:
  equal values visit the same values in the same order.  Its 64-value budget
  makes it O(1) and terminating on cyclic data.
- Every hash is stable under the moving collector: strings and symbols hash
  their chars (with a kind marker for symbols), primitives their C function
  pointer, and all closures one constant.  `eq?` hashing of heap objects, which
  needs a stable per-object identity, is left to hashtables (R7RS-small roadmap
  milestone 7).
- The FNV-1a step (`minim_hash_mix`, `minim_string_hash` in `minim.h`) is
  shared with the symbol table.

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

### Chars

| Call | Result |
|---|---|
| `(char? x)` | whether `x` is a char |
| `(char->integer c)` | the code point of `c` |
| `(integer->char k)` | the char with code point `k`, which must be a Unicode scalar value |
| `(char=? c ...)`, `(char<? ...)`, `(char>? ...)`, `(char<=? ...)`, `(char>=? ...)` | whether each adjacent pair is in order, by code point |

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
| `(eqv? a b)` | same as `eq?` (they will differ for flonums and bignums) |
| `(equal? a b)` | whether `a` and `b` have the same structure: pairs, vectors, and strings compared element by element, everything else by `eqv?`.  Cyclic data loops forever |
| `(equal-hash x)` | a non-negative fixnum, equal for `equal?` values.  Visits at most 64 values of `x`, so it terminates on cyclic data.  Closures all hash alike |
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
| `(string? x)` | whether `x` is a string |
| `(make-string k [c])` | new string of length `k`, filled with `c` (default `#\space`) |
| `(string-length s)` | length of `s` |
| `(string-ref s i)` | char `i` of `s` |
| `(string-set! s i c)` | sets char `i` of `s`, which must be mutable (symbol names aren't); void |
| `(string-append s ...)` | new string of the arguments' chars in order |
| `(substring s start end)` | new string of chars `start` through `end - 1` |
| `(string=? s ...)`, `(string<? ...)`, `(string>? ...)`, `(string<=? ...)`, `(string>=? ...)` | whether each adjacent pair is in order, lexicographically by code point |
| `(string->list s)` | list of the chars of `s` |
| `(list->string lst)` | new string of the chars in the list `lst` |
| `(symbol->string sym)` | the name of `sym`, an immutable string (not a copy) |
| `(string->symbol s)` | the symbol named `s` |
| `(gensym [s])` | a new uninterned symbol named `s` (default `g0`, `g1`, ...): it prints like its name but is `eq?` to no other symbol |
| `(string-hash s)` | a non-negative fixnum, equal for `string=?` strings |

### Expander support

For the macro expander in the prelude; the `%` marks them internal.

| Call | Result |
|---|---|
| `(%make-alias id env)` | a new alias: an uninterned symbol named like the symbol `id`, recording `id` and `env` |
| `(%alias-original x)`, `(%alias-env x)` | the `id` or `env` an alias was made with; `#f` if `x` isn't an alias |
| `(%bound? sym)` | whether the global `sym` holds a value; `#f` for an alias |
| `(%syntax-error message args)` | raises the string `message` with `args` as the irritant: void if empty, its element if it has one |

An alias stores `#(tag id env)` in its value slot, where `tag` is a private
uninterned symbol, so no value a program can build looks like an alias.

### Output

| Call | Result |
|---|---|
| `(write x)` | prints `x` in `write` syntax to stdout; void |
| `(display x)` | prints `x` with strings and chars unescaped; void |
| `(newline)` | prints a newline; void |

### Storage

After Chez Scheme's storage-management procedures; see `gc.md`.

| Call | Result |
|---|---|
| `(collect)` | runs the next scheduled collection; void |
| `(collect-nursery-bytes)`, `(collect-nursery-bytes n)` | bytes allocated between collections; `n` sets it, a positive fixnum |
| `(collect-generation-radix)`, `(collect-generation-radix r)` | the schedule's base: generation g is collected every `r`^g collections; `r` sets it, a positive fixnum |
| `(collect-maximum-generation)`, `(collect-maximum-generation g)` | the oldest generation; `g` sets it, from 1 to 8 |

### Files

| Call | Result |
|---|---|
| `(load path)` | runs the forms in the file `path`, relative to the current directory, as top-level forms; void |

## Conventions

- Arithmetic that leaves the fixnum range raises `<name>: fixnum overflow`;
  dividing by zero raises `<name>: division by zero`.
- Type errors are `<name>: not a <type>`.
- Index errors are `<name>: bad index`, covering non-fixnum, negative, and
  out-of-range indices.
- Strings are mutable except symbol names; `string-set!` on one raises
  `string-set!: immutable string`.  String literals are mutable, unlike R7RS.

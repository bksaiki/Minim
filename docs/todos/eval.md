# Evaluator: procedures, an explicit-stack machine, and primitives

Implementation plan.  The design is settled; what follows is the phase
breakdown, one phase per commit.

## Working policy

- **Pause after each phase for review.** Do not begin the next phase until the
  current one has been looked at.
- **Do not commit.** The author of the change leaves the working tree dirty;
  commits are made by the repository owner.
- **Run only the tests relevant to the phase.** The full suite runs once, at
  the end, after the last phase.
- **Comments stay succinct**, and notes about *process* (what was tried, what
  a phase decided, why an ordering was chosen) belong in this document, not in
  source comments.

## Context

The REPL (`docs/design/repl.md`) reads data, prints them back, and recovers
from errors with `minim_error` / `minim_catch`.  Nothing evaluates them.

This plan adds procedures, an evaluator, and a primitive library, so the REPL
evaluates what it reads:

```
> (define count (lambda (n) (if (= n 0) 'done (count (- n 1)))))
> (count 1000000)
done
> (car 5)
error: car: not a pair: 5
> 
```

Out of scope: macros and derived forms (`let`, `cond`, `and`, `or`),
`quasiquote`, `call/cc`, non-fixnum numbers, `equal?`, and file loading.

**Decisions:**

| Decision | Choice | Reason |
|---|---|---|
| Evaluator | explicit-stack machine; continuation frames on the heap | proper tail calls; `call/cc` possible later; state is a few GC roots, not C locals |
| Closures | primary tag `101`, 2 words, no header | reserved tag; fits the existing two-word scan |
| Primitives | typed objects holding a C function pointer | must live in the heap, since the collector copies every tagged pointer |
| Primitive arguments | an array on a value stack: `fn(minim_obj *args, size_t n)` | no allocation per call; `args[i]` access.  A future `call/cc` must copy the stack |
| Global environment | symbol value slots | already exist; `MINIM_UNBOUND` means unbound |
| Local environments | association lists behind `src/env.h` | simple now; a hash table can replace them without touching the evaluator |
| Special forms | recognized by operator symbol identity | simple; local bindings can't shadow them |

## Procedures

```
closure    (tag 101)   [ code : (params . body) | env ]       16 B
primitive  (tag 111)   [ header | fn | name | min | max ]     40 B → 48 B

primitive header = (0 << 4) | (immutable << 3) | 100
```

- Closures go in the pair to-space, which holds any two traced words.
- A primitive's `fn` is a raw `minim_obj (*)(minim_obj *args, size_t n)` and
  isn't traced; `name` (a symbol) is.  `min` and `max` are fixnum arities;
  `max` is `MINIM_VARIADIC` (-1) for any number of arguments.
- `minim_is_procedure` covers both kinds.
- They print as `#<procedure>`, or `#<procedure car>` for primitives.

## Evaluator

`minim_eval(minim_obj expr)` evaluates `expr` in the global environment.  The
machine's state is a set of registers, locals of `minim_eval` protected by
`MINIM_FRAME`:

| Register | Holds |
|---|---|
| `expr` | expression being evaluated |
| `env` | current environment |
| `val` | last value |
| `k` | continuation: a chain of heap frames |

Each frame is a vector `#(kind next ...saved)`, where `kind` is a fixnum.
Kinds: `IF` (branches, env), `SEQ` (remaining body, env), `DEFINE` and
`SET` (symbol, env), and `ARGS` (remaining operands, env, base).  `k = '()`
returns from `minim_eval`.

**Value stack.**  A growable C array of `minim_obj` in `gc.c`, scanned as
roots in `[0, sp)` and updated in place by the collector.  It holds the
operator and evaluated operands of calls in progress; an `ARGS` frame records
`base`, the stack height where its call's values start.  A pointer into the
stack stays valid across allocation, but not across a push, which may
`realloc`; primitives never push.

- **Environments.**  The evaluator uses only `src/env.h`: `minim_env_extend`
  (new frame from parameters and arguments), `minim_env_lookup`,
  `minim_env_define`, and `minim_env_set`.  A local environment is a pair
  `(bindings . parent)` whose bindings are an association list; `'()` is the
  global environment, which uses symbol value slots.  An internal `define`
  prepends to the innermost frame's bindings.
- **Special forms:** `quote`, `if`, `define` (only `(define name expr)`),
  `set!`, `lambda` (fixed and rest parameters), `begin`.  Their symbols
  are interned at init into C globals registered as roots.
- **Application** pushes the operator and then each operand, left to right.
  With `n = sp - base - 1` operands, a primitive gets `fn(&stack[base + 1],
  n)`; a closure binds its parameters from the same slots with
  `minim_env_extend`.  Either way the call's values are popped (`sp = base`)
  before the result or the body is used.  A call in tail position pushes no
  frame, so loops run in constant space.
- **Truthiness:** everything except `#f` is true.
- **Errors** (unbound variable, non-procedure call, arity mismatch, bad
  syntax) go through `minim_error`.  `minim_catch` also restores the
  value-stack height; the registers are restored with the root stack.

## Phases

### Phase 1 - Procedures

**Done.**  Diverged from the plan:

- Primitives store `min` and `max` arities as fixnum fields instead of one
  arity in the header, since Phase 3's `make-vector` takes 1 or 2 arguments.
  This makes them 40 B (48 B allocated) instead of 24 B.
- A closure's first field is `code` (accessor `minim_closure_code`).
- `MINIM_VARIADIC` (-1) is the `max` for any number of arguments; `minim_prim_fn`
  is the function type.
- `tests/test_procedure.c` also covers printing.  Its first stress run caught
  a test that read a field and then allocated in the same `CHECK_EQ`.
- `docs/design/gc.md` now lists the closure tag and both layouts.

- `include/minim.h`, `src/object.c`: `MINIM_TAG_CLOSURE`,
  `MINIM_TYPE_PRIMITIVE`, constructors, accessors, `minim_is_procedure`.
- `src/gc.c`: closures copy into the pair space; primitives scan only `name`.
- `src/print.c`: procedure printing.

Separate so the new layouts and collector changes are reviewed before the
evaluator depends on them.

Tests (`tests/test_procedure.c`, also in `stress`):

- constructors and accessors; predicates are disjoint from the other kinds;
- a closure and a primitive survive a collection with their fields intact,
  and a primitive's `fn` still works after it.

```sh
cmake --build --preset dev && ctest --preset dev -R procedure
cmake --build --preset stress && ctest --preset stress -R procedure
```

### Phase 2 - Evaluator

**Done.**  Diverged from the plan:

- The registers are locals of `minim_eval` in a `MINIM_FRAME`, not global
  roots.  A catch restores them with the root stack, so `error.c` knows
  nothing about the evaluator, and `minim_eval` can be re-entered later.
- There is no `HALT` frame: `k = '()` returns.  Frames are 5-slot vectors
  `#(kind next code env base)`.
- Special-form symbols join `minim_sym_quote` in `src/symbol.h`
  (`intern_global` interns each into a registered C global).
- The value stack has `minim_values_push`; popping assigns
  `minim_values.len`.
- `tests/repl/echo.scm` now quotes its data, since the REPL evaluates.
- The stress-mode tail loops run 10k iterations (`tests/repl/tail.scm` too,
  since a golden file can't vary by preset).  At 100k, `eval` and `repl_tail`
  took about 30 s each under stress; they now take about 3 s.  `dev` runs
  100k: at 1M it took 7 s, and 100k non-tail iterations (~17 MiB) still exceed
  the 8 MiB bound.
- The `dev` and `stress` presets compile with `-O1`, as AddressSanitizer
  recommends.  `-O0` made the evaluator about 10x slower; `test_eval` went
  from 7.2 s to 0.3 s.
- Checked by hand: 1M-deep non-tail recursion works (no C stack) and leaves a
  171 MiB heap, so the test's 8 MiB bound catches a missing tail call.

- `src/env.h`, `src/env.c`: environments.  `minim_env_extend(params, args,
  n, parent)` reads `args` by index after each allocation.
- `src/gc.h`, `src/gc.c`: the value stack (`minim_values_push`,
  `minim_values_pop`, the stack height), scanned during collection.
- `src/error.c`: `minim_catch` also saves and restores the value-stack height.
- `include/minim.h`, `src/eval.c`: the machine, special forms, and
  `minim_eval`.
- `src/prims.c`: fixnum `+`, `-`, `*`, `=`, `<` (overflow is an error), plus
  `cons`, `car`, `cdr`; installed as global symbol values by `minim_init`.
- `src/main.c`: the loop calls `minim_eval` and prints nothing for void.

Separate because it is the largest change; the primitive library waits for
Phase 3.

Tests:

- `tests/test_eval.c`, also in `stress`: each special form, closures
  capturing and mutating their environment, rest parameters, internal
  `define`, and each error kind through `minim_catch`.  After an error, the
  value stack is back at its height before the call.  A primitive that
  allocates before reading its later arguments still sees them correctly.
- `tests/repl/eval.scm`: arithmetic, `define`, closures (a counter), and
  error recovery.
- `tests/repl/tail.scm`: a 10k-iteration tail loop completes.
  `tests/test_eval.c` also checks a 100k-iteration loop keeps the heap
  bounded (10k under `stress`).

```sh
cmake --build --preset dev && ctest --preset dev -R 'eval|repl'
cmake --build --preset stress && ctest --preset stress -R 'eval|repl'
```

### Phase 3 - Primitive library

**Done.**  Details settled during the phase:

- The golden runner also compares stderr with `<name>.err` when that file
  exists, so `prims.err` checks each family's type and index errors.
- Type errors are `<name>: not a <type>`; index errors are `<name>: bad
  index`, covering non-fixnum, negative, and out-of-range indices.
- `make-vector` fills with 0 by default, as Chez does.
- `symbol->string` returns the symbol's immutable name, not a copy.
- Mutators, `write`, and `newline` return void.
- Later: `define` accepts only `(define name expr)`.  The procedure shorthand
  `(define (f . params) body ...)` was removed; write
  `(define f (lambda params body ...))`.

- `src/prims.c`:
  - pairs: `set-car!`, `set-cdr!`, `pair?`, `null?`, `list`;
  - `eq?`, `not`, `symbol?`, `procedure?`, `boolean?`;
  - vectors: `vector`, `make-vector`, `vector-ref`, `vector-set!`,
    `vector-length`, `vector?`;
  - strings: `string-length`, `string-ref`, `string?`, `symbol->string`,
    `string->symbol`;
  - `write`, `newline`.
- Each primitive checks its argument types with `minim_error`.

Separate from Phase 2 so the evaluator is reviewed without a long list of
primitives attached.

Tests (`tests/repl/prims.scm`): each primitive, plus a type error and an
index error for each family.

```sh
cmake --build --preset dev && ctest --preset dev -R repl
```

### After the last phase

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
cmake --preset stress && cmake --build --preset stress && ctest --preset stress
```

No linter is configured; the build must be warning-free.

## Open items

None.

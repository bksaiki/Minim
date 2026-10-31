# REPL: printer, reader, and an explicit-stack evaluator

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

The runtime has tagged values (fixnums, chars, booleans, `'()`, void, eof),
four heap kinds (pairs, vectors, strings, symbols), a precise copying
collector, and symbol interning (see `docs/design/gc.md`).  `src/main.c` still
prints "Hello, world!".  Nothing can print, read, or evaluate Scheme.

This plan adds, in order: a printer, a reader with a read-print loop, error
recovery, procedures, and an evaluator, ending with a REPL:

```
> (define (count n) (if (= n 0) 'done (count (- n 1))))
> (count 1000000)
done
> (car 5)
error: car: not a pair: 5
> 
```

Out of scope: macros and derived forms (`let`, `cond`, `and`, `or`),
`quasiquote`, `call/cc`, non-fixnum numbers, `equal?`,
port objects, file loading, line editing, and condition objects.

**Decisions:**

| Decision | Choice | Reason |
|---|---|---|
| Evaluator | explicit-stack machine; continuation frames on the heap | proper tail calls; `call/cc` possible later; state is a few GC roots, not C locals |
| Errors | `longjmp` to the innermost `minim_catch` | unwinds C primitives and the reader; restores the root stack |
| Input / output | `FILE *`; no port objects | ports can come later |
| Closures | primary tag `101`, 2 words, no header | reserved tag; fits the existing two-word scan |
| Primitives | typed objects holding a C function pointer | must live in the heap, since the collector copies every tagged pointer |
| Primitive arguments | an array on a value stack: `fn(minim_obj *args, size_t n)` | no allocation per call; `args[i]` access.  A future `call/cc` must copy the stack |
| Global environment | symbol value slots | already exist; `MINIM_UNBOUND` means unbound |
| Local environments | association lists behind `src/env.h` | simple now; a hash table can replace them without touching the evaluator |
| Special forms | recognized by operator symbol identity | simple; local bindings can't shadow them |
| REPL prompt | on stderr, only when stdin is a terminal (`isatty`) | the Unix convention (bash, Python); piped runs print only results |

## Printer

`minim_write(FILE *out, minim_obj x)` prints `write` syntax and never
allocates, so it is safe in error paths.

| Value | Printed |
|---|---|
| fixnum | decimal |
| `#t` / `#f`, `'()` | `#t` / `#f`, `()` |
| void / eof / unbound | `#<void>` / `#<eof>` / `#<unbound>` |
| char | `#\a`, `#\space`, `#\newline`, `#\tab`, `#\nul`, otherwise `#\xHH` if not printable |
| string | `"..."` with `\"`, `\\`, `\n`, `\t` escapes, UTF-8 encoded |
| symbol | name, UTF-8 encoded |
| pair | `(a b . c)`; `(quote x)` as `'x` |
| vector | `#(a b)` |
| procedure | `#<procedure>`, or `#<procedure car>` for primitives |

Cyclic data loops forever; datum labels are out of scope.

## Errors

```c
_Noreturn void minim_error(const char *msg, minim_obj irritant);
bool minim_catch(minim_obj (*fn)(void *), void *arg, minim_obj *result);
```

`minim_catch` saves the root-stack height and a `jmp_buf`, then calls `fn`.
`minim_error` records the message and irritant (a global root) and jumps to
the innermost catch, which restores the root-stack height, the value-stack
height (from Phase 5), and the evaluator registers, and returns `false`.  `minim_last_error(const char **msg, minim_obj
*irritant)` retrieves them.  Catches nest.  With no catch installed,
`minim_error` prints and aborts.

## Reader

`minim_read(FILE *in)` returns the next datum, or `MINIM_EOF`.  It is
recursive descent and reports errors with `minim_error`.

- Lists, including dotted pairs; vectors `#(...)`.
- Fixnums with an optional sign; out-of-range literals are errors.  `+`, `-`,
  and `...` are symbols.
- Strings with `\"`, `\\`, `\n`, `\t`, and `\xHH;` escapes.
- Chars: `#\a`, the names the printer uses, and `#\xHH`.
- `#t`, `#f`, `#true`, `#false`.
- Symbols are case-sensitive.
- `'x` reads as `(quote x)`.
- `;` comments.
- Input is UTF-8; the decoder in `object.c` becomes shared.

## Procedures

```
closure    (tag 101)   [ lambda : (params . body) | env ]   16 B
primitive  (tag 111)   [ header | fn | name ]               24 B → 32 B

primitive header = (arity << 4) | (immutable << 3) | 100
```

- Closures go in the pair to-space, which holds any two traced words.
- A primitive's `fn` is a raw `minim_obj (*)(minim_obj *args, size_t n)` and
  isn't traced; `name` (a symbol) is.  The header's length field holds the arity; `-1` (all
  bits set) means variadic.
- `minim_is_procedure` covers both kinds.

## Evaluator

`minim_eval(minim_obj expr)` evaluates `expr` in the global environment.  The
machine's state is a set of registers, all registered as global roots:

| Register | Holds |
|---|---|
| `expr` | expression being evaluated |
| `env` | current environment |
| `val` | last value |
| `k` | continuation: a chain of heap frames |

Each frame is a vector `#(kind next ...saved)`, where `kind` is a fixnum.
Kinds: `IF` (then, else, env), `SEQ` (remaining body, env), `DEFINE` and
`SET` (symbol, env), `ARGS` (remaining operands, env, base), and `HALT`.

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
- **Special forms:** `quote`, `if`, `define` (variable and `(define (f . args)
  ...)`), `set!`, `lambda` (fixed and rest parameters), `begin`.  Their symbols
  are interned at init into C globals registered as roots.
- **Application** pushes the operator and then each operand, left to right.
  With `n = sp - base - 1` operands, a primitive gets `fn(&stack[base + 1],
  n)`; a closure binds its parameters from the same slots with
  `minim_env_extend`.  Either way the call's values are popped (`sp = base`)
  before the result or the body is used.  A call in tail position pushes no
  frame, so loops run in constant space.
- **Truthiness:** everything except `#f` is true.
- **Errors** (unbound variable, non-procedure call, arity mismatch, bad
  syntax) go through `minim_error`.

## Phases

### Phase 1 - Printer

- `include/minim.h`, `src/print.c`: `minim_write`.
- `src/object.c`: split out a UTF-8 encoder (`minim_utf8_encode`, internal,
  in a new `src/utf8.h` with the existing decoder).

Separate because every later phase uses it for tests and debugging.

Tests (`tests/test_print.c`, writing to `tmpfile()` and reading it back):

- every row of the printer table, including dotted pairs, nested vectors,
  `'x`, string escapes, and non-ASCII chars;
- printing performs no allocation (`minim_heap.allocated` unchanged).

```sh
cmake --build --preset dev && ctest --preset dev -R print
```

### Phase 2 - Errors

- `include/minim.h`, `src/error.c`: `minim_error`, `minim_catch`,
  `minim_last_error`.

Separate because the reader, evaluator, and primitives all report errors
through it.

Tests (`tests/test_error.c`):

- `minim_catch` returns `true` and the result when `fn` returns;
- an error returns `false`, with the message and irritant retrievable;
- the root-stack height is restored after an error raised inside a frame;
- nested catches: the inner one catches; after it returns, the outer one is
  active again;
- the irritant survives a collection.

```sh
cmake --build --preset dev && ctest --preset dev -R error
```

### Phase 3 - Reader and read-print loop

- `include/minim.h`, `src/read.c`: `minim_read`.
- `src/main.c`: loop: prompt `> ` on stderr if stdin is a terminal
  (`isatty`, or `_isatty` on Windows), `minim_read` from stdin,
  `minim_write` to stdout, newline.  Exits on eof.  Errors print `error: msg:
  irritant` to stderr and the loop continues.
- `tests/run_repl.cmake`, `tests/repl/*.scm` with `.out` files: a golden-test
  runner that pipes each `.scm` file into `minim` and compares stdout.
  `minim_add_repl_test(name)` in `tests/CMakeLists.txt`.

Separate because it gives an end-to-end harness before the evaluator exists.

Tests:

- `tests/test_read.c` (reading from `tmpfile()`): each datum kind,
  round-tripped through the printer; errors for `)`, `(1 . )`, an eof inside
  a list or string, a bad `#` syntax, and an out-of-range fixnum.
- `tests/repl/echo.scm`: data echo back unchanged, and an error line is
  followed by further output.

```sh
cmake --build --preset dev && ctest --preset dev -R 'read|repl'
```

### Phase 4 - Procedures

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

### Phase 5 - Evaluator

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
Phase 6.

Tests:

- `tests/test_eval.c`, also in `stress`: each special form, closures
  capturing and mutating their environment, rest parameters, internal
  `define`, and each error kind through `minim_catch`.  After an error, the
  value stack is back at its height before the call.  A primitive that
  allocates before reading its later arguments still sees them correctly.
- `tests/repl/eval.scm`: arithmetic, `define`, closures (a counter), and
  error recovery.
- `tests/repl/tail.scm`: a 100k-iteration tail loop completes.
  `tests/test_eval.c` also checks a 1M-iteration loop keeps the heap bounded
  (100k under `stress`).

```sh
cmake --build --preset dev && ctest --preset dev -R 'eval|repl'
cmake --build --preset stress && ctest --preset stress -R 'eval|repl'
```

### Phase 6 - Primitive library

- `src/prims.c`:
  - pairs: `set-car!`, `set-cdr!`, `pair?`, `null?`, `list`;
  - `eq?`, `not`, `symbol?`, `procedure?`, `boolean?`;
  - vectors: `vector`, `make-vector`, `vector-ref`, `vector-set!`,
    `vector-length`, `vector?`;
  - strings: `string-length`, `string-ref`, `string?`, `symbol->string`,
    `string->symbol`;
  - `write`, `newline`.
- Each primitive checks its argument types with `minim_error`.

Separate from Phase 5 so the evaluator is reviewed without a long list of
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

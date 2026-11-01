# Multiple values: values and the binding forms

Implementation plan.  The design is settled; what follows is the phase
breakdown, one phase per commit.

This is the first of two plans.  It *adds* multiple values to the evaluator,
with `values`, `call-with-values`, and the core forms `define-values`,
`let-values`, and `letrec-values`.  The second *reduces* the core: the
expander rewrites bodies into `letrec-values` and `let` into `let-values`,
as in Racket's fully expanded programs, and the evaluator drops what that
makes unused (internal `define`).

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
- **Design docs are synced after the last phase**, not during the phases, so
  they never describe an intermediate state.  Each phase records its decisions
  in this document instead.
- **Design docs stay succinct**: only the details a reader needs to understand
  the current implementation, said in as few words as possible.

## Context

The evaluator (`src/eval.c`, `docs/design/eval.md`) is an explicit-stack
machine whose `val` register holds the one value every expression returns.
Its core forms are `quote`, `if`, `define`, `set!`, `lambda`, and `begin`;
the expander (`src/prelude.scm`, `macros.md`) rewrites everything else into
them.  Nothing returns more than one value:

| Input | Today |
|---|---|
| `(values 1 2)` | `unbound variable: values` |
| `(call-with-values (lambda () 1) list)` | `unbound variable: call-with-values` |
| `(define-values (a b) (values 1 2))` | `unbound variable: define-values` |
| `(let-values (((a b) (values 1 2))) a)` | `unbound variable: let-values` |

R7RS requires `values`, `call-with-values` (6.10), `let-values`,
`let*-values` (4.2.2), and `define-values` (5.3.3); `letrec-values` is
Racket's, needed by the second plan.

**Settled in discussion:**

- Multiple values live on the value stack with a count, not in a heap
  "values object", which `(list (values 1 2))` would leak, and which
  `call/cc` (`(k 1 2)`) won't want.
- A one-value continuation that receives another count is an error.
- The REPL prints each value on its own line, so `(values)` prints nothing.
- `define-values`, `let-values`, and `letrec-values` are evaluator core
  forms.  Top-level `define` stays core; the second plan may expand it.

## Multiple values in the machine

**Returns.**  A new register, `nvals`, counts the values being returned.
With 1, the value is in `val`, as now.  Otherwise the `nvals` values are the
top of the value stack.  The stack is otherwise where it was when the
receiving frame was pushed, so the values sit just above that frame's data.

**Continuations.**  On `ret`, by frame kind:

| Frame | Takes |
|---|---|
| `SEQ`, `TOPLEVEL` (a non-final form) | any count, discarded (as R6RS, Chez, Racket) |
| `CALL_VALUES`, `DEFINE_VALUES`, `LET_VALUES` | any count; see below |
| `'()` (return from `minim_eval`) | see *C API* |
| every other frame | exactly 1, otherwise `wrong number of values` |

The error's irritant is the list of values received.

**`values`** is a primitive the machine applies itself, as it does `apply`:
`[values a ...]` at `base` becomes the values `a ...` at `base`, with
`nvals` their count; one value goes to `val`.

**`(call-with-values producer consumer)`**, also applied by the machine,
rearranges `[call-with-values producer consumer]` at `base` into `[consumer
producer]`, pushes a `CALL_VALUES` frame whose `base` is the consumer's slot,
and applies the producer to no arguments.  The producer's values then return
right above the consumer (pushing `val` if `nvals` is 1), so the frame
applies the consumer at `base` with no copying.  The consumer's call is in
tail position.

**C API.**  `minim_eval(expr)` returns the one value and raises `wrong
number of values` for any other count.  A new `minim_eval_values(expr)`
returns the list of the values; the REPL uses it and prints each.

## Binding forms

`formals` is a lambda list: `(a b)`, `(a . rest)`, or `rest`.  A value count
the formals don't accept is `wrong number of values`.

- **`(define-values formals expr)`** binds each variable as `define` would:
  globally at top level, otherwise in the innermost frame.  Returns void.
- **`(let-values ((formals init) ...) body ...)`** evaluates the inits left to
  right in the outer environment, then the body in a new frame binding them
  all.
- **`(letrec-values ((formals init) ...) body ...)`** likewise, but the new
  frame comes first and each init is evaluated in it, binding its formals
  before the next init runs (`letrec*` semantics, as Racket's).

The let forms share one frame kind, `LET_VALUES`: `code` holds the pending
clauses and the body, `env` the environment the inits run in, and the slot
`base` holds the new frame (`env` itself for `letrec-values`).  Each clause's
values are bound into the new frame as they arrive, through
`minim_env_define`, so the value stack holds nothing across inits.  Malformed
forms are `bad syntax`, checked as for `lambda`.

## Expander

- `define-values`, at top level and at the start of a body, as `define` is:
  each variable through `%top-name` at top level, or `%bind-variable!` in a
  body.  Elsewhere, `bad syntax`.
- `let-values` and `letrec-values`: fresh names for every formal, the inits
  expanded in the outer environment or the new frame, and the body with
  `%expand-body`.
- `let*-values`, a `syntax-rules` macro among the derived forms (R7RS 7.3).

## Phases

### Phase 1 - Multiple values in the machine

**Done.**  As planned.  `minim_eval` and `minim_eval_values` share one
machine, `eval(expr, many)`.  The REPL still skips void values.  A loaded
file's forms returning several values are covered by
`tests/repl/load/values.scm`.  Release benchmarks unchanged against `main`
(2.7-2.8 s and 1.8-1.9 s for both, run alternately).

- `src/eval.c`: the `nvals` register; the checks on `ret` above; `values`
  and `call-with-values` applied in `apply`, with the `CALL_VALUES` frame.
  `minim_eval_values`, sharing the machine with `minim_eval`.
- `src/prims.c`, `src/prims.h`: the `values` and `call-with-values`
  primitives, whose functions are never called (like `apply`'s).
- `include/minim.h`: `minim_eval_values`.
- `src/main.c`: the REPL prints each value from `minim_eval_values`.

Separate because the return protocol is the foundation the binding forms use,
and it touches every frame kind.

Tests:

- `tests/repl/values.scm` (new): `values` with 0, 1, and several values;
  `call-with-values` with primitives and closures as producer and consumer;
  `apply` of `values`; `values` as a first-class procedure (`(map values
  ...)`); discarded values in a non-final body form; the REPL printing each
  value, and nothing for `(values)`.  Errors (`values.err`): several values
  to an operand, an `if` test, `define`, and `set!`.
- `tests/test_eval.c`: `minim_eval` raises `wrong number of values` for 0 and
  2 values; `minim_eval_values` returns the list; a `call-with-values` loop
  runs in constant space (as `test_tail_calls`).
- Release benchmarks against `main` (`ret` gains a check).

```sh
cmake --build --preset dev && ctest --preset dev -R 'eval|values' && ctest --preset stress -R 'eval|values'
```

### Phase 2 - Binding forms in the evaluator

**Done.**  As planned.  `bind_values` binds formals to the values atop the
value stack for both frame kinds.  A new register, `frame`, holds the frame a
let form binds into between `let_values` steps.  Each clause after the first
allocates one pair, for the frame's `(clauses . body)`.  `test_binding_values`
also checks that the body is in tail position.  Injected bugs, each caught:
`let-values` inits evaluated in the new frame, and `define-values` refusing
several values.

- `src/eval.c`: `define-values`, `let-values`, and `letrec-values` as core
  forms, with the `DEFINE_VALUES` and `LET_VALUES` frames.
- `src/symbol.c`, `src/symbol.h`: their symbols.

Separate because it is machine code reviewable without the expander.  Until
Phase 3, the expander doesn't know the forms, so the tests run them with
`%expand` unbound.

Tests (`tests/test_eval.c`, with `%expand` unbound around them):

- each form with fixed, dotted, and rest formals; zero values; several
  clauses; `let-values` inits not seeing the new frame and `letrec-values`
  inits seeing earlier ones; `define-values` at top level and in a body;
- errors: a count the formals don't accept, and malformed forms (`bad
  syntax`).

```sh
cmake --build --preset dev && ctest --preset dev -R 'eval' && ctest --preset stress -R 'eval'
```

### Phase 3 - Expander

- `src/prelude.scm`: `%expand-form` handles `let-values` and
  `letrec-values`; `%expand` and `%expand-body` handle `define-values`;
  `let*-values` among the derived forms.

Separate because it is the Scheme side, after the forms run.

Tests (`tests/repl/values.scm`, `values.err`):

- each form end to end, including internal `define-values` mixed with
  `define`, and `let*-values`;
- hygiene: a macro that inserts a `let-values` binding doesn't capture the
  user's variable, and a user's `let-values` binding doesn't capture a
  macro's free identifier;
- errors: misplaced `define-values`, malformed formals, a formal that is a
  macro keyword.

```sh
cmake --build --preset dev && ctest --preset dev -R 'values|expand|derived' && ctest --preset stress -R 'values|expand|derived'
```

### After the last phase

Sync the docs: `eval.md` (the `nvals` protocol, the new frames and core
forms, `minim_eval_values`), `primitives.md` (`values`,
`call-with-values`), `macros.md` (the new forms, `define-values` in bodies,
`let*-values`), `repl.md` (one line per value), `errors.md` (`wrong number of
values`), and the roadmap (milestone 4: `values` and `call-with-values`
done).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

None.  Settled before Phase 1: C gets multiple values from
`minim_eval_values`, a list; `letrec-values` keeps Racket's name and in-order
semantics, visible to users, and a reference to a variable whose init hasn't
run is `unbound variable`.

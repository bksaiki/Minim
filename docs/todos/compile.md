# Compile: a vector-based tree and lexically addressed frames

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
- **Design docs are synced after the last phase**, not during the phases, so
  they never describe an intermediate state.  Each phase records its decisions
  in this document instead.
- **Design docs stay succinct**: only the details a reader needs to understand
  the current implementation, said in as few words as possible.

## Context

The evaluator (`src/eval.c`, `eval.md`) runs expanded core forms as lists.
Every time it evaluates a form, it re-checks it: `list_length`, the head
compared against `quote`, `if`, `define`, ..., `valid_params` for a `lambda`,
`valid_clauses` for a `let-values`.  Variables are looked up by name
(`src/env.c`): a local environment is a chain of frames, each an association
list, searched symbol by symbol, then the symbol's global value slot.

Since the last two PRs, a local frame comes only from a procedure call,
`let-values`, or `letrec-values`, and its variables are known when it is
created: definitions bind only globals.  So every environment is the global
one or a chain of fixed-size frames, and each local variable could be found by
position instead of by name.

## Design

**Compile to a vector-based tree.**  A compiler (`src/compile.c`) turns each
expanded top-level form into a tree of nodes, once, before it runs.  A node
is a vector whose slot 0 is its kind, a fixnum; its other slots are fixed by
the kind:

| Kind | Slots | From |
|---|---|---|
| `CONST` | value | `quote`, self-evaluating data |
| `REF` | symbol | a variable |
| `SET` | symbol, value | `set!` |
| `DEFINE` | symbol, value | top-level `define` |
| `DEFINE_VALUES` | formals, value | top-level `define-values` |
| `IF` | test, then, else | `if`; a missing else is `CONST` void |
| `LAMBDA` | formals, body | `lambda`; a body of several forms is a `SEQ` |
| `SEQ` | node ... | `begin`, bodies |
| `CALL` | operator, operand ... | an application |
| `LET_VALUES` | rec?, body, then formals and init per clause | `let-values`, `letrec-values` |

The nodes are ordinary heap vectors, so the collector needs nothing new.  A
closure's code becomes its `LAMBDA` node.  The boot file stays core forms,
compiled as it loads.

- **Where it runs**: in `minim_eval`, `minim_eval_values`, and
  `minim_eval_expanded`, on each top-level form after expansion (if any), and
  so on each form `load` runs.
- **What it checks**: everything the evaluator checks today, with the same
  `bad syntax` errors and irritants, reported once, before the form runs.
  The evaluator stops checking.
- **The machine** keeps its shape: continuation frames point at nodes instead
  of list fragments.  An `IF` frame holds the `IF` node; a `SEQ` frame the
  `SEQ` node and the next index; an `ARGS` frame the `CALL` node, the next
  operand's index being the value-stack height above the call's base; a
  `LET_VALUES` frame the node, the new frame, and the next clause's index.

**Then vector frames and lexical addresses.**  A local frame becomes a
vector, `#(parent v0 v1 ...)`, one allocation, filled from the value stack for
a call.  The compiler tracks the frames in scope and resolves each variable:

| Kind | Slots |
|---|---|
| `LOCAL_REF`, `LOCAL_SET` | frames up, slot, symbol (for errors), and the value for a set |
| `GLOBAL_REF`, `GLOBAL_SET` | symbol, and the value for a set |
| `LAMBDA` | required count, rest?, body, names |
| `LET_VALUES` | rec?, frame size, body, then first slot, count, rest?, and init per clause |

A `letrec-values` frame's slots start as `MINIM_UNBOUND`; reading one raises
`unbound variable` with the node's symbol.  Globals stay symbol value slots
(libraries will revisit them).

**Later, not here**: flat closures, frames merged per procedure call, and
boxes for assigned variables, decided by the measurements below.

## Phases

### Phase 1 - Compile to a vector-based tree

- `src/compile.c`, `src/compile.h` (new): `minim_compile(expr)`, with the
  checks moved from `src/eval.c`.
- `src/eval.c`: the machine dispatches on node kinds; its frames hold nodes;
  `toplevel` compiles each form after expansion; closures hold `LAMBDA`
  nodes; `list_length`, `valid_params`, and `valid_clauses` move to the
  compiler.
- Environments unchanged.

Separate because it changes how code is represented and run, with behaviour
identical, so the full suite is its regression net.

Tests: the full suite, unchanged, including `test_eval`'s `bad syntax`
checks with the expander off, which the compiler now raises.  A deeply nested
expression (see Open items).  Measure against `main`, run alternately:
release, the allocation programs, a call-heavy loop (`fib`), a `let`-heavy
loop, startup; and `ctest --preset stress -j 8`.

```sh
cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
```

### Phase 2 - Vector frames and lexical addresses

- `src/compile.c`: the compile-time scope; `LOCAL_*` and `GLOBAL_*` nodes;
  frame sizes and slots in `LAMBDA` and `LET_VALUES`.
- `src/env.c`, `src/env.h`: frames as vectors, accessed by address; lookup by
  name goes.
- `src/eval.c`: calls fill a frame from the value stack; `bind_values` writes
  slots; the `UNBOUND` check on local reads.

Separate because it changes the environment representation, measurable on
its own against Phase 1.

Tests: the full suite; `test_eval`: references across several frames, `set!`
of a local in an outer frame, a `letrec-values` variable read before its
init (`unbound variable` naming it), rest parameters.  The same measurements
against Phase 1.

```sh
cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
```

### After the last phase

Sync the docs: `eval.md` (the compiler, the node kinds, the machine on nodes,
frames and addresses), `gc.md` (a closure's code is a `LAMBDA` node, its
environment a frame vector), `errors.md` if any message changes, and the
roadmap (milestone 9: environments done; flattening as a later option).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

None.  Settled before Phase 1: the compiler rejects a definition anywhere
but top level or a top-level `begin`, since it knows the position for free;
and it recurses on the C stack, with a test that 10,000-deep nesting
compiles under ASan.

# Continuations: a segmented value stack, call/cc, and dynamic-wind

Implementation plan.  The design is settled; what follows is the phase
breakdown, one phase per commit.

Errors (`raise`, `guard`, ...) are a separate, later plan, which this one
prepares for: `guard` is built from continuations and `dynamic-wind`.

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

R7RS requires `call-with-current-continuation` (`call/cc`) and
`dynamic-wind` (6.10); Minim has neither (roadmap milestone 4).

The evaluator's state (`eval.md`) is two things:

- **The continuation `k`**, a chain of heap frames, never mutated once made.
  Capturing it is keeping a pointer.
- **The value stack** (`minim_gc.values`), one growable C array holding the
  operators and operands of calls in progress, and values being returned.
  Frames refer to it by height (an `ARGS` frame's `base`).

A continuation needs both.  Copying the value stack on every capture costs
time and memory in proportion to its height, which deep non-tail recursion
makes large.  Chez Scheme avoids the equivalent cost for its control stack by
segmenting it (Hieb, Dybvig, and Bruggeman, "Representing Control in the
Presence of First-Class Continuations", 1990); Minim's frames are already on
the heap, so only the value stack needs segmenting.

## Design

**The segmented value stack.**  The stack is a writable C buffer on top of a
chain of frozen segments:

```
frozen segment (heap vector, never mutated):

  #( prev   start   v0   v1   ...   vn-1 )
     │      │       └── the values at logical heights start .. start+n-1
     │      └── fixnum: logical height of v0
     └── the next older frozen segment, or '()
```

```c
struct minim_value_stack {
    minim_obj *items;  // capacity MINIM_SEGMENT_SIZE, never reallocated
    size_t len;        // values in the buffer
    size_t start;      // logical height of items[0]
    minim_obj below;   // the newest frozen segment, or '()
};
```

- **Sizes.**  `MINIM_SEGMENT_SIZE` is 8,192 values (64 KiB), about Chez
  Scheme's stack (`s/cmacros.ss`: four 16 KiB segments), and
  `MINIM_MAX_ARGS` is 8,191, Gambit's and Vicare's limit; Chicken 4 had
  2,048, and Racket and Chibi have none (survey:
  docs.scheme.org/surveys/apply-args-limit).
- **Heights** are logical, `start + len` at the top, so frames keep plain
  heights.  The buffer never moves, so a primitive's `args` pointer stays
  valid across allocation, as now.
- **Copy-out.**  Before a call's operator is pushed, if fewer than
  `MINIM_MAX_ARGS + 1` slots remain, the buffer's values become a new frozen
  segment, `below` points to it, and the buffer restarts empty at the same
  logical height.  So a call's values never straddle two segments.  A call
  written out with more than `MINIM_MAX_ARGS` operands, in source or by a
  macro, raises `too many arguments`.
- **`apply` is exempt.**  Applied to a closure, it binds the parameters
  straight from the list, a rest parameter getting a copy of the remaining
  tail, so the stack isn't involved.  Applied to a primitive with more
  arguments than fit, it spreads them into a temporary C array, a collector
  root for the call, since a primitive takes a C array.  `values` and a
  continuation still return at most `MINIM_MAX_ARGS` values, since values
  are returned on the stack; more raises `too many arguments`.
- **Underflow.**  On returning to a frame whose height is below `start`, the
  segment `below` is copied back into the buffer, and `start` and `below` are
  taken from its slots.  The frozen copy stays as it was, so anything sharing
  it never sees a write.  Values being returned on top of the buffer are set
  aside across the reload.
- **The collector** sees frozen segments as vectors, and scans the buffer and
  `below` as roots.  Nothing about it changes.
- **`minim_catch`** copies the buffer out on entry, unless it is empty, and
  on an error restores `start`, `len`, and `below`.  With the buffer empty at
  entry, nothing a continuation did inside can have corrupted what is
  restored.

**Continuation objects.**  A new typed object, `[header | k | segment]`.  It
is a procedure (`procedure?`), prints as `#<continuation>`, and is traced
like a vector's elements.

- **`(%call/cc f)`**, a primitive the machine applies itself as it does
  `apply`, freezes the buffer's values into a new segment *without* emptying
  the buffer, makes a continuation of `k` and that segment, and applies `f`
  to it in tail position.  Capture costs at most one segment's copy,
  whatever the recursion depth.
- **Applying a continuation** to `n` arguments sets them aside, empties the
  buffer onto the captured segment (`below` = it, `start` = its end), sets
  `k`, and returns the arguments as `n` values.  The next return underflows
  into the captured values.  The current continuation is discarded, so a loop
  through continuations runs in constant space.
- **Delimited by `minim_eval`.**  A frame chain ends at `'()`, the return from
  `minim_eval`, so a continuation captured by one top-level form and invoked
  by a later one finishes the earlier form's work and returns from the current
  `minim_eval`: after `(+ 1 (call/cc (lambda (c) (set! k c) 1)))` prints `2`,
  `(k 10)` prints `11`.  Nothing re-enters `minim_eval` while the machine
  runs (primitives don't), so the stack belongs to one machine.

**Later, if measurements or programs call for it**: a call too big for the
buffer gets its own oversized buffer, as a large object gets its own chunk,
removing the cap; and an underflow copies only the returning frame's values,
splitting the frozen segment, as Chez copies only the top frame, instead of
the whole segment.

**`dynamic-wind`**, in the prelude (`src/prelude.scm`), after the R7RS
reference implementation:

- The *winders* list the `dynamic-wind` calls in progress, innermost first,
  as `(before . after)` pairs.
- `(dynamic-wind before thunk after)` calls `before`, pushes the pair, calls
  `thunk`, pops it, calls `after`, and returns `thunk`'s values.
- `call/cc` wraps `%call/cc`: the procedure it passes to `f` remembers the
  winders when captured and, when called, runs `after`s out to the common
  tail of the two lists and `before`s in to the remembered one, then applies
  the raw continuation.

**Errors** escape by `longjmp`, past any `dynamic-wind`, without running
`after`s; the errors plan changes that.  Until then `minim_catch` restores the
winders too, so a caught error leaves no stale pair for a later jump to run.

## Phases

### Phase 0 - Bounded applications in the prelude

**Done.**  `list->vector` checks for a proper list (`list->vector: not a
list`).  `%append-map` keeps `(apply append (map f lst))`: `append` is a
closure, so Phase 1's `apply` exemption leaves it uncapped, and one call
beats a call per element.  Quasiquote's generated
vector code now calls `list->vector`, so `macros.md`'s Limitations list
changes (`apply` and `vector` out, `list->vector` in).  `src/boot.scm` uses
`list->vector` too.  The boot file changed in 4 lines; the second
regeneration was identical.  `test_eval`'s `test_large_forms` builds both
forms as strings; it uses 100 elements under stress, where 10,000 took
minutes, and 10,000 otherwise (about 4 s under ASan, linear in size).

- `src/prims.c`: `list->vector` (R7RS 6.8).
- `src/expander.scm`: no `apply` to a list of unbounded length:
  `%append-map` appends as it goes; `%strip`, `instantiate`, and quasiquote's
  vector code use `list->vector`.  `src/prelude.boot` regenerated twice.

Separate because the cap in Phase 1 would otherwise break large macro uses
and vector literals.

Tests: `tests/repl/prims.scm`: `list->vector`.  `tests/repl/expander.scm`: a
quasiquoted vector and a `syntax-rules` ellipsis, each of 10,000 elements,
more than `MINIM_MAX_ARGS`, so they keep guarding once Phase 1 adds the cap.

```sh
cmake --build --preset dev && ctest --preset dev -R 'repl|boot' && ctest --preset stress -R 'repl'
```

### Phase 1 - Segmented value stack

**Done.**  Details settled during the phase:

- **A call reserves exactly its own values**, not `MINIM_MAX_ARGS + 1`: an
  application knows its operand count from the form's length.  With a
  segment of 8,192 and a cap of 8,191, reserving the cap would have frozen
  at nearly every call.  The cap is unchanged: a call must fit an empty
  buffer.
- **The module** is `src/stack.c` and `src/stack.h`.  The fast paths of
  push, reserve, and reload are inline, with freezing and reloading out of
  line; inlining cut the slowdown on call-heavy programs from 6-10% to about
  4%.
- **Heights never drift**: a freeze happens at the current height, a call is
  applied only with its values in the buffer, and only a resuming `ARGS`
  frame reloads, with the buffer then empty.
- **`call-with-values`**: a freeze while the producer ran leaves the consumer
  frozen just below the buffer; `minim_stack_include` copies it back down,
  without allocating, into the slot its `values` call reserved.  Reloads
  then ignore a segment's values the buffer already holds.
- **`minim_catch`** saves only the height and on an error unwinds to it,
  dropping and reloading segments (`minim_stack_unwind`).  Freezing at
  entry, as planned, would allocate while callers hold unrooted objects (the
  REPL's datum), and a saved `below` in a C local would go stale across a
  collection and `longjmp`.  Phase 2 must revisit this: a continuation from
  outside, invoked inside, replaces the chain the unwind walks.
- **`apply`**: within the cap it spreads in place, relocating to the bottom
  of the buffer if it must; beyond it (`apply_long` in `src/eval.c`), a
  closure binds from a fresh list (`minim_env_extend_list`), and a primitive
  takes `minim_gc.spill`, a collector root that `minim_catch` clears.
  `apply` of `apply`, `values`, `call-with-values`, or `load` beyond the cap
  raises `too many arguments`.
- **Tests**: `tests/test_stack.c` (new) drives the module directly, since
  when a freeze happens depends on how full the buffer is; it caught
  `minim_stack_unwind` walking off the end of the chain.  `test_eval`:
  `test_deep_recursion` and `test_many_arguments`; `test_tail_calls` now
  checks, through a test primitive `stack-height`, that each loop's height
  at its last iteration equals its height with none, in place of the old
  array's capacity.  Under stress, recursion 6,000 deep, and no 8,191-operand
  call: each collection scans the whole buffer, so these took minutes.
- Measured against `main`, release, run alternately five times:

  | Program | This phase | `main` |
  |---|---|---|
  | `(sum 100000)` non-tail, 30 times | 2.16-2.33 s | 2.48-2.59 s |
  | `fib 27` | 0.29-0.32 s | 0.30-0.32 s |
  | a `let`-heavy loop | 3.12-3.26 s | 2.95-3.10 s |
  | the short-lived allocation program | 1.81-1.87 s | 1.72-1.80 s |

  `ctest --preset stress -j 8`: 19-20 s (from 14), `eval_stress` the longest
  at 17 s.

- `src/gc.h`, `src/gc.c`: `struct minim_value_stack` in place of the values
  array; `below` a root; copy-out, underflow, and push.
- `src/eval.c`: copy-out before pushing an operator; underflow on return;
  `too many arguments`; `apply` and `values` check the cap.
- `src/error.c`: `minim_catch` as above.

Separate because it changes the stack under every call with behaviour
unchanged, measurable alone, before anything depends on it.

Tests:

- `tests/test_eval.c`: non-tail recursion 10,000 deep, crossing many
  segments; several values returned across an underflow; an error deep in a
  recursion, then a clean stack; `too many arguments` for a written-out call
  over the cap; `apply` of a closure (with a rest parameter) and of a
  primitive (`+`) to 100,000 elements;
  `test_tail_calls` checks no frozen segments build up, in place of the
  array's capacity.
- The full suite, unchanged.  Measure against `main`, run alternately:
  release, the allocation programs, a call-heavy loop, deep recursion, and
  `ctest --preset stress -j 8`.

```sh
cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
```

### Phase 2 - Continuation objects and %call/cc

**Done.**  Details settled during the phase:

- The continuation object is `[header | k | stack | height]`, type `0x6`:
  `height` records where its frozen segments end, so a capture at the
  buffer's bottom shares `below` without a new segment.  The collector
  traces `k` and `stack`.
- `minim_stack_capture` and `minim_stack_reinstate` in `src/stack.c`.
  Applying a continuation moves its arguments to the bottom of the emptied
  buffer, so nothing is set aside or allocated.  Beyond the cap, through
  `apply`, it raises `too many arguments`, as `%call/cc` itself does.
- `minim_catch` (Phase 1's note): it unwinds to its entry height along
  whatever chain is current, which is exact when it is entered with the stack
  empty, as every caller does (the REPL, `load` from `main.c`, the tests,
  `boot_gen`).  Its doc in `minim.h` now says so.
- `call/cc` and `call-with-current-continuation` are `%call/cc` in
  `src/prelude.scm`; the boot file regenerated.
- Tests: `tests/repl/continuations.scm` as planned, plus
  `call-with-current-continuation` and `(call/cc (lambda () 1))`;
  `test_tail_calls` gains a loop through `call/cc` and a loop re-entering one
  continuation.
- Measured, release: capturing at the bottom of a recursion 10,000 deep,
  1,000 times, 6.1-6.3 s, the same as without `call/cc`; a loop capturing at
  every step, 1M steps, 0.73-0.76 s, also the same.  A capture copies at most
  the buffer's live part.  `ctest --preset stress -j 8`: 19.2 s.

- `include/minim.h`, `src/object.c`: the continuation type, constructor, and
  accessors; `minim_is_procedure` includes it.
- `src/gc.c`: tracing and size.  `src/print.c`: `#<continuation>`.
  `src/prims.c`: `equal-hash` treats it as it treats closures.
- `src/eval.c`, `src/prims.c`, `src/prims.h`: `%call/cc` and applying a
  continuation.
- `src/prelude.scm`: `call/cc` and `call-with-current-continuation` as
  `%call/cc`, until Phase 3.  `src/prelude.boot` regenerated.

Separate because it is the machine change, testable without `dynamic-wind`.

Tests:

- `tests/repl/continuations.scm` (new): an escape from `for-each`; the
  example above across two top-level forms; re-entry several times within one
  form; `(k)` and `(k 1 2)` into `call-with-values`; capture 10,000 deep and
  re-entry there; `procedure?` and printing; `(call/cc 5)`, `(call/cc)`.
- `tests/test_eval.c`: a loop through `call/cc` in tail position runs in
  constant space.  Measure a capture 10,000 deep, and a loop capturing every
  step.

```sh
cmake --build --preset dev && ctest --preset dev -R 'continuations|eval|boot' && ctest --preset stress -R 'continuations|eval'
```

### Phase 3 - dynamic-wind

- `src/prelude.scm`: `dynamic-wind` and the winding `call/cc`.
  `src/prelude.boot` regenerated.
- The winders and `minim_catch` restoring them (see Open items).

Separate because it is Scheme on top of Phase 2, with its own semantics.

Tests (`tests/repl/continuations.scm`, with a log of thunk calls): the order
`before`, thunk, `after`, returning several values; an escape out of nested
extents runs the `after`s innermost first; re-entry runs the `before`s
outermost first; a jump between sibling extents; after an error inside a
thunk, a later jump runs no stale `after`.

```sh
cmake --build --preset dev && ctest --preset dev -R 'continuations|boot' && ctest --preset stress -R 'continuations'
```

### After the last phase

Sync the docs: `eval.md` (the segmented value stack, capturing and applying
continuations, delimited by `minim_eval`, `too many arguments`), `gc.md`
(frozen segments and the continuation object), `primitives.md` (`%call/cc`,
`list->vector`), `prelude.md` (`call/cc`, `dynamic-wind`), `errors.md`
(`minim_catch` and the value stack and winders; errors skip `after`s), and
the roadmap (milestone 4: `call/cc` and `dynamic-wind` done).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

None.  Settled:

- Before Phase 0: the winders are a C variable in the evaluator's state,
  registered as a collector root, read and set by `%winders` and
  `%set-winders!`.
- During Phase 0: returning more than `MINIM_MAX_ARGS` values, from `values`
  or a continuation, raises `too many arguments`, as Gambit and Vicare cap
  arguments; `apply` of closures and primitives stays uncapped.  Values set
  aside across an underflow then fit a fixed-size C array.  A separate values
  buffer, uncapped and never captured since values are only ever in flight to
  the next return, is the option if a program needs more.

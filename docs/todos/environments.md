# Environments: local frames as vectors

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

Environments today (`src/env.c`, `eval.md`):

| Part | Representation | Cost |
|---|---|---|
| Local frame | `(bindings . parent)`, bindings an association list | a call with n arguments allocates 2n + 1 pairs; a lookup compares names pair by pair |
| `let-values` frame | the same, built one binding at a time by `minim_env_define` | a pair per binding, and two per clause's rest list |
| Global environment | `'()`; each symbol's value slot | one load |

The global environment stays as it is.  Several global environments, for
libraries, will follow Chez Scheme (`s/syntax.ss`): an environment maps each
name to a *label*, a symbol whose value slot is the variable's location, and
the expander rewrites references to labels, so the runtime sees only symbols
and a global reference stays one load.  That is for the libraries milestone,
where the mapping has content; with one environment, every name is its own
label, which is today.

## Design

**A local frame is a vector**, `#(parent name ... value ...)`: one
allocation, its n names then their n values, n being (length - 1) / 2.

- The names are copied from the parameters or clauses when the frame is
  made.  A lookup scans them, contiguous, by name, as today; name i's value
  is at slot 1 + n + i.
- A rest parameter's slot holds the list of the remaining arguments.
- A `let-values` frame is made once, sized from its clauses, its slots
  `MINIM_UNBOUND` until bound; reading or setting one unbound raises
  `unbound variable`, as reading a renamed `letrec` variable early does today
  (it falls through to an unbound global).
- Definitions bind only globals (the expander turns body definitions into
  `letrec-values`), so a frame is never extended, and `minim_env_define`
  binds only globals.
- The chain still ends at `'()`, the global environment.

## Phases

### Phase 1 - Benchmarks

- `bench/` (new): `fib.scm` (calls), `let.scm` (`let`- and `letrec`-heavy
  loops), `globals.scm` (references to global procedures and variables), and
  `run.sh`, which runs two builds alternately, several times, and prints each
  one's times; startup is `minim` on an empty file.

Separate because the next phase is measured against it, and against `main`.

Tests: none; `bench/run.sh build/release build/release` runs.

```sh
cmake --build --preset release && bench/run.sh build/release build/release
```

**Done.**
- Startup is `bench/empty.scm`, so every benchmark is a file the runner
  treats alike.
- `run.sh` times with bash's `time` (`TIMEFORMAT=%R`), which works on Linux
  and macOS, and discards each run's output; the benchmarks print nothing,
  since a file runs without the REPL.
- Sizes, release build: `fib` (fib 28) about 0.52 s; `globals` 500,000
  iterations, about 0.49 s; `let`, about 0.55 s; startup about 3 ms.  The
  same build against itself differs by 2-3%, the noise to beat.

### Phase 2 - Vector frames

- `src/env.c`, `src/env.h`: frames as vectors; `minim_env_extend` from the
  value stack or a list; a `let-values` frame and its slot setter; lookup and
  `set!` by name over `names`; `minim_env_define` only global.
- `src/eval.c`: `let-values` makes its frame once, and `bind_values` sets its
  slots.

Tests: the full dev suite (every program uses frames); `test_eval`:
references across frames, `set!` of an outer local, rest parameters (stacked
and from a long `apply`), a `letrec-values` variable read and set before its
init (`unbound variable`).  Measure against `main` with `bench/run.sh`.

```sh
cmake --build --preset dev && ctest --preset dev && ctest --preset stress -R 'eval|tail|values|derived'
```

**Done.**
- First built as `#(parent names v ...)`, `names` the declaring code itself:
  a `lambda`'s parameters, or a `let-values` form's clauses.  `let` was 5%
  slower than `main`: a lookup reached each name through the clause, then
  its formals.  Copying the names into the frame, names before values so the
  scan is contiguous, costs n words but no allocation, and every frame has
  one shape.
- Measure with code aligned: unaligned release builds differed by up to 7%
  from code placement alone.  Built both with `-falign-functions=64
  -falign-loops=64 -falign-jumps=64`.  Against `main`, 9 runs, medians:
  `fib` 0.470 to 0.453 s, `globals` 0.451 to 0.422, `let` 0.490 to 0.465.
- gprof on `globals`: heap allocations 14.6M to 11.5M, root pushes 25.6M to
  23.6M.
- A first version took the rest symbol from the whole parameter list, not
  its tail, so rest parameters were empty; the expander uses them, so every
  `let-values` it expanded broke.
- Ran the dev suite, and stress `eval|tail|values|derived|records|errors|
  continuations`.

### After the last phase

Sync the docs: `eval.md` (Environments), `gc.md` if it describes frames;
revise `docs/todos/compile.md`, whose Phase 2 assumed frames without names;
and the roadmap: milestone 9 (local frames as vectors; benchmarks exist) and
milestone 5 (environments as name-to-label maps in the expander, after Chez).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

None: the global environment and hash tables were deferred to the libraries
milestone, where they have a use.

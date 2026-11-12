# Compile pipeline: a simplified core, a compiler in Scheme, and its hook

Implementation plan.  The design is settled; what follows is the phase
breakdown, one phase per commit.  Milestone 1 of `docs/roadmaps/compiler.md`.

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

The evaluator (`src/eval.c`, `eval.md`) runs the expander's output, the core
forms, as lists.  `run`'s `eval` label checks each form every time it runs
(`list_length`, `valid_params`, `valid_clauses`, `valid_lambda_clauses`) and
dispatches on its head symbol, an application being any pair whose head
isn't a special form's.  Applying a closure walks its clauses' formals to
choose one (`clause_for`, `closure_accepts`).  Variables are found by name
(`src/env.c`).

The roadmap's later milestones simplify the core in passes written in Scheme,
for an evaluator that trusts its input.  This milestone builds the pipeline
those passes plug into, changing no semantics: the compiler's first output
still finds variables by name.  So everything that differs between the two
languages here is plumbing, and milestone 2's change of representation lands
on a pipeline already tested.

## Design

**Three languages.**

- **The core**: the expander's output, as today: `quote`, `if`, `define`,
  `set!`, `lambda`, `case-lambda`, `begin`, `define-values`, `let-values`,
  `letrec-values`, applications, variables, and self-evaluating data.
- **Parsed**, `parse`'s output: the core checked and normalized.  Every
  constant is `(quote d)`; every `if` has an else; `(begin)` is `(quote
  #<void>)` and a one-form `begin` its form; a body is one expression, a
  `begin` if it had several; `lambda` and `case-lambda` are one form,
  `(lambda (formals body) ...)`, a list of clauses; an application is
  `(call f arg ...)`.
- **The simplified core**, `emit`'s output, which the evaluator runs: parsed,
  each `lambda` a `clambda`, each clause `(formals required rest? body)`, its
  required count and whether it takes a rest parameter computed once:

  ```
  e ::= x | (quote d) | (if e e e) | (begin e e ...) | (set! x e)
      | (define x e) | (define-values formals e)
      | (let-values ((formals e) ...) e) | (letrec-values ((formals e) ...) e)
      | (clambda (formals required rest? e) ...) | (call e e ...)
  ```

Nodes are lists with symbol heads (`compiler.md`, Open items).

**The compiler**, `scheme/compile.scm`, the last prelude file:
`%compile-form` is `(emit (parse x))`.  `parse` makes every check the
evaluator makes today, raising `bad syntax` with the same irritant, the whole
offending form.  It knows each form's position, so `define` and
`define-values` anywhere but top level, or in a top-level `begin`, are `bad
syntax`, which the evaluator finds today by its environment.  A checker per
language, `%parsed?` and `%simplified?`, holds the passes to their output in
tests.  Passes are written by hand (`compiler.md`).

**The evaluator runs both languages.**  A register, `compiled`, says which
language `expr` is in; a label, `ceval`, runs the simplified core without
checks; continuation frames record the language they resume in.  Which
language runs is never decided by inspecting a form: simplified code
arrives only from the compiler, and a closure's code is a `clambda` exactly
when compiled code made it.  Applying one chooses a clause by its
`required` and `rest?`, binds its formals by name, and evaluates its body
compiled; unsimplified closures run as today, and either kind calls the
other.

**The hook** is a parameter, `%compile`, after Racket's `current-compile`:
its value is the compiler, `%compile-form`, or `#f`.  After `%expand`, and
on the core forms `minim_eval_expanded` and `%eval-expanded` take, the
evaluator calls `(%compile)` through the machine, as it calls `%expand`; if
the value is `#f`, it runs the form unsimplified, else it applies the value
to the form and runs the result compiled.  While `%compile` is unbound, the
form runs unsimplified.  So `(parameterize ((%compile #f)) (eval x))`
compiles nothing in its extent, and `(%compile #f)` switches compiling off
for the rest of a session.  `compile.scm`'s last form defines it; being
last, it is bound only after every prelude form has loaded, so startup
compiles nothing.  Internal, in `(minim system)`, as `%expand` is.

**`minim --interpret`** sets `%compile` to `#f` before running anything, the
REPL, a script, or a program.  A test preset, `interpreted`, sets
`MINIM_TEST_INTERPRET=1`, which the test scripts (`tests/run_minim.cmake`),
not Minim, read, passing `--interpret`; CI runs it beside `dev` and
`stress`.  The C unit tests run with the hook as the prelude binds it.

## Phases

### Phase 1 - The simplified core in the evaluator

- `src/eval.c`: the `compiled` register; `ceval`; frames recording their
  language, which `sequence` and `let_values` honor; `clambda` closures
  applied by `required` and `rest?`, on the stack and through `apply`'s long
  path.
- `src/symbol.c`, `src/symbol.h`: `call` and `clambda`.
- `src/control.c`, `src/prims.h`: `%eval-compiled`, which the evaluator
  recognizes, as `%eval-expanded`: its argument, simplified code, evaluated
  globally in tail position.  Internal, in `(minim system)`.

First, so the evaluator's new half is tested on hand-written simplified code
before any compiler produces it.

Tests: `tests/repl/compiled.scm` (new), through `%eval-compiled`: each form;
`clambda` with fixed, rest, and catch-all formals, and several clauses; a
million-iteration loop in constant space (tail calls); `call/cc` out of and
back into compiled code; a compiled closure passed to `map`, and an
unsimplified closure called from compiled code; `let-values` and
`call-with-values` with several values; `letrec-values` read before its init
(`unbound variable`); arity mismatch; `%closure-code` of a compiled closure
a `clambda`; `procedure-rename` of one still applying.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'compiled|eval|continuations|values|boot'
```

**Done.**  `compiled` is a register of `run`, like `expr` and `env`, never
carried across a jump: a closure's application sets it from its code's
head, `ret` from the frame it returns to, whose kind carries `K_COMPILED`
(`0x100`), and the entry points from what they take.  So a tail call, which
pushes no frame, and a continuation, whose frames each carry their
language, need nothing more.  `next` dispatches to `eval` or `ceval`;
`sequence`, `let_values`, and `K_IF`'s and `K_ARGS`'s returns go through
it, and the frames they push take the register's value.  `ceval` tests
`call` first, as the commonest node, and treats any head it doesn't know as
`define-values`, trusting its input.  `%eval-expanded` and `%eval-compiled`
share their case in `apply`.  `compiled_clause_for` picks a `clambda`
clause by `required` and `rest?`; `minim_env_extend` binds its formals as
today.  `tests/repl/compiled.scm` as planned, plus a 10,000-argument
`apply` of a compiled closure, the long path.

### Phase 2 - The compiler

- `scheme/compile.scm` (new; last in `MINIM_SCHEME_SOURCES`): `parse`,
  `emit`, `%compile-form`, `%parsed?`, `%simplified?`.
  `scheme/prelude.boot` regenerated.

Separate from the hook, so the compiler's output is tested directly before
the evaluator depends on it.

Tests: `tests/repl/compile.scm` (new): each core form's simplified form,
printed, and each output `%simplified?`, each `parse` output `%parsed?`;
each of the evaluator's `bad syntax` cases raising the same message and
irritant from `parse`; `define` inside a `lambda` or `let-values` rejected,
inside a top-level `begin` accepted; and forms whose values, through
`(%eval-compiled (%compile-form 'x))`, equal those of evaluating them.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'compile|boot'
```

**Done.**  `%parse` mirrors `eval.c`'s checks one for one, raising through
`%bad-syntax`, whose message and irritants match `minim_error`'s; it also
rejects definitions outside top level, `(if a (define x 1))` included,
which the evaluator accepts while its environment is global.  A one-form
`begin` becomes its form, keeping `top?`, so nested top-level `begin`s may
define.  `%emit` adds `required` and `rest?` to each clause, from
`%required` and `%rest?`.  The checkers share `%in-language?`, which
rejects the other language's `lambda` or `clambda` and a `clambda` whose
arity doesn't match its formals.  Helpers: `%formals?`, `%all?`,
`%lambda-clause?`, `%values-clause?`, `%void-form`.  Tests as planned, in
`tests/repl/compile.scm`: 22 forms printed and checked, 6 programs agreeing
with the evaluator, 18 malformed forms raising the same failure, and the
definition positions.

### Phase 3 - The hook

- `src/eval.c`: after `%expand`, and on `minim_eval_expanded`'s and
  `%eval-expanded`'s forms, a frame calling `(%compile)`, then one applying
  its value, whose return runs the result compiled.  `src/symbol.c`:
  `%compile`.
- `scheme/compile.scm`: `%compile`, a parameter, `%compile-form` initially.
- `src/main.c`: `--interpret`, in the usage and help.
- `tests/run_minim.cmake`: `--interpret` when `MINIM_TEST_INTERPRET` is
  set.  `CMakePresets.json`: the `interpreted` test preset, setting it.
  `.github/workflows/ci.yml`: the preset in the matrix.

Last, as it is the step that makes compiled code the default, with the full
suite as its net in both modes.

Tests: `tests/repl/compile-hook.scm` (new): a closure's code a `clambda`,
and one made inside `(parameterize ((%compile #f)) (eval ...))` a `lambda`;
`eval` compiling; `(%compile #f)` switching it off for later forms.  Script
tests: a script and a program with `--interpret` making `lambda` closures;
`--help` listing the option.  The tests that expect a `clambda` hold only
with the hook on, so the interpreted preset skips them, by a label.  The
full suite with `--preset dev` and `--preset interpreted`.
Measure `bench/run.sh` against `main`, release: the compiler runs
interpreted until milestone 3, so `expand`, many small top-level forms, will
slow; the others should hold.

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset interpreted
```

### After the last phase

Sync the docs: `eval.md` (the two languages, the `compiled` register,
frames' languages, `clambda` closures, the hook), a new `compile.md` in
`docs/design/` (the languages, the passes, the checkers, the hook and its
switch), `prelude.md` (`compile.scm` in the file table), `primitives.md`
(`%eval-compiled`), `repl.md` (`--interpret` in the command table),
`testing.md` (the `interpreted` preset), the design index, and the roadmap
(`compiler.md`: milestone 1 done).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8 && ctest --preset interpreted
cmake --preset release && cmake --build --preset release
```

## Open items

None; see Settled in review.

## Settled in review

- Compiling is controlled by the hook itself, `%compile`, a parameter, as
  Racket's `current-compile`: `#f` runs forms unsimplified, so
  `parameterize` scopes it and calling it with `#f` switches it off.  It is
  internal, as `%expand` is.  A C global was weighed: it is cheap to check,
  but can't be scoped, and would be a second control beside the hook.
- `minim --interpret` switches compiling off for a run; Minim reads no
  environment variable for it.  The `interpreted` preset passes the option
  through the test scripts.
- Malformed core code raises `bad syntax` at compile time, from `parse`,
  before any of the top-level form runs.  Only core forms written by hand
  can show the difference; the interpreted preset keeps the run-time
  behavior covered.
- The compiler's cost at the REPL, running interpreted until milestone 3
  compiles the boot file, is accepted, and measured in Phase 3.

# Self-hosting: the expander in normal Scheme

Implementation plan.  The design is settled; what follows is the phase
breakdown, one phase per commit.

This is the second of two PRs.  The first made startup load the prelude from
its expansion, the checked-in `src/prelude.boot`.  So the expander no longer
runs unexpanded, and its source can use the derived forms; this PR rewrites it
that way, then removes what only unexpanded code needed: internal `define`
and `define-values` in the evaluator.

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

The expander is lines 1-639 of `src/prelude.scm`.  Its header still says it
"uses only core forms and primitives", from when it ran before `%expand`
existed.  So it is written in core forms:

| Today | In normal Scheme |
|---|---|
| `(define f (lambda (x) ...))`, 124 `define`s | `(define (f x) ...)` |
| nested `(if a b (if c d (if ...)))`, 13 deep in `%expand-form` | `cond`, `case` |
| internal `define` for every local | `let`, `let*`, named `let` |
| `%map`, `%append`, `%reverse`, `%memq`, ... (66 uses) | `map`, `append`, `reverse`, `memq`, ... |

None of that is needed any more: `boot_gen` expands the expander with the
running system, which has every macro and procedure the prelude defines, in
whatever order the file defines them.

The evaluator still binds `define` and `define-values` in a procedure's frame
(`eval.md`), only for that unexpanded code.

`(define (f x) ...)`, R7RS's procedure definition (5.3), isn't supported:
`(define (f x) x)` is `bad syntax`.

## Design

**Phase 0 adds procedure definitions.**  `(define (name . formals) body ...)`
means `(define name (lambda formals body ...))`, at top level and in bodies,
as in R7RS 5.3.  The rewrite uses it for most of its definitions.

**What the rewrite may use**: the derived forms (`let`, named `let`, `let*`,
`letrec`, `cond` with `=>`, `case`, `when`, `unless`, `and`, `or`, `do`),
quasiquote, procedure definitions, internal definitions, and the list
library (see Open items).  It changes no behaviour: `tests/repl/expansion.out`
and every other expected output stay as they are.

**Regenerating safely.**  Each rewrite phase regenerates the boot file twice
(`prelude.md`): the first run expands the new source with the old expander,
the second with the new one.  Between them, rebuild and run the REPL tests,
so a broken new expander shows up before it expands itself.  If a build
can't start, `git checkout src/prelude.boot` recovers.

**Removing internal definitions.**  The evaluator raises `bad syntax` for
`define` and `define-values` outside the global environment.
`minim_env_define` then binds into a local frame only for `let-values` and
`letrec-values`.

## Phases

### Phase 0 - Procedure definitions

- `src/prelude.scm`: `%expand-define` and `%expand-body`'s scan accept
  `(define (name . formals) body ...)`, rewriting it to `(define name (lambda
  formals body ...))` first, so the rest of their handling is unchanged.
- `src/prelude.boot`: regenerated twice.

Separate because it is a language feature, reviewable on its own, that the
rewrite depends on.

Tests:

- `tests/repl/expander.scm`: procedure definitions at top level and in a
  body, with fixed, dotted, and rest formals; recursion; one inserted by a
  macro; `bad syntax` for `(define (1) 1)`, `(define (f))`, and `(define ()
  1)`.
- `tests/repl/expansion.scm`: what a procedure definition expands to.
- `tests/test_eval.c`: `(define (f) 1)` is no longer `bad syntax`; check a
  malformed one instead.

```sh
cmake --build --preset dev && ctest --preset dev -R 'repl|eval|boot' && ctest --preset stress -R 'repl|eval'
```

### Phase 1 - Rewrite: expansion

- `src/prelude.scm`: the helpers, syntactic environments, resolution,
  `%expand-head` through `%expand-let-syntax`, and the top-level forms
  (`%top-name` through `%expand`), in normal Scheme.  The section header
  drops "uses only core forms and primitives".
- `src/prelude.boot`: regenerated twice.

Separate from Phase 2 so each review is about 300 lines, and because these
are the parts every expansion runs through.

Tests: the full suite, unchanged; the `boot` test.

```sh
cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
```

### Phase 2 - Rewrite: quasiquote and syntax-rules

- `src/prelude.scm`: `%qq` through `%qq-vector`, `%transformer`,
  `%syntax-rules`, and their helpers, in normal Scheme.
- `src/prelude.boot`: regenerated twice.

Tests: the full suite, unchanged; the `boot` test.

```sh
cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
```

### Phase 3 - Drop internal definitions from the evaluator

- `src/eval.c`: `define` and `define-values` are `bad syntax` outside the
  global environment.
- `src/env.c`, `src/env.h`: `minim_env_define` keeps what `let-values` and
  `letrec-values` need, and its doc says so.

Separate because it is the step that makes the boot file the only way to run
the expander; everything before it can be reverted without one.

Tests (`tests/test_eval.c`, without the expander): an internal `define` and
`define-values` are `bad syntax`; top-level ones still work.  The full suite,
which shows nothing expanded still produces an internal definition.

```sh
cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
```

### After the last phase

Sync the docs: `macros.md` (the expander is written in normal Scheme;
procedure definitions; Limitations, if it now calls the list library),
`prelude.md` (the expander's source), `eval.md` (`define` and `define-values`
only at top level), and the roadmap (self-hosting done).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

### Should the expander use the list library or keep its own helpers?

`map`, `append`, `reverse`, `memq`, and `assq` would replace `%map`,
`%append`, `%reverse`, `%memq`, and `%lookup`-style loops, about 50 lines,
and read as normal Scheme.  But then redefining `map` at top level breaks
expansion, as redefining `car` or `list` already does (`macros.md`,
Limitations).  Helpers with no library equivalent stay either way: `%length`
(which returns `#f` for an improper list), `%pairs`, `%take`, `%drop`,
`%append-map`, `%vector->list`.

Provisionally use the library: it widens a limitation that already exists,
and separating system bindings from the user's (roadmap milestone 8) fixes
both.  Reopen if that separation is dropped.

### Is procedure definition this PR's to add?

It is an R7RS feature in its own right, so it could be its own PR.  But the
rewrite would otherwise write `(define f (lambda ...))` a hundred times, and
adding it first is small.

Provisionally Phase 0 of this PR.

# Eval: (scheme eval) and (scheme repl)

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

R7RS 6.12 has `eval`, `environment` (`(scheme eval)`), and
`interaction-environment` (`(scheme repl)`); `(scheme r5rs)` adds
`scheme-report-environment` and `null-environment`.  Minim has none:

| Input | Today |
|---|---|
| `(eval '(+ 1 2) (environment '(scheme base)))` | `unbound variable: eval` |
| `(interaction-environment)` | `unbound variable: interaction-environment` |
| `(scheme-report-environment 5)` | `unbound variable: scheme-report-environment` |

The pieces exist (`macros.md`): a top-level environment of the library kind
holds what its imports name (`%import!`), as `%expand-program` builds for a
program; `%expand-top` and `%expand-expr` expand in one; and the evaluator
runs core forms without expanding them (`minim_eval_expanded`).  What's
missing is a way for Scheme code to run expanded code: a primitive can't
re-enter the machine, as `load` and `apply` can't (`primitives.md`).

## Design

**`%eval-expanded`**, a primitive the evaluator applies, like `load`:
`(%eval-expanded code)` evaluates the core form `code` globally, in tail
position, so its values are the call's and a top-level `define` in it binds
a global.  Internal, in `(minim system)`.

**Environment specifiers** are records of a new type, `environment`, holding
a top-level environment, so they print as `#<environment>` and nothing can
reach the vector inside.  `(interaction-environment)` returns the one for the
interaction environment, made once.

**`(environment set ...)`** returns a specifier for a new top-level
environment of the library kind holding what the import sets name.  Its
bindings are immutable (R7RS), so the specifiers are cached by their sets
(`equal?`): the same sets give the same environment.  The cache also bounds
the stale properties environments leave on interned symbols (`macros.md`).

**`(eval x env)`** expands `x` in `env`'s top-level environment, then calls
`%eval-expanded` on the result in tail position:

- In the interaction environment, as a top-level form (`%expand-top`):
  definitions, `define-syntax`, and `import` work as at the REPL.
- In an `environment` one, as an expression (`%expand-expr`): a definition
  is `bad syntax`, and `set!` of an imported binding is `set!: immutable
  variable`, as R7RS's immutable environments require.
- `env` not a specifier: `eval: not an environment`.

**`(scheme-report-environment 5)`** is `(environment '(scheme r5rs))`;
**`(null-environment 5)`**, an environment with only `(scheme r5rs)`'s
syntax: its exports bound to macros or core forms.  Any other version is an
error.

## Phases

### Phase 1 - %eval-expanded

- `src/prims/control.c`, `src/prims.h`: `%eval-expanded`, registered with
  the other primitives the evaluator applies.
- `src/eval.c`: applying it evaluates its argument with an empty local
  environment, in tail position; it joins the "too many arguments" check for
  spilled calls.

Separate because it changes the evaluator, and everything else is Scheme on
top of it.

Tests (`tests/repl/expansion.scm`, which imports `(minim system)`):
`(%eval-expanded '(+ 1 2))`; multiple values; a core `define` binds a
global; a loop through it in tail position doesn't grow the stack; a
non-core form is `bad syntax`.

```sh
cmake --build --preset dev && ctest --preset dev -R 'expansion|eval' && ctest --preset stress -R 'expansion|eval'
```

### Phase 2 - eval, environment, interaction-environment

- `scheme/eval.scm` (new, after `derived.scm` in `MINIM_SCHEME_SOURCES`):
  the `environment` record type, `environment` with its cache,
  `interaction-environment`, and `eval`.
- `scheme/standard.scm`: `(scheme eval)` and `(scheme repl)`; both in the
  libraries `(minim)` takes its names from.  `scheme/prelude.boot`
  regenerated.

Separate because it is the libraries' substance, on Phase 1.

Tests (`tests/repl/eval.scm`, new): `eval` of an expression in
`(environment '(scheme base))`; a name the environment doesn't import is
unbound; a definition there is `bad syntax`, `set!` of an import immutable;
the same sets give `eq?` specifiers; in `(interaction-environment)`, a
definition is visible at the REPL afterward, and `define-syntax` and
`import` work; `eval` in tail position; a non-specifier is an error; a
specifier prints as `#<environment>`.  A program importing `(scheme eval)`
(`tests/scripts/`).

```sh
cmake --build --preset dev && ctest --preset dev -R 'eval|libraries|program' && ctest --preset stress -R 'eval'
```

### Phase 3 - scheme-report-environment, null-environment

- `scheme/eval.scm`: both procedures; `scheme/standard.scm`: in
  `(scheme r5rs)`.  `scheme/prelude.boot` regenerated.

Separate because they are `(scheme r5rs)`'s, a library of their own.

Tests (`tests/repl/eval.scm`): `(eval '(car '(1)) (scheme-report-environment
5))`; `if` and `let` work in `(null-environment 5)` but `car` is unbound;
version 4 is an error.

```sh
cmake --build --preset dev && ctest --preset dev -R 'eval' && ctest --preset stress -R 'eval'
```

### After the last phase

Sync the docs: `macros.md` (environment specifiers, `eval`, the cache),
`primitives.md` (`%eval-expanded` among the primitives the evaluator
applies), `prelude.md` (`eval.scm`), and the roadmap (`r7rs-small.md`: the
coverage table, `(scheme eval)`, `(scheme repl)`, and `(scheme r5rs)`;
milestone 9).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

### Cache environments, or make a new one per call?

- **Cache**: environments are immutable, so sharing one is unobservable
  except by `eq?`, and repeated `(environment '(scheme base))` calls don't
  each leave about 130 properties on interned symbols for good.  But the
  cache holds every distinct set list ever passed.
- **New each call**: simpler, and what a reader might expect, but a loop
  calling `environment` grows memory without bound until symbols' properties
  can be dropped (a weak symbol table, `beyond-r7rs.md`).

Provisionally cache.  Reopen with a weak symbol table.

### What is `(null-environment 5)`?

R7RS: the bindings of R5RS's syntactic keywords.  Provisionally
`(scheme r5rs)`'s exports bound to macros or core forms.  The alternative,
the same as `scheme-report-environment`, is simpler but doesn't hide
procedures.  Reopen if telling core forms' labels from variables proves
awkward.

# Parameters: make-parameter, parameterize, and interaction-environment

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

R7RS 4.2.6 has parameter objects: `(make-parameter init [converter])` and
`(parameterize ((param value) ...) body ...)`.  Minim has neither:

| Input | Today |
|---|---|
| `(make-parameter 1)` | `unbound variable: make-parameter` |
| `(parameterize () 1)` | `unbound variable: parameterize` |

Ports (milestone 6) need them: `current-input-port`, `current-output-port`,
and `current-error-port` are parameters, and `parameterize` is how output is
redirected.  `dynamic-wind` (`scheme/control.scm`, `prelude.md`) already runs
`before` and `after` as continuations enter and leave its extent, which is
what `parameterize` needs.

## Design

After Chez Scheme (`s/front.ss`, `s/syntax.ss`) and R7RS 7.3's reference
implementation, in the prelude; C adds only `%closure-code`:

**A parameter is a procedure** closed over its value and converter.
`(p)` returns the value.  `(p v)` sets it to `v` converted, as Chez does.
`(p %parameter-token)` returns the converter, and `(p %parameter-token v)`
sets the value without converting it, for `parameterize`; the token is a
private object, so user code can't make these calls.

**`(make-parameter init [converter])`** converts `init` once, at creation
(R7RS); the converter defaults to the identity.

**`parameter?`**, Minim's own, in `(minim)` (Racket has one; R7RS and Chez
don't), tells parameters apart without calling them: every
parameter is a closure of the one `lambda` in `make-parameter`, so they all
share its code, and no other procedure does.  `(%closure-code x)`, a new
internal primitive, is a closure's `(params . body)`, or `#f`;
`parameter?` compares it, by `eq?`, with a parameter's made at startup.
So `parameterize` of a non-parameter raises `parameterize: not a parameter`
before calling anything.  Chez (a `case-lambda`) and Chibi (a parameter
opcode) don't check, and fail with an arity error; Racket and Chibi have a
parameter type, which Minim's core would need an applicable typed object
for.  Parameters print as `#<procedure>`, as in Chez.

**`parameterize`** is a macro over a procedure, as `guard` is over
`%guard`: `(parameterize ((p v) ...) body ...)` is `(%parameterize (list p
...) (list v ...) (lambda () body ...))`.  `%parameterize` converts each
value with its parameter's converter, then calls the thunk inside a
`dynamic-wind` whose `before` and `after` both swap the parameters' values
with the saved ones, as Chez's does.  Swapping rather than setting keeps a
value set inside the extent, `(p v)`, across leaving and re-entering it by a
continuation.  The values are restored without converting them (R7RS).  The
body is a `lambda` body, so it may start with definitions.

**`interaction-environment` becomes a parameter**, as in Chez and Chibi
(`macros.md`): the REPL, `load`, and `minim_define` expand in its current
environment, so `parameterize` redirects them.  Whether definitions are
allowed becomes a property of the specifier, mutable or not, rather than a
comparison with the REPL's environment.

## Phases

### Phase 1 - make-parameter

- `src/control.c`: `%closure-code`, with `procedure?`.
- `scheme/control.scm`: `%parameter-token`, `make-parameter`, and
  `parameter?`.
- `scheme/standard.scm`: `make-parameter` in `(scheme base)`.
  `scheme/prelude.boot` regenerated.

Separate because it is a procedure of its own, testable without
`parameterize`.

Tests (`tests/repl/parameters.scm`, new): a parameter's value; the converter
applied to `init`, once; `(p v)` sets the converted value; a parameter is a
procedure; a converter that raises makes `make-parameter` raise; wrong
arguments (`(p 1 2)`) are an error.  `parameter?`
of a parameter, a `lambda`, `car`, and `1`; `%closure-code` of a closure and
of `car`.

```sh
cmake --build --preset dev && ctest --preset dev -R 'parameters|libraries' && ctest --preset stress -R 'parameters'
```

**Done.**

- `make-parameter` is a `(lambda args ...)` dispatching on the arguments:
  none, the token (with or without a value), or one value.  More is
  `parameter: too many arguments`.
- `%parameter-code` is taken from a parameter made when `control.scm`
  loads.
- `parameter?` is public, in `(minim)`, not `%parameter?`: it is exact and
  never calls its argument, so user code can check arguments the same way.
- Ran `ctest -R 'parameters|libraries|boot'`, and stress `parameters`.

### Phase 2 - parameterize

- `scheme/control.scm`: `%parameterize`, which checks its parameters with
  `parameter?` first.
- `scheme/derived.scm`: the `parameterize` macro.
- `scheme/standard.scm`: `parameterize` in `(scheme base)`.
  `scheme/prelude.boot` regenerated.

Separate because it is the syntax, on Phase 1, and its tests are about
dynamic extent.

Tests (`tests/repl/parameters.scm`): the value inside and after the body;
several parameters at once; nesting, the inner binding shadowing the outer;
the converter applied to the new value but not to the restored one; the old
value restored when the body escapes by `call/cc` and by `raise` caught with
`guard`; re-entering the body by a continuation re-establishes the binding;
a value set with `(p v)` inside the body survives leaving and re-entering;
definitions at the start of the body; `(parameterize () 1)`;
`(parameterize ((car 1)) 'x)` is `parameterize: not a parameter: #<procedure
car>`, and the body doesn't run.

```sh
cmake --build --preset dev && ctest --preset dev -R 'parameters|continuations|guard' && ctest --preset stress -R 'parameters'
```

**Done.**

- The swap reads every current value before setting any, as Chez's does, so
  a parameter named twice (`(parameterize ((p 1) (p 2)) ...)`) is restored
  to its old value; swapping one at a time would leave it at `1`.
- `%parameterize` converts every value, after checking every parameter,
  before entering the `dynamic-wind`, so a converter's error leaves nothing
  set.
- Ran `ctest -R 'parameters|continuations|guard|boot'`, and stress
  `parameters`.

### Phase 3 - interaction-environment as a parameter

- `scheme/eval.scm`: environment specifiers record whether they are
  mutable: the REPL's is, `environment`'s aren't.  `interaction-environment`
  becomes a parameter, its converter requiring a specifier, as Chez's does.
  `eval` expands a form as a top-level form in a mutable environment and as
  an expression in an immutable one, instead of comparing with the REPL's.
- `%expand` and `%interaction-label!` (for `minim_define`) move from
  `scheme/expander.scm` to `scheme/eval.scm` and expand in the parameter's
  current environment by `eval`'s rule, so the REPL, `load`, and
  `minim_define` follow `parameterize`.  The prelude loads already expanded,
  so nothing needs `%expand` before `eval.scm`.  `scheme/prelude.boot`
  regenerated.

Separate because it changes what the REPL and `load` expand in, on Phases 1
and 2.

Tests (`tests/repl/eval.scm`): `(parameterize ((interaction-environment
(environment '(scheme base)))) ...)` evaluates `(interaction-environment)`'s
forms there, and the REPL's environment is back after; `load` inside it
expands the file there; at the REPL, after `(interaction-environment env)`
with an immutable `env`, a definition is `bad syntax` until the saved
environment is set back; `(interaction-environment 5)` is an error.

```sh
cmake --build --preset dev && ctest --preset dev -R 'eval|files|load|top-level' && ctest --preset stress -R 'eval'
```

### After the last phase

Sync the docs: `primitives.md` (`%closure-code`), `prelude.md`
(`make-parameter`, `parameter?`, `%parameterize`),
`macros.md` (`parameterize` among the derived forms; `interaction-environment`
a parameter, specifiers' mutability, `%expand` in `eval.scm`), and the roadmap
(`r7rs-small.md`: the coverage table, `(scheme base)` 128 to 130, and
milestone 9).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

### May `interaction-environment` be set to an immutable environment?

R7RS says `interaction-environment` returns a mutable environment, but only
the REPL's is mutable: `environment`'s aren't, and Minim has nothing like
Chez's `copy-environment` to make another.  Requiring a mutable one would
leave the parameter nothing to be set to but the REPL's own.

Provisionally any specifier, as Chez's converter allows: in an immutable one
the REPL accepts only expressions.  Reopen with a way to make mutable
environments.

# Derived forms and macros: roadmap milestone 3

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
  they never describe an intermediate state.  Phases 1-4 edited them before
  this rule; the sync covers whatever they still get wrong.
- **Design docs stay succinct**: only the details a reader needs to understand
  the current implementation, said in as few words as possible.

## Context

The evaluator (`src/eval.c`, `docs/design/eval.md`) knows six core forms:
`quote`, `if`, `define`, `set!`, `lambda`, and `begin`.  Everything else must
be written with them, which makes ordinary Scheme tedious and the prelude
(`src/prelude.scm`) a thicket of nested `if`s:

| Input | Today |
|---|---|
| `(let ((x 1)) x)` | `error: unbound variable: let` |
| `(cond ((= 1 1) 'a))` | `error: unbound variable: cond` |
| `(and #t #f)` | `error: unbound variable: and` |
| `` `(1 ,(+ 1 1)) `` | reads `` ` `` and `,` as symbols: `error: unbound variable: ,` |
| `(define-syntax swap! ...)` | `error: unbound variable: define-syntax` |
| `(lambda (if) (if 1))` | the local `if` can't shadow the special form, so `(if 1)` is `bad syntax` |

This milestone adds quasiquote syntax, an R7RS-small macro system
(`define-syntax`, `let-syntax`, `letrec-syntax`, and hygienic
`syntax-rules`), and the standard derived forms as macros.

**Target:** R7RS-small sections 4.2 (derived expression types used here),
4.3 (macros, including `syntax-error`), 5.3 (definitions), and 5.4 (syntax
definitions), including the details below.

**Decisions:**

| Decision | Choice | Reason |
|---|---|---|
| Expander language | Scheme, defined first in the prelude using only core forms | pattern matching is list processing; in C, recursive code that allocates needs `MINIM_FRAME` everywhere |
| Startup cost | accepted: the prelude is expanded at every startup, measured after the last phase | the prelude is small; precomputing needs a bootstrap build step, and the static space (roadmap milestone 8) addresses stress mode |
| When expansion happens | each top-level form is expanded just before it is evaluated | a `define-syntax` affects the forms after it, as R7RS requires |
| Hook | the evaluator runs top-level forms (from `minim_eval` and `load`) through `%expand`, if it's bound, inside the machine | no C re-entry into `minim_eval`; before the prelude defines `%expand`, forms run unexpanded |
| Hygiene | renaming (Clinger and Rees, "Macros That Work"); see below | R7RS 4.3 requires `syntax-rules` to be hygienic and referentially transparent |
| Local bindings | every local binding is renamed to a fresh uninterned symbol | needed for hygiene; also lets locals shadow special forms and keywords, as R7RS requires |
| Macro scope | `define-syntax` at top level and in bodies, `let-syntax`, `letrec-syntax` | R7RS 4.3.1 and 5.4 |
| Transformers | `syntax-rules` only, evaluated at expansion time without running code | R7RS-small has no other transformer |
| Top-level definitions inserted by a macro | an assignment if the name already holds a value; otherwise a fresh, private global | R7RS 4.3 and 5.3.1: a definition "may or may not introduce a binding" |
| Quasiquote | the reader turns `` ` ``, `,`, `,@` into `quasiquote`, `unquote`, `unquote-splicing`; the expander rewrites them into `cons`, `list`, `append`, and `quote`, tracking nesting levels | R7RS 4.2.8; the printer abbreviates them like `'x` |
| Internal names | the prelude's implementation procedures start with `%` | keeps them out of the way of user names; hygiene protects macro temporaries |

## Expansion

`(%expand form)` returns `form` rewritten into core forms.  It walks `form`
with a *syntactic environment*: a list mapping identifiers to their local
meaning, either a fresh variable name or a macro.

**Identifiers** are symbols or *aliases*.  An alias is an uninterned symbol
created when a template is instantiated.  It prints like the name it was
created from, and it remembers that identifier (its *original*) and the
syntactic environment where the macro was defined.

**Resolving an identifier:**

1. If the environment binds it, that binding.
2. Otherwise, if it is an alias, its original, resolved in the alias's
   definition environment.
3. Otherwise the symbol itself, a global variable, special form, or global
   macro.

An operator is a special form only when it resolves to the global with that
name, and a macro only when it resolves to a macro binding.  So a local `if`
is an ordinary variable, and a macro's `if` means the special form even
inside a user's `(let ((if ...)) ...)`.

| Form | Expansion |
|---|---|
| identifier | its variable name, resolved as above |
| constant | itself |
| `(quote d)` | `d` with every alias replaced by its original |
| `(quasiquote t)` | `cons` / `list` / `append` / `quote` calls building `t`, expanding unquoted parts |
| `(lambda params body ...)` | fresh names for `params`, then the body expanded in the extended environment |
| body | see below |
| `(if ...)`, `(set! x e)`, `(begin ...)` | subforms expanded |
| `(define x e)` at top level | `e` expanded; `x` an alias inserted by a macro is renamed to a fresh global unless its original already holds a value |
| `(define-syntax k spec)` at top level | binds the global macro `k`; expands to void |
| `(let-syntax ((k spec) ...) body ...)` | the body expanded with the macros bound, each closed over the outer environment |
| `(letrec-syntax ((k spec) ...) body ...)` | likewise, each closed over the extended environment |
| `(k ...)`, `k` a macro | the transformer's result, expanded again |
| `(f arg ...)` | each element expanded |

**Bodies** (of `lambda`, `let-syntax`, `letrec-syntax`): each form is expanded
until its head isn't a macro use, with `(begin ...)` spliced.  The leading
`define`s and `define-syntax`es are then bound in one new environment level,
so all of them, and the rest of the body, can refer to each other (R7RS
5.4).  Then everything is expanded.

**`syntax-rules`** (R7RS 4.3.2): `(syntax-rules (literal ...) rule ...)` or
`(syntax-rules ellipsis (literal ...) rule ...)`, with a custom ellipsis
identifier.  Rules are tried in order, ignoring the keyword position.  A
pattern supports literals, `_`, the ellipsis (also nested, and followed by
more patterns), dotted patterns, and vector patterns.  A literal matches an
identifier with the same binding.  In a template, `(... template)` escapes
the ellipsis.  No match raises `bad syntax` with the form.  A template may
expand to `(syntax-error message arg ...)` (R7RS 4.3.3), which raises at
expansion time.

Instantiating a template copies pattern variables' matches unchanged and
replaces every other identifier with a fresh alias (one per name per
instantiation).  An alias the template binds then gets a fresh name distinct
from any user binding (no capture), and a free alias resolves in the macro's
definition environment (referential transparency):

```scheme
(define-syntax my-or
  (syntax-rules () ((_ a b) (let ((t a)) (if t t b)))))
(let ((t 5)) (my-or #f t))      ; 5: the template's t is a different variable
(let ((if list)) (my-or #f 1))  ; 1: the template's if is the special form

(let ((x 'outer))
  (let-syntax ((get-x (syntax-rules () ((_) x))))
    (let ((x 'inner))
      (get-x))))                ; outer: x means the binding visible at the definition
```

**Derived forms**, all `syntax-rules` macros in the prelude (R7RS 4.2):
`let` (including named `let`), `let*`, `letrec`, `letrec*`, `and`, `or`,
`when`, `unless`, `cond` (with `else` and `=>`), `case` (with `else` and `=>`
in both kinds of clause), and `do`.

## Phases

### Phase 1 - Quasiquote syntax

**Done.**  Details settled during the phase:

- `read_item` is split into skipping atmosphere plus `read_item_from(in, c,
  x)`.  After `,`, the next character may begin the datum and may be
  non-ASCII, which can't be pushed back, so it is handed on directly.
- The abbreviation helper takes a pointer to the symbol's global and reads it
  after the datum, since reading the datum may collect and move the symbol.
- The printer abbreviates a list's tail too, so `` `(1 . ,x) `` prints as
  written rather than as `` `(1 unquote x) `` (as Chez does).  A consequence:
  `(a quote b)` prints as `(a . 'b)`.  `(unquote @x)` stays unabbreviated.
- Errors: `eof after quasiquote`, `eof after unquote`, `eof after
  unquote-splicing`, matching `eof after quote`.

- `src/read.c`: `` `x ``, `,x`, and `,@x` read as `(quasiquote x)`,
  `(unquote x)`, and `(unquote-splicing x)`.
- `src/symbol.c`, `src/symbol.h`: the three symbols.
- `src/print.c`: prints them abbreviated, like `'x`.
- `docs/design/repl.md`: the reader and printer tables.

Separate because it is reader and printer only, with no evaluation change.

Tests: `tests/test_read.c` and `tests/test_print.c` cases for each, including
nesting (`` `(a ,(b ,@c)) ``); `tests/repl/echo.scm` quotes a quasiquoted
form.

```sh
cmake --build --preset dev && ctest --preset dev -R 'read|print|echo'
```

### Phase 2 - Expansion hook

**Done.**  Details settled during the phase:

- A `toplevel` entry in `minim_eval` resets `env`, then either evaluates the
  form or pushes a `K_EXPAND` frame and applies `%expand` to it on the value
  stack.  `load` now pushes a `K_TOPLEVEL` frame over the file's forms
  instead of running them as a `K_SEQ` body; an empty file needs no special
  case.
- The tests are a REPL golden test (`tests/repl/expand.scm`, with
  `load/expand.scm`) rather than `tests/test_eval.c`: defining `%expand` there
  would leak into the unit tests that follow.

- `src/symbol.c`, `src/symbol.h`: the symbol `%expand`.
- `src/eval.c`: top-level forms run through a new frame pair.  `K_EXPAND`
  receives an expanded form and evaluates it in the global environment;
  `K_TOPLEVEL` receives its value and starts the next form.  `minim_eval` and
  `load` both enter through them.  If `%expand` is unbound, a form is
  evaluated as is.
- `docs/design/eval.md`: the top-level loop.

Separate because it changes the evaluator's entry, with no expander yet.
Every existing test guards that unexpanded evaluation is unchanged.

Tests (`tests/test_eval.c`, also in `stress`): defining `%expand` as a
procedure that rewrites `(twice e)` to `(begin e e)` makes `(twice
(set! n (+ n 1)))` add 2, both at top level and in a loaded file.  A form is
expanded only after the previous one runs, so a `%expand` redefined by one
form applies to the next.

```sh
cmake --build --preset dev && ctest --preset dev -R 'eval|load'
cmake --build --preset stress && ctest --preset stress -R 'eval|load'
```

### Phase 3 - Uninterned symbols and aliases

**Done.**  Details settled during the phase:

- An alias's value slot holds `#(tag id env)` rather than `(id . env)`, with
  `tag` a private uninterned symbol: a renamed global (a gensym) may hold any
  value, including a pair, and must not look like an alias.
- `%bound?` is `#f` for an alias, whose value slot isn't a global value.
- `minim_intern` now creates its symbol with `minim_make_uninterned`, which
  shares an immutable name (another symbol's) instead of copying it, so an
  alias costs one symbol and one vector.
- `(gensym)` names are `g0`, `g1`, ...; `(gensym s)` takes a string.

- `src/symbol.c`, `include/minim.h`: `minim_make_uninterned(name)`, a symbol
  not in the symbol table.
- `src/prims.c`:
  - `(gensym [name])`, a fresh uninterned symbol;
  - `(%make-alias id env)`, an uninterned symbol named like `id` whose value
    slot holds `(id . env)`; `(%alias-original x)` and `(%alias-env x)` read
    it back, and return `#f` for anything that isn't an alias;
  - `(%bound? sym)`, whether the global `sym` holds a value.
- `docs/design/primitives.md`: these.

Separate because it is the C support the expander needs, testable alone.

Tests (`tests/repl/symbols.scm` / `.out`, and `tests/test_symbol.c`): an
uninterned symbol prints like its name but isn't `eq?` to the interned one
or to another uninterned symbol; an alias's original and environment survive
a collection; aliases of aliases; `%bound?` before and after a `define`.

```sh
cmake --build --preset dev && ctest --preset dev -R symbol
```

### Phase 4 - Expander and quasiquote

**Done.**  Details settled during the phase:

- Environments are lists of frames; a frame is a one-element list holding an
  association list, so a body adds bindings in place as it scans.  A
  variable's meaning is its gensym; Phase 6 adds macro meanings.
- A new primitive, `(%bad-syntax form)`, raises `bad syntax` from Scheme.
- `define` outside the top level or a body's leading definitions is now `bad
  syntax`, as in R7RS, and malformed forms raise when expanded rather than
  when run.  No existing test relied on the old behaviour.
- Quasiquote folds constant parts into a single `quote`; vectors use `(apply
  vector ...)`.  `,@` outside a list element is `bad syntax`.
- The test is `tests/repl/expander.scm`, since `expand.scm` is Phase 2's hook
  test.
- `test_tail_calls` compared the value stack's capacity with 64; expanding
  the prelude recurses, leaving it at 128, so the test now checks that the
  loops don't grow it.
- Release startup: about 2.4 ms before, 10 ms after.  Stress startup
  (collecting on every allocation): 0.14 s before, 28 s after, since every
  collection copies the whole prelude.

- `src/prelude.scm`, first: `%expand` with syntactic environments,
  resolution, renaming of `lambda` parameters and internal `define`s, body
  scanning, and `quasiquote`.  The global macro table (`%macros`) and macro
  dispatch are in place but empty.
- `docs/design/macros.md` (new, linked from `docs/design/README.md`):
  expansion.

Separate because the walk, renaming, and quasiquote are testable without
`syntax-rules`, which is the largest single piece.

Tests (`tests/repl/expand.scm` / `.out` / `.err`):

- quasiquote with `unquote` and `unquote-splicing` at the head, middle, and
  tail, in dotted and vector templates, and nested;
- `(lambda (if) (if 1))` and `(lambda (quote) quote)` treat the parameter as
  a variable; internal `define`s shadow globals throughout their body and can
  refer to each other;
- `quote`d data is left alone;
- `,@` of a non-list raises;
- every existing golden test still passes, now fully expanded.

```sh
cmake --build --preset dev && ctest --preset dev -R repl
```

### Phase 5 - Global macros and hygienic `syntax-rules`

**Done.**  Details settled during the phase:

- A transformer is a procedure `(form use-env)` → expansion.  `%syntax-rules`
  builds one closed over the spec and its definition environment, so Phase 6
  passes a local environment instead of `'()`.
- The ellipsis (default or custom), `_`, and literals are recognized by
  resolution: a pattern identifier is the ellipsis if it resolves like it in
  the definition environment, and a literal matches an input identifier that
  resolves (in the use environment) as the literal does (in the definition
  one).
- Pattern variables are bound to `(depth . match)`; a match at depth n > 0 is
  the list of depth n - 1 matches, so `x ... ...` flattens one level per
  ellipsis.  A variable used at the wrong depth, or ellipsis variables of
  unequal lengths, raise `bad syntax`.
- Aliases: one per template identifier per use, from a table the transformer
  resets on each call.
- Inserted top-level `define`: `%top-name` assigns the original when it holds
  a value; otherwise it defines a fresh global and records `(alias . name)`
  in `%top-renames`, which `%resolve` consults before an alias's original, so
  the same use's other insertions find it while user code sees the original.
  An inserted `define-syntax` binds its original name (simpler; R7RS leaves
  it open).
- A top-level `define` of a macro's name removes the macro.
- `syntax-error` is a new primitive, `(%syntax-error message args)`, whose
  message is a Scheme string: converted with `minim_string_to_utf8` into a
  buffer kept until the next syntax error, since `minim_error` keeps the
  pointer.  The irritant is void, the single argument, or the list.
- `define-syntax` outside the top level is `bad syntax` until Phase 6.
- `docs/design/macros.md` is not touched (policy); its `syntax-rules` and
  hygiene sections go in the post-plan sync.
- Release startup: 12.8 ms (20 runs: 0.256 s), from about 10 ms.

- `src/prelude.scm`: the matcher and template instantiator, with custom
  ellipsis, `(... ...)`, and aliasing; top-level `define-syntax`;
  `syntax-error`; and the rule for top-level definitions inserted by a macro.
- `docs/design/macros.md`: `syntax-rules` and hygiene.

Separate because it is the core of the milestone and needs its own review.

Tests (`tests/repl/syntax-rules.scm` / `.out` / `.err`):

- literals, `_`, single and nested ellipsis, ellipsis followed by more
  patterns, dotted and vector patterns, multiple rules tried in order, a
  custom ellipsis, and `(... ...)` producing a literal `...`;
- a macro whose expansion uses another macro;
- hygiene: the two `my-or` cases above, a macro-introduced binding invisible
  to user code passed in, and a template's quoted symbol printing as written;
- top-level definitions: an inserted `(define x ...)` assigns an existing
  global `x`, but defines a private global when `x` is unbound, so a later
  user reference to `x` is unbound;
- errors: no rule matches; a transformer other than `syntax-rules`; a
  template expanding to `syntax-error`, which reports its message and
  arguments.

```sh
cmake --build --preset dev && ctest --preset dev -R 'syntax-rules|repl'
```

### Phase 6 - Local macros

- `src/prelude.scm`: `let-syntax`, `letrec-syntax`, and `define-syntax` in
  bodies.  Macro bindings in syntactic environments, closed over their
  definition environment, which instantiated aliases carry.
- `docs/design/macros.md`: local macros.

Separate because it generalizes Phase 5's global-only definition environment,
and its tests target that change.

Tests (`tests/repl/local-macros.scm` / `.out` / `.err`):

- the `get-x` example above: a local macro's free identifier refers to the
  binding visible at its definition;
- `let-syntax` versus `letrec-syntax`: a macro referring to itself, or to a
  sibling, only works in `letrec-syntax`;
- an internal `define-syntax` used by an internal `define` before and after
  it in the same body;
- a local macro shadowing a global one, and a local variable shadowing a
  macro.

```sh
cmake --build --preset dev && ctest --preset dev -R 'macros|repl'
```

### Phase 7 - Derived forms

- `src/prelude.scm`: the derived forms, as macros, defined right after
  `syntax-rules`.  The prelude after them may use them; the list library is
  rewritten with `let`, `cond`, `and`, and `or` where that reads better.
- `docs/design/macros.md`: the derived forms.
- `docs/roadmap.md`: milestone 3 done, and the hygiene open item settled.

Separate because it is pure Scheme on top of Phases 5 and 6.

Tests (`tests/repl/derived.scm` / `.out` / `.err`, also in `stress`):

- each form, including named `let` loops, `let*` sequencing, mutually
  recursive `letrec` procedures, `cond` with `=>` and `else`, `case` with
  lists of data, `=>`, and `else`, `and` and `or` returning the last value,
  and a `do` loop;
- `or`, `cond =>`, and `case` don't capture a user variable named like their
  temporaries;
- `and`, `or`, and `when` in tail position run in constant space;
- every existing golden and C test still passes on the rewritten prelude.

```sh
cmake --build --preset dev && ctest --preset dev
cmake --build --preset stress && ctest --preset stress
```

### After the last phase

**Changes on this branch outside the phases**, made after rebasing onto the
generational collector:

- Spare chunks: the limit counts chunks in use (`minim_heap_max_spares`: in
  use + the nursery's worth + the quarantine), and `minim_heap_trim_spares`
  trims the queue after each collection.  The old limit (the nursery's worth
  plus the quarantine) made every major collection under stress allocate
  fresh chunks and free the surplus, which kept ASan's 256 MB quarantine full:
  stress tests peaked at about 490 MB each, now 16-148 MB.
- CI runs `ctest --preset <preset> -j`, one job per processor (CMake 3.29+).

Sync the design docs: `macros.md` (and every doc Phases 1-4 touched) against
the code, adding `syntax-rules`, hygiene, top-level definitions inserted by
macros, `syntax-error`, local macros, and the derived forms; `primitives.md`
for `%syntax-error`; `gc.md`'s spare-chunk paragraph for the new limit and
trimming.

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress
```

Also measure startup time (`time ./build/release/minim < /dev/null`) and the
stress suite's time before and after, and record both here: every
top-level form, including the prelude's own, now runs through an interpreted
expander.

## Open items

None.

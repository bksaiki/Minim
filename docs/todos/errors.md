# Errors: raise, handlers, guard, and error objects

Implementation plan.  The design is settled; what follows is the phase
breakdown, one phase per commit.

This finishes roadmap milestone 4, after continuations and `dynamic-wind`,
which `guard` is built from.

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

An error today is C only (`errors.md`): `minim_error(msg, irritant)`
`longjmp`s to the innermost `minim_catch`, and the REPL prints `error: msg:
irritant`.  Scheme can neither raise nor handle one:

| Input | Today |
|---|---|
| `(error "x" 1)` | `unbound variable: error` |
| `(raise 'x)` | `unbound variable: raise` |
| `(with-exception-handler (lambda (e) 0) (lambda () 1))` | `unbound variable: with-exception-handler` |
| `(guard (e (#t 1)) 2)` | `unbound variable: guard` |
| `(car 5)`, inside any of the above | straight to the REPL: `error: car: not a pair: 5` |

R7RS 6.11 requires `raise`, `raise-continuable`, `with-exception-handler`,
`error`, the error-object accessors, `read-error?`, and `file-error?`; 4.2.7
requires `guard`.  An error also skips the `after`s of the `dynamic-wind`s
it escapes, and `minim_catch` resets the winders to none.

## Design

**Error objects** are a new typed object, `[header | kind | message |
irritants]`: `kind` is `error`, `read`, or `file`; `message` a string;
`irritants` a list.  `error-object?`, `error-object-message`,
`error-object-irritants`, `read-error?`, and `file-error?` read them.  They
print as `#<error message>`.  Their type code is 1, the first odd code: the
even codes are taken, and an odd one can't equal the collector's forwarding
marker, `0x3E`.  (Chez makes error objects records, whose header is their
descriptor pointer, tagged `0x7`; Minim may follow when it has records.)

**Handlers** are a list, innermost first, held like the winders:
`minim_gc.handlers`, a collector root, read and set by `%handlers` and
`%set-handlers!`, and reset to none by `minim_catch`.  In the prelude, after
the R7RS reference implementation:

- `(with-exception-handler handler thunk)` pushes `handler` for `thunk`'s
  dynamic extent, through `dynamic-wind`, so continuations restore the
  handlers along with the winders.
- `(raise obj)` calls the innermost handler on `obj`, with the handlers below
  it installed; if the handler returns, that is an error.
  `(raise-continuable obj)` returns the handler's values instead.
- `(error message irritant ...)` raises a new error object.
- With no handler, a raise leaves Scheme through `%raise-uncaught`, which
  `longjmp`s to `minim_catch` as `minim_error` does: an error object as its
  message and irritants, anything else as `uncaught exception` with the
  object as the irritant.

**C errors raise in Scheme.**  `minim_error` stays the C API, and
`minim_catch` stays the way out to C.  But `eval` catches its own errors:
if any handler is installed, it makes an error object of the message and
irritant (irritants `(irritant)`, or `()` for none) and raises it on the
machine, calling the handler with a frame that raises `handler returned` if
it does.  With no handler, the error passes to the enclosing `minim_catch` as
today, so the REPL's output doesn't change.

- **Registers across the jump.**  After a `longjmp`, a local changed since
  the `setjmp` is indeterminate unless `volatile`.  The handler call needs
  only `k`, the continuation at the error, so `k` is `volatile`; the value
  stack is left as it was, below the handler call, since a non-continuable
  raise never returns to it.
- **Kinds.**  The reader's errors are read errors, and `load`'s failure to
  open a file a file error: `minim_error_kind(kind, msg, irritant)`, with
  `minim_error` the `error` kind.

**`guard`** is a `syntax-rules` macro after R7RS 7.3: it calls its body under
a handler that escapes, with `call/cc`, to the `guard`'s continuation, tests
the clauses there (`else`, `=>`), and re-raises with `raise-continuable` in
the original dynamic environment if none matches.  Since it escapes by a
continuation, the `after`s of the `dynamic-wind`s it leaves run.

## Phases

### Phase 1 - Error objects

- `include/minim.h`, `src/object.c`, `src/gc.c`, `src/print.c`: the type,
  its constructor and accessors, tracing, size, and printing.
- `src/prims.c`: `error-object?`, `error-object-message`,
  `error-object-irritants`, `read-error?`, `file-error?`, and
  `%make-error-object kind message irritants`.

Separate because it is a new object kind, reviewable before anything raises
one.

Tests: `tests/repl/errors.scm` (new): each predicate and accessor on objects
of each kind and on non-error objects, printing, and survival across a
collection.

```sh
cmake --build --preset dev && ctest --preset dev -R 'errors|object|gc' && ctest --preset stress -R 'errors'
```

### Phase 2 - raise and handlers

- `src/gc.h`, `src/gc.c`, `src/error.c`, `src/prims.c`: `minim_gc.handlers`,
  `%handlers`, `%set-handlers!`, `%raise-uncaught`; `minim_catch` resets the
  handlers.
- `src/prelude.scm`: `with-exception-handler`, `raise`,
  `raise-continuable`, `error`.  `src/prelude.boot` regenerated.

Separate because it is Scheme raising Scheme, testable before C errors take
part.

Tests (`tests/repl/errors.scm`): a handler's value from `raise-continuable`;
a handler escaping with `call/cc`; nested handlers, the inner one called
first and with the outer installed; a handler returning from `raise` is an
error; `error`'s object; an uncaught `raise` of an error object and of
another object, as the REPL prints them; a `dynamic-wind` around a raise
that escapes runs its `after`.

```sh
cmake --build --preset dev && ctest --preset dev -R 'errors|boot|continuations' && ctest --preset stress -R 'errors'
```

### Phase 3 - C errors raise in Scheme

- `src/eval.c`: `eval` catches errors raised while it runs and, with a
  handler installed, raises an error object; `k` `volatile`.
- `src/error.c`, `include/minim.h`: `minim_error_kind`; `src/read.c` and
  `load` use it for read and file errors.

Separate because it changes how every C error travels, with the REPL's
output for unhandled errors unchanged.

Tests (`tests/repl/errors.scm`, `tests/test_eval.c`): a handler sees `(car
5)`, `unbound variable`, `arity mismatch`, and a bad `apply` as error objects
with their messages and irritants; a read error from `load` is a
`read-error?`, a missing file a `file-error?`; an error inside a handler; the
value stack and winders after a handled error deep in a recursion; every
existing `.err` file unchanged.

```sh
cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
```

### Phase 4 - guard

- `src/expander.scm`: `guard` among the derived forms.  `src/prelude.boot`
  regenerated twice.

Separate because it is syntax on top of Phases 2 and 3.

Tests (`tests/repl/errors.scm`): clauses with `else` and `=>`; no clause
matching re-raises to an outer `guard`; a C error caught; the `after`s of
the `dynamic-wind`s left run; the body's several values returned; `guard` in
tail position of a loop runs in constant space (`test_eval`).

```sh
cmake --build --preset dev && ctest --preset dev -R 'errors|expan|boot' && ctest --preset stress -R 'errors'
```

### After the last phase

Sync the docs: `errors.md` (error objects, handlers, C errors raising in
Scheme, kinds, `minim_catch`), `eval.md` (the machine's catch and the
handler call), `primitives.md` (the error-object procedures and `%`
primitives), `prelude.md` (`with-exception-handler`, `raise`,
`raise-continuable`, `error`), `macros.md` (`guard`), `gc.md` (the error
object's layout and the handlers root), and the roadmap (milestone 4 done).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

### Do unhandled errors run the `after`s?

An error no handler catches `longjmp`s to `minim_catch`, past the
`dynamic-wind`s it leaves, without running their `after`s.  R7RS leaves this
to the implementation; Chez runs them, printing the error first, then
resetting the REPL by a continuation captured at its prompt.  Running them means the top level reinstating its own
continuation, like an escape, before printing the error; skipping them is
today's behaviour and simpler.

Provisionally skip them, as now.  Reopen if programs rely on `after` for
cleanup that must run on an uncaught error.

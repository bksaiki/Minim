# Errors

```c
_Noreturn void minim_error(const char *msg, minim_obj irritant);
_Noreturn void minim_error_of_kind(minim_error_kind kind, const char *msg, minim_obj irritant);
bool minim_catch(minim_obj (*fn)(void *), void *arg, minim_obj *result);
```

`minim_error` records `msg` and `irritant` and `longjmp`s to the innermost
catch.  `minim_catch` installs one: on an error it restores the root stack and
the value stack to their heights on entry, empties `minim_gc.spill`, and
returns `false`.  `minim_last_error` and `minim_write_error` retrieve the
error.

- `msg` is stored as a pointer, so it must outlive the catch; in practice it
  is a literal.
- `MINIM_VOID` as the irritant means there is none.
- The irritant is a GC root, so it survives collections.
- Catches nest.  With none installed, `minim_error` prints and aborts.
- `minim_catch` is called outside any evaluation, with the value stack
  empty: a continuation from outside, invoked inside, would replace what lies
  below.
- The kind is general for `minim_error`; read and file errors are made where
  `minim_read_file` fails (`load`).

## In Scheme

An error is an *error object*, a typed object `[header | kind | message |
irritants]` (`gc.md`): the kind (general, read, or file), a string, and a
list.  It prints as `#<error message>`.  Any object can be raised.

Handlers are a list, innermost first, in `minim_gc.handlers`, read and set
by `%handlers` and `%set-handlers!`.  The prelude (`prelude.md`) installs
them through `dynamic-wind`, so a continuation restores the handlers along
with the winders.  A handler runs at the raise, inside every `dynamic-wind`
around it, with the handlers outside it installed; nothing unwinds until
control jumps, as when `guard` escapes.

**C errors.**  `minim_eval` holds a catch around the machine (`eval.md`).
If a handler or a `dynamic-wind` is in progress when an error reaches it, it
makes an error object of the error, with irritants `(irritant)` or `()`, and
applies `%raise-c-error` to it on a fresh machine: no frames, and the value
stack as `minim_eval` found it, since a non-continuable raise never returns.
That raises to the handlers; with none, it runs the `after`s of every
`dynamic-wind` in progress, innermost first, then leaves through
`%raise-uncaught`.  Otherwise the error goes on to the enclosing catch
unchanged.

**Uncaught.**  `%raise-uncaught` raises an error object's message and
irritants as `minim_error` does, or, for any other object, `uncaught
exception` with the object as the irritant.  Several irritants become one,
their list, as for `%syntax-error`.  So an error that leaves `minim_eval`
leaves no `dynamic-wind` in progress, and the REPL prints it after the
`after`s ran.

**Order.**  Chez prints an uncaught error first, then runs the `after`s: its
default handler runs at the raise, like any handler, then resets to the REPL.
Here the REPL prints in C after `minim_catch` returns, so the `after`s run
first.  Matching Chez would take the REPL rewinding after it prints, or a
Scheme-level default handler.

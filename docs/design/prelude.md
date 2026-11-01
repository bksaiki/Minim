# Prelude

The prelude is the Scheme code compiled into the binary and evaluated by
`minim_init`: a list library (`src/prelude.scm`), then the expander and the
derived forms (`src/expander.scm`, `macros.md`).  The library holds what is
easiest in Scheme, above all higher-order procedures, which primitives can't
express because they can't call closures (see `primitives.md`).

## The boot file

Minim loads `src/prelude.boot`, both files expanded into core forms, one
top-level form per line.  It is checked in, so the expander that writes it is
the one it contains.

- `boot_gen` (`src/boot.c`) writes it: it expands each form of the two
  files, in order, without evaluating it, with `%boot-form`
  (`src/boot.scm`).  That splices a `begin`; writes a `define-syntax` as
  `(%define-macro! 'name 'spec)`, since a transformer is a closure; renames
  each fresh name to `name.N`, numbered per name within its form, so an edit
  doesn't renumber other forms; and fails if a macro defines a fresh global.
- After editing either file, build the `boot` target, which rewrites
  `src/prelude.boot`.  After an expander change, build it twice: the first
  run expands with the old expander.  The `boot` test fails if the file is
  out of date; since `boot_gen` starts from it, a pass also means it
  reproduces itself.  A boot file that can't regenerate itself is recovered
  from an older one in git.

## Loading

- At build time, `cmake/embed.cmake` writes the bytes of `src/prelude.boot`
  to a generated `prelude_boot.c` as `const unsigned char
  minim_prelude_boot[]`, NUL-terminated.  A byte array, not a string literal,
  avoids MSVC's string-literal length limit.
- `minim_prelude_init` (`src/prelude.c`), called by `minim_init`, reads
  each form with `minim_read_string` and evaluates it with
  `minim_eval_expanded`, without expansion.  No catch is installed, so an
  error in the prelude, which is a bug in Minim, aborts with the message.
- The forms load in order: the list library, the expander, the derived
  forms.  Each `%define-macro!` builds a transformer as it loads, so the
  expander code that runs then may call only what is defined before it.

## Library

| Procedure | Result |
|---|---|
| `(list? x)` | whether `x` is a proper list; `#f` for cyclic lists |
| `(append lst ...)` | the lists joined; all but the last are copied, the last is shared |
| `(reverse lst)` | a new list in reverse order |
| `(list-tail lst k)`, `(list-ref lst k)` | the `k`th tail, or element |
| `(list-copy lst)` | a copy of the pairs of `lst`; an improper tail is kept |
| `(memq x lst)`, `(memv x lst)`, `(member x lst [compare])` | the first tail whose car matches `x` by `eq?`, `eqv?`, or `compare` (default `equal?`); `#f` if none |
| `(assq x alist)`, `(assv x alist)`, `(assoc x alist [compare])` | the first pair whose car matches `x`, likewise; `#f` if none |
| `(map f lst ...)` | a list of `f` applied to corresponding elements, left to right, stopping at the shortest list |
| `(for-each f lst ...)` | like `map`, for effect; void |
| `(filter keep? lst)` | the elements satisfying `keep?`, in order |
| `(fold kons knil lst)`, `(fold-right kons knil lst)` | SRFI-1 folds, calling `(kons elem acc)` from the left or right |
| `(call/cc f)`, `(call-with-current-continuation f)` | `f` applied to the current continuation, a procedure taking any number of values to return |
| `(dynamic-wind before thunk after)` | calls `before`, `thunk`, then `after`, returning `thunk`'s values; a continuation leaving `thunk`'s extent runs `after`, and one entering it runs `before` |

| `(with-exception-handler handler thunk)` | calls `thunk` with `handler` installed for its extent |
| `(raise obj)` | calls the innermost handler on `obj`, with the handlers outside it installed; if it returns, raises `handler returned` with irritants `(obj)` to them |
| `(raise-continuable obj)` | likewise, returning the handler's values |
| `(error message irritant ...)` | raises a new error object |

`call/cc` wraps `%call/cc` (`primitives.md`): the procedure it passes runs
the `after`s of the `dynamic-wind`s it leaves, innermost first, and the
`before`s of those it enters, outermost first, before jumping.

The exception procedures follow the R7RS reference implementation
(`errors.md`).  `%with-handlers` installs a list of handlers through
`dynamic-wind`.  `%guard`, which `guard` expands into (`macros.md`), calls
its body with a handler that escapes to the `guard`'s continuation and runs
the clauses there; if none matches, it jumps back and re-raises with
`raise-continuable`.  `%raise-c-error` is what errors from C raise through.

`define-record-type` (`macros.md`) binds procedures from `%record-constructor`,
`%record-predicate`, `%record-accessor`, and `%record-modifier`, which find
each field's index once, when the definition runs.  An accessor or modifier
checks the type with `%record?`, then reads or sets the field, raising
`name: not a type` with the record as the irritant otherwise.

Prelude procedures print as `#<procedure>`, since closures have no names.
Errors inside them come from the primitives they call, except those of
`raise`.

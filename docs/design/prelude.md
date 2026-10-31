# Prelude

The prelude is a library written in Scheme (`src/prelude.scm`), compiled into
the binary and evaluated by `minim_init`.  It holds what is easiest in Scheme,
above all higher-order procedures, which primitives can't express because
they can't call closures (see `primitives.md`).

## Build and loading

- At build time, `cmake/embed.cmake` writes the bytes of `src/prelude.scm` to
  a generated `prelude_scm.c` as `const unsigned char minim_prelude_scm[]`,
  NUL-terminated.  Editing the `.scm` file regenerates it.  A byte array, not a
  string literal, avoids MSVC's string-literal length limit.
- `minim_prelude_init` (`src/prelude.c`), called by `minim_init`, reads
  each form with `minim_read_string` and evaluates it.  No catch is installed,
  so an error in the prelude, which is a bug in Minim, aborts with the
  message.
- The prelude starts with the expander (`macros.md`), which uses only the core
  forms (`quote`, `if`, `define`, `set!`, `lambda`, `begin`), then defines the
  derived forms.  Every form after the expander is expanded, and the library
  uses the derived forms.

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

Prelude procedures print as `#<procedure>`, since closures have no names.
Errors inside them come from the primitives they call.

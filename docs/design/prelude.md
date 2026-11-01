# Prelude

The prelude is a library written in Scheme (`src/prelude.scm`), expanded
into core forms, compiled into the binary, and evaluated by `minim_init`.
It holds what is easiest in Scheme, above all higher-order procedures,
which primitives can't express because they can't call closures (see
`primitives.md`).

## The boot file

Minim loads `src/prelude.boot`, the prelude expanded into core forms, one
top-level form per line.  It is checked in, so the expander that writes it is
the one it contains.

- `boot_gen` (`src/boot.c`) writes it: it expands each form of
  `src/prelude.scm`, without evaluating it, with `%boot-form`
  (`src/boot.scm`).  That splices a `begin`; writes a `define-syntax` as
  `(%define-macro! 'name 'spec)`, since a transformer is a closure; renames
  each fresh name to `name.N`, numbered per name within its form, so an edit
  doesn't renumber other forms; and fails if a macro defines a fresh global.
- After editing `src/prelude.scm`, build the `boot` target, which rewrites
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
- The prelude starts with the expander (`macros.md`), then defines the
  derived forms and the library.

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

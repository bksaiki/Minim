# Errors

```c
_Noreturn void minim_error(const char *msg, minim_obj irritant);
bool minim_catch(minim_obj (*fn)(void *), void *arg, minim_obj *result);
```

`minim_error` records `msg` and `irritant` and `longjmp`s to the innermost
`minim_catch`.  That catch restores the root stack and the value stack to
their heights on entry, empties `minim_gc.spill`, sets the `dynamic-wind`s in
progress to none, and returns `false`.  `minim_last_error` and
`minim_write_error` retrieve the error.

- `minim_catch` is called outside any evaluation, with the value stack
  empty: a continuation from outside, invoked inside, would replace what lies
  below.  So no `dynamic-wind` is in progress at entry.
- The error skips the `after`s of the `dynamic-wind`s it escapes.

- `msg` is stored as a pointer, so it must outlive the catch; in practice it
  is a literal.
- `MINIM_VOID` as the irritant means there is none.
- The irritant is a GC root, so it survives collections.
- Catches nest.  With none installed, `minim_error` prints and aborts.

# Errors

```c
_Noreturn void minim_error(const char *msg, minim_obj irritant);
bool minim_catch(minim_obj (*fn)(void *), void *arg, minim_obj *result);
```

`minim_error` records `msg` and `irritant` and `longjmp`s to the innermost
`minim_catch`.  That catch restores the root and value stacks to their heights
on entry and returns `false`.  `minim_last_error` and `minim_write_error` retrieve the
error.

- `msg` is stored as a pointer, so it must outlive the catch; in practice it
  is a literal.
- `MINIM_VOID` as the irritant means there is none.
- The irritant is a GC root, so it survives collections.
- Catches nest.  With none installed, `minim_error` prints and aborts.

# Environment variables

Every environment variable Minim reads.  `minim_init` reads each one once,
so they apply to every program that embeds the library, including the `minim`
executable and the tests.  An unset or empty variable leaves the default.  A
malformed value prints a message naming the variable and exits with status 1.

Code reads them only through `minim_getenv(name)` (`src/system.h`), which
returns the value, or NULL if it is unset or empty; the caller converts it.
A new variable goes in the table below.

| Variable | Value | Default | Meaning |
|---|---|---|---|
| `MINIM_GC_NURSERY_BYTES` | positive integer | 1048576 (1 MiB) | bytes allocated between collections |
| `MINIM_GC_RADIX` | nonnegative integer | 0 | every *n*-th collection is major; 0 for none |
| `MINIM_GC_MAJOR_GROWTH` | positive integer | 2 | a major collection follows when the old generation grows by this factor |
| `MINIM_GC_MAJOR_MIN_BYTES` | nonnegative integer | 1048576 (1 MiB) | ...and exceeds this many bytes |
| `MINIM_GC_STARTUP` | 0 or 1 | 0 | 1 applies the settings above from the start of `minim_init`, not after it |

Values must be decimal and at most the largest fixnum, 2^60 - 1.

The `MINIM_GC_` variables are the collector settings (`gc.md`).  A program can
still change them, with `minim_gc_set_config` or the `collect-` primitives.

Collecting at every allocation, with every collection major, exposes missing
GC roots under AddressSanitizer:

```sh
MINIM_GC_NURSERY_BYTES=1 MINIM_GC_RADIX=1 ./build/dev/minim file.scm
```

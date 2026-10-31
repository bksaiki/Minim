# Environment variables

Every environment variable Minim reads.  `minim_init` reads each one once,
so they apply to every program that embeds the library, including the `minim`
executable and the tests.  An unset or empty variable leaves the default.  A
malformed value prints a message naming the variable and exits with status 1.

Code reads them only through `minim_getenv(name)` (`src/system.h`), which
returns the value, or NULL if it is unset or empty; the caller converts it.
A new variable goes in the table below.

| Variable | Value | Default | Sets |
|---|---|---|---|
| `MINIM_GC_NURSERY_BYTES` | positive integer | 1048576 (1 MiB) | `nursery_bytes` |
| `MINIM_GC_RADIX` | nonnegative integer | 0 | `radix` |
| `MINIM_GC_MAJOR_GROWTH` | positive integer | 2 | `major_growth` |
| `MINIM_GC_MAJOR_MIN_BYTES` | nonnegative integer | 1048576 (1 MiB) | `major_min_bytes` |
| `MINIM_GC_STARTUP` | 0 or 1 | 0 | 1 applies the settings above from the start of `minim_init`, not after it |

Values must be decimal and at most the largest fixnum, 2^60 - 1.

The settings are the collector's (`gc.md`); a program can still change them,
with `minim_gc_set_config` or the `collect-` primitives.

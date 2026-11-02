# Roadmap: beyond R7RS-small

Work that R7RS-small doesn't require: the runtime's maturity, tooling, and
extensions.  It interleaves with the R7RS-small milestones (`r7rs-small.md`);
each item gets an implementation plan in `docs/todos/` when it starts.

## Runtime

Not features, but needed as programs grow.

- Done: a generational collector on Chez's radix schedule, with a write
  barrier, reuse of freed chunks, and run-time collector settings
  (`docs/design/gc.md`).
- Done: startup loads the prelude already expanded, from a checked-in boot
  file (`docs/design/prelude.md`), so it expands nothing and runs under the
  environment's collector settings throughout.
- Done: the expander is written in normal Scheme and regenerates its own
  boot file; the evaluator binds definitions only globally.
- Done: expansion into the `-values` binding forms, so `let` allocates no
  closure and each frame's variables are known before it runs.
- Done: local frames are vectors, one allocation each, looked up by name
  (`docs/design/eval.md`).
- Done: benchmarks, in `bench/` (`docs/design/testing.md`).
- Lexical addresses: a pass after expansion resolves each local variable to
  (frames up, slot), so a lookup is a few loads instead of a search by name
  (`docs/todos/compile.md`).
- A weak symbol table, so unused symbols are collected, as Chez does.

## Libraries

- Done: the Scheme sources in `scheme/`, one file per part; the boot
  generator in `tools/`; the primitives in `src/prims/`, one file per area
  (`docs/design/prelude.md`, `docs/design/primitives.md`).
- The standard libraries written as `.sld` files, each re-exporting from a
  core library of the system's bindings, and expanded into the boot file, so
  nothing is read from disk at run time.

## Data types

Not in R7RS-small; in R7RS-large and SRFIs.

- Hashtables: `eq?`-based and `equal?`-based (`equal-hash` exists).  `eq?`
  hashing must survive the moving collector, by rehashing after a collection
  or by hashing a stable per-object value.  They would also hold an
  environment's bindings, should association lists grow too slow.
- Boxes.

## Tooling

- Source locations in error messages.
- Line editing in the REPL, if it can be done without a dependency.

## Out of scope for now

Threads, a foreign function interface, a native or bytecode compiler, and
Unicode case folding and normalization.

## Open items

### Library in C or in Scheme?

- **Scheme (the prelude)** is shorter and handles higher-order procedures
  naturally, but runs slower and grows startup time.
- **C** is fast but verbose, and can't call closures.
- **Provisional:** higher-order and list-processing procedures in Scheme;
  type predicates, accessors, and arithmetic in C.  Move hot procedures to C
  when benchmarks show it.

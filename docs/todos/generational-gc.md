# Generational collection: roadmap milestone 8, in part

Implementation plan.  The design is settled; what follows is the phase
breakdown, one phase per commit.

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
  they never describe an intermediate state.  Phases 1-4 edited them before
  this rule; the sync covers whatever they still get wrong.

## Context

The collector (`src/gc.c`, `docs/design/gc.md`) copies every live object on
every collection.  Long-lived data, such as loaded code, a program's tables,
or the prelude, is therefore copied again and again, though it rarely dies.

All measurements below are release builds of `macros` at b115a5e:

| Program | Time | Max RSS |
|---|---|---|
| allocate 3,000,000 short-lived pairs | 1.85 s | 3 MB |
| the same, with a 1,000,000-element list kept live | 3.52 s | 65 MB |

The same allocation takes twice as long when large data stays live.  The
budget, 2 × live, keeps the amortized cost bounded, but every collection
still copies the whole list, and the heap needs room for three copies of it.

Stress mode (`MINIM_GC_STRESS`) collects on every allocation, so it pays the
cost of all live data on every allocation.  Since the macros branch expanded
the prelude, this dominates:

| Measurement | Value |
|---|---|
| allocated during `minim_init` | 8.5 MB |
| live after `minim_init` | 50 KB |
| stress startup (ASan) | 28 s, up from 0.14 s |
| stress startup, no sanitizers | 11.5 s |

A profile (gprof, no sanitizers) puts `copy` and the scan at about 60% of the
time, plus unattributed libc time.

An instrumented run collected every 64 allocations during startup.  On
average, 5.4 KB of the 50 KB live at each collection had been created since
the previous top-level form finished.  A collection that copied only recent
data would copy about 9× less.

This is the weak generational hypothesis: most objects die young, and those
that don't tend to live long.  A generational collector copies an object
roughly once, when it is promoted, rather than at every collection.

**Out of scope:**
- **Chunk reuse**, at first: reusing from-space chunks would hide stale
  pointers from AddressSanitizer, and catching those is what stress mode is
  for.  Phase 8 adds it back with poisoning and a quarantine.
- **A static space**, weak references, guardians, mark-in-place, and evaluator
  speed.

## Chez's approach

Chez Scheme ("Don't Stop the BIBOP", Dybvig, Eby, and Bruggeman 1994; CSUG
chapter 13) has several generations:

- **Generations.**  New objects start in generation 0.  A collection of
  generation `g` promotes its survivors into generation `g + 1`, up to
  `collect-maximum-generation` (default 4).
- **Radix.**  The collection radix (default 4) sets the schedule: generation
  0 every collection, generation 1 every 4th, generation 2 every 16th, and so
  on.  Older data is copied geometrically less often, and data that lives a
  medium time dies in a middle generation without reaching the oldest one.
- **Static generation.**  The oldest generation is never collected.  Objects
  reach it only when a heap image is saved.
- **Bookkeeping.**  A segment table records each segment's generation.  A
  card-marking write barrier records old objects that point into younger
  generations, and the "dirty sweep" of a collection treats them as roots.
- **Hybrid mark-copy** (from version 9.9.9).  Mostly-live or large segments
  are marked in place instead of copied.

**Minim takes the two-generation case:** young and old, with old collected
when it has doubled.  The design keeps the path to Chez's scheme open:

- the generation is a number in each chunk's header, not a flag;
- the barrier remembers *objects*, and a collection of generations `0..g`
  would scan the remembered objects older than `g`.

Going to more generations means adding:
- per-generation spaces;
- a radix schedule;
- a barrier that records the youngest generation each old object points to,
  because promotion would no longer empty the young generations.

## Design

**Two generations.**

- **Young:** the existing new space, where every object is allocated, all
  kinds interleaved.
- **Old:** the existing per-kind pair, symbol, and typed spaces, which today
  hold survivors only until the next collection.  They become permanent until
  a major collection.

**Minor collection.**

- The from-space is the young chunks only.
- `copy` returns old objects unchanged.
- Roots: globals, locals, the value stack, and the remembered set.
- Every survivor is promoted into the old spaces at once (en-masse
  promotion).  The scan starts at each old space's end before the collection,
  so it visits only promoted objects.
- Afterward there are no young objects, so the remembered set is empty.

**Major collection.**

- Today's collector: from-space is young plus old, to-space is fresh old
  spaces.
- `minim_gc_collect()` stays a major collection, since its callers, mostly
  tests, expect everything to move.
- A minor collection is `minim_gc_collect_minor()` in `gc.h`.

**Collection policy**, set at run time as in Chez (`collect-trip-bytes`,
here the nursery size, and `collect-generation-radix`):

| Setting | Meaning | Default |
|---|---|---|
| nursery bytes | allocation between minor collections | 1 MiB, today's minimum budget |
| radix | every *r*-th minor collection is major; 0 means never by count | 0 |

- **Major collection by growth:** after a minor collection, a major one also
  runs when the old generation exceeds twice its size after the last major
  collection, with a minimum of 1 MiB (both settings since Phase 4).  So long-lived data is copied once at
  promotion, then about once per doubling of the old generation.
- **The allocation check is unchanged:** a budget compared with each
  allocation's size, reset to the nursery size after a collection.
- **Setting it:**
  - a C API in `minim.h`, `minim_gc_set_policy` and `minim_gc_get_policy`,
    for embedders and unit tests;
  - the environment variables `MINIM_GC_NURSERY` and `MINIM_GC_RADIX`, read by
    `minim_init`, so any binary or test can be stressed without rebuilding
    (.NET's `GCStress` works the same way).  A malformed value aborts with a
    message;
  - Scheme primitives `(collect)`, `(collect-nursery-bytes [n])`, and
    `(collect-generation-radix [r])`, so a golden test can stress only the
    code it exercises.  Chez's radix applies to each generation in turn;
    with two generations it means the same thing, except that 0 is ours.

**Generation of an object.**  `copy` and the barrier need it in O(1).

- Every chunk is allocated aligned to its 16 KiB size: `aligned_alloc`, or
  `_aligned_malloc` on MSVC.
- The chunk header comes first, with a generation byte at offset 0.
- An object's chunk is its address with the low 14 bits cleared.  This holds
  even for a large object in its own oversized chunk, since the object starts
  in the chunk's first 16 KiB.
- `MINIM_CHUNK_SIZE` becomes the whole chunk, header included.

**Write barrier.**  All heap writes go through four inline setters in
`include/minim.h`: `minim_set_car`, `minim_set_cdr`, `minim_vector_set`, and
`minim_set_symbol_value`.

- If the target's chunk generation is nonzero, the setter calls an out-of-line
  `minim_gc_remember(obj)`.
- That function sets the object's bit in its chunk's *remembered bitmap*, one
  bit per 16-byte unit, the size of the smallest object.  If the bit was
  clear, it also appends the object to the remembered set, an array.
- The bit keeps each object in the set once.
- The setters don't check the stored value.  A minor collection scans every
  remembered object, then the set and its bits are cleared.
- The bitmap is allocated only for old chunks.

**Stress mode is a policy, not a build.**  A 1-byte nursery collects before
every allocation, which is today's `MINIM_GC_STRESS`.  The compile-time option
and the `stress` configure preset go away.

| Mode | Nursery | Radix | Catches |
|---|---|---|---|
| stress | 1 | 16 | unrooted young objects and missed barriers at every allocation; unrooted old objects at every 16th |
| full stress | 1 | 1 | everything, at today's cost |

- **Why radix 16 is enough for CI:** a minor collection at every allocation
  catches the common bug, a fresh object held unrooted across the next
  allocation, plus any missed barrier.  A site holding an *old* object
  unrooted is caught when it meets a major collection, and a site run many
  times, with varying allocation counts, does.  Interval 16 once missed
  injected bugs, but that interval applied to *all* collections, so
  young-object bugs went unchecked too.
- **CTest:** every test is registered twice.  The second copy is named
  `<name>_stress`, labelled `stress`, and runs with `MINIM_GC_NURSERY=1` and
  `MINIM_GC_RADIX=16`.
  - Test presets on the one `dev` build: `dev` excludes the label, `stress`
    runs only it.  CI keeps its two jobs from one configure preset.
  - Full stress is by hand: `MINIM_GC_NURSERY=1 MINIM_GC_RADIX=1 ctest --preset
    dev`.  Tests without their own settings inherit the environment.
- **Tests that shrink their work under stress** (`#ifdef MINIM_GC_STRESS`
  today) check the policy at run time instead.
- Reopen the radix with Phase 5's injected-bug results.

**Promotion on first survival** (en-masse).

- It is the simplest policy, and it empties the young generation, so the
  remembered set is cleared after every collection.
- The cost: an object live at a minor collection is promoted even if it dies
  soon after, and takes up old space until the next major collection.
- Aging, where an object must survive *k* minor collections, would need
  per-object ages or a survivor space.
- Reopen if Phase 5 shows major collections driven by promoted garbage.

**Two generations**, not several.  It is the smallest change that copies
long-lived data once.  Reopen if benchmarks show frequent major collections,
or long major pauses with large heaps (see Chez's approach above).

**Accounting.**

- `minim_heap.live` is the old generation's size after the last collection.
- New fields count minor and major collections, for tests and measurements.

## Phases

### Phase 1 - Aligned chunks

**Done.**  Details settled during the phase:

- Chunks are 16 KiB, not 64 KiB.  With 64 KiB, `aligned_alloc` takes each
  chunk directly from the OS (glibc: mmap per chunk; ASan: its secondary
  allocator), and every stress collection allocates and frees chunks.  `gc`
  under stress took 27.8 s instead of 5.4 s with ASan, and 7.1 s instead of
  2.9 s without.  At 32 KiB, ASan was back to 5.3 s but glibc still spent
  1.2 s in the kernel; at 16 KiB, 5.3 s and 1.7 s, with no system time.
  Reusing freed chunks also fixed it (4.0 s and 1.7 s) but was rejected: a
  reused chunk hides a stale pointer held across two collections from ASan.
- A space records its generation, which `minim_space_free` and the collector
  preserve when they empty it, and `space_grow` stamps on new chunks.

- `src/heap.c`, `src/heap.h`:
  - chunks allocated with 16 KiB alignment;
  - a generation byte at the start of `minim_chunk`, 0 for the new space and
    1 for the per-kind spaces;
  - `minim_chunk_of(p)`;
  - `MINIM_CHUNK_SIZE` covers the header;
  - a portability shim for MSVC.
- `docs/design/gc.md`: the heap section.

Separate because the allocation change carries the portability risk (glibc,
macOS, and MSVC), and collection doesn't change.

Tests (`tests/test_heap.c`): every chunk is aligned, and `minim_chunk_of`
finds the chunk and generation for a pair, a symbol, a string, and a vector
larger than a chunk, before and after a collection.  `test_large_vector` is
adjusted for the new chunk capacity.

```sh
cmake --build --preset dev && ctest --preset dev -R 'heap|gc'
```

### Phase 2 - Minor and major collections

**Done.**  Details settled during the phase:

- The API is public and symmetric (`include/minim.h`):
  `minim_gc_collect_minor`, `minim_gc_collect_major`, and
  `minim_gc_collect`, which runs the policy, as Chez's `(collect)` does.
  The allocator calls `minim_gc_collect` when the budget runs out.  Existing
  callers, all expecting every object to move, call `minim_gc_collect_major`.
- The policy is `minim_gc_policy {nursery_bytes, radix}`, with
  `minim_gc_get_policy` and `minim_gc_set_policy`; setting it restarts the
  nursery.  Counters are `minim_heap.minor_collections` and
  `major_collections`.
- A minor collection scans the old spaces from their start, which covers both
  the old objects as roots and the promoted ones.
- Phase 1 bug, fixed here: a chunk sized for a large object kept its tail for
  later small objects, which could start past the first 16 KiB, where
  `minim_chunk_of` reads a garbage generation.  A minor collection then left
  a young object in place and freed it.  A large object's chunk now holds
  nothing else; `test_heap` checks an allocation after a large vector.
- Removing the old-generation scan from a minor collection is caught by the
  tests (ASan).
- Release, the Context programs: long-lived 14.6 s and 27 MB (Phase 1: 3.7 s
  and 96 MB), short-lived 1.65 s (1.47 s).  The time is the full old scan on
  each minor collection, which Phase 3 removes; the memory is the absence of
  a second copy of the old generation.

- `src/gc.c`, `src/gc.h`, `src/heap.c`, `src/heap.h`:
  - `minim_gc_collect_minor`;
  - `copy` leaves old objects alone in a minor collection;
  - the collection policy's C API (nursery bytes and radix) and the growth
    trigger, with collection counters;
  - `minim_gc_collect` stays major.
- Until Phase 3, a minor collection scans the whole old generation as roots:
  correct without a barrier, at a cost proportional to the old generation.
- The compile-time stress mode is unchanged until Phase 5: every stress
  collection is major.
- `docs/design/gc.md`: generations, collections, and the policy.

Separate because it is the new collector behaviour, made correct the simple
way, so that Phase 3's optimization has a regression net.

Tests (`tests/test_gc.c`, also in `stress`):
- a survivor of a minor collection is old, and keeps its address across
  later minor collections but not a major one;
- an old pair, vector, and symbol, each later set to a new object (with each
  of the four setters), keep it alive and correct across many minor
  collections;
- an old object pointing to a young object that points back to old ones;
- a major collection reclaims old garbage;
- with a large structure live, a long run of short-lived allocations does
  mostly minor collections (through the counters);
- nursery and radix: a small nursery collects on schedule, and radix *r* makes every
  *r*-th collection major.

Tests set the policy themselves where they need a schedule, and restore it.

```sh
cmake --build --preset dev && ctest --preset dev -R gc
cmake --build --preset stress && ctest --preset stress -R gc
```

### Phase 3 - Write barrier

**Done.**  Details settled during the phase:

- The barrier is `minim_gc_write(obj, x)` in `include/minim.h`, called at the
  end of each setter.  It skips fixnums and immediates `x`, a tag test that
  keeps counters and flags out of the remembered set.  `MINIM_ALIGN` and
  `MINIM_CHUNK_SIZE` moved to `minim.h`; `heap.h` asserts the generation byte
  is first.
- The remembered bitmap is inline in every chunk header (128 bytes per 16
  KiB), not allocated for old chunks only: simpler, and new chunks start
  zeroed.
- The scan cursors are taken before the roots are copied, since copying a
  root already promotes into the old spaces.
- Removing the barrier from each setter in turn is caught by the tests.  It
  first missed `minim_set_car`, because the test also wrote the same pair with
  `minim_set_cdr`; each setter now writes its own object.
- Release, the Context programs (3 runs each): long-lived 2.29-2.33 s, 27 MB
  (Phase 1: 3.72-3.79 s, 96 MB); short-lived 1.62-1.66 s (Phase 1:
  1.59-1.65 s).

- `include/minim.h`:
  - the four setters call `minim_gc_remember` when the target is old;
  - the chunk constants and the generation byte's offset, which the inline
    check needs, move to or are mirrored in the public header.
- `src/heap.c`: the remembered bitmap of each old chunk.
- `src/gc.c`:
  - the remembered set;
  - minor collections scan it instead of the whole old generation;
  - every collection empties it.
- `docs/design/gc.md`: the barrier.

Separate because it replaces Phase 2's full scan with an optimization whose
failure mode, a missed write, Phase 2's tests are written to catch.

Tests (`tests/test_gc.c`, also in `stress`):
- Phase 2's tests, unchanged;
- writing the same old object repeatedly remembers it once;
- the set is empty after a minor collection, and after a major one;
- a symbol interned into an old symbol table survives minor collections.

```sh
cmake --build --preset dev && ctest --preset dev -R gc
cmake --build --preset stress && ctest --preset stress -R gc
```

### Phase 4 - Run-time collection settings

**Done.**  Details settled during the phase:

- `minim_gc_init` reads the variables, so the settings cover startup.  A
  malformed value, or one past the largest fixnum, prints `minim: MINIM_GC_NURSERY
  must be a positive integer, not "1k"` and exits with status 1.
- `(collect)` takes no arguments; the setters return void and reject a
  non-fixnum or out-of-range value.
- The defaults are tested in a script test without the variables
  (`gc-settings`), not in `tests/repl/gc.scm`, which Phase 5's stress copy
  will run with the variables set.  The REPL test sets everything it reads.
- `minim_add_script_test` takes `ENV var=value...`.
- Chez's "trip bytes" is called the nursery size throughout, the standard
  term: `nursery_bytes`, `MINIM_GC_NURSERY`, `collect-nursery-bytes`.
- The growth trigger's two constants became policy settings too:
  `major_growth` (default 2) and `major_min_bytes` (default 1 MiB), with
  `MINIM_GC_MAJOR_GROWTH`, `MINIM_GC_MAJOR_MIN`, `collect-major-growth`, and
  `collect-major-min-bytes`.  The limit is computed from the policy at each
  check, so a change applies at once.  The four primitives share one
  `policy_setting` helper.
- Environment variables are read only through `minim_getenv(name)` in a new
  `src/system.h`, the home for OS interfaces.  It returns the value, or NULL
  if unset or empty; callers convert.  `docs/design/environment.md` lists
  every variable.

- `src/gc.c`: `minim_init` reads `MINIM_GC_NURSERY` and `MINIM_GC_RADIX`.
- `src/prims.c`: `collect`, `collect-nursery-bytes`, and
  `collect-generation-radix`.
- `docs/design/gc.md`, `docs/design/primitives.md`, `README.md`: the
  settings.

Separate because it adds the run-time controls without yet changing how CI
stresses, so both can be compared on the same tree.

Tests:
- `tests/repl/gc.scm` / `.out` / `.err`: the primitives' defaults, setting
  and reading them back, rejected values, and a loop that builds a list with
  the nursery at 1 byte;
- script tests that run `minim` with each variable set, including malformed
  values (exit status and message).

```sh
cmake --build --preset dev && ctest --preset dev -R 'gc|prims'
```

### Phase 5 - Stress as a policy

**Done.**  Details settled during the phase:

- `minim_add_stressed(name command...)` in `tests/CMakeLists.txt` registers
  both copies; each test helper uses it.  Script tests with `ENV`, or the new
  `NO_STRESS` flag (`gc-settings`, which checks the defaults), get no stress
  copy.  The `stress` test preset has 27 tests.
- `tests/test.h`: `gc_stressed()` (nursery under 1 KiB) shrinks slow tests,
  and `default_gc_policy()` sets the defaults for a test that relies on when
  collections happen, returning the policy to restore.  `test_heap`'s
  young-object checks and `test_gc`'s scheduling tests use it, so they run in
  both copies instead of being compiled out.
- CI: one `dev` configure and build per job; the matrix is over the test
  presets.

Injected bugs (`ctest --preset dev`, `--preset stress`, and full stress):

| Bug | normal | stress | full |
|---|---|---|---|
| `minim_cons` doesn't protect `cdr` (young) | caught | caught | caught |
| `grow` doesn't protect the old table | caught | caught | caught |
| `minim_set_cdr` skips the barrier | caught | caught | caught |

These sit on hot paths, so they don't separate the settings.  A synthetic
program that holds an *old* pair unrooted across one allocation, at a site
run *n* times:

| *n* | radix 16 | radix 1 | default policy |
|---|---|---|---|
| 1, 4 | missed | caught | missed |
| 16, 64 | caught | caught | missed (64) |

So radix 16 catches an old-object bug only at a site that runs at least
about 16 times under the tests; `stress-full` (radix 1) catches it anywhere.

Measurements (release unless noted):

| | Phase 1 / before | This branch | With `macros` merged |
|---|---|---|---|
| long-lived program | 3.72 s, 96 MB | 2.56 s, 27 MB | 2.36 s, 27 MB |
| short-lived program | 1.59 s | 1.76 s | 1.74 s |
| startup (20 runs) | 0.048 s | 0.036 s | 0.21 s |
| stress startup (`dev` build) | 0.14 s (`macros`: 28 s) | 0.06 s | 3.1 s |
| stress suite, serial | about 29 s (old preset) | 11.3 s | 104 s |

With `macros`, stress startup is 9× faster than under the old preset, as
estimated, but each of the 30 stress tests still spends about 3 s starting
up: expanding the prelude allocates about 270,000 times, and a collection at
every allocation has a fixed cost (roots, a fresh nursery chunk) even when
little survives.

- `CMakeLists.txt`, `CMakePresets.json`, `.github/workflows/ci.yml`:
  - remove `MINIM_GC_STRESS`, `MINIM_GC_STRESS_INTERVAL`, and the `stress`
    configure and build presets;
  - add `dev` and `stress` test presets on the `dev` build, filtered by the
    `stress` label;
  - CI's matrix runs those two test presets.
- `tests/CMakeLists.txt`: each helper also registers `<name>_stress`.
- `src/heap.c`: the compile-time stress branch goes.
- `tests/test_*.c`: `#ifdef MINIM_GC_STRESS` becomes a run-time check of the
  policy.
Separate because it changes test coverage and CI, which is decided by
measurement.

Validation, recorded here and not committed: inject three bugs and record
which settings catch each, and how long `ctest --preset stress` takes:
- a young object held unrooted across an allocation;
- an old object held unrooted across an allocation;
- a setter that skips the barrier.

Measure and record here, against the tables in Context:
- the two allocation programs;
- release startup (20 runs);
- stress startup (`MINIM_GC_NURSERY=1 MINIM_GC_RADIX=16`);
- the stress suite's total time.

Measure each both on this branch and with the macros branch rebased on it.

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset stress
```

### Phase 6 - One configuration struct, one state struct

**Done.**  Details settled during the phase:

- Configuration: `minim_gc_policy` is renamed `minim_gc_config`, with
  `minim_gc_get_config` and `minim_gc_set_config`; the primitives' helper is
  `config_setting`, and the test helper `default_gc_config`.
- State: `struct minim_gc minim_gc` in `src/gc.h`, holding the configuration,
  the spaces, the root stacks (`minim_root_stack`), the value stack and the
  remembered set (`minim_obj_stack`), the byte counts, the collection
  counters, and the scheduling bookkeeping.  `minim_heap` and `minim_values`
  are gone; their fields are `minim_gc.*` and `minim_gc.values`.
- `minim_heap_init` and `minim_heap_shutdown` are folded into
  `minim_gc_init` and `minim_gc_shutdown`, which assign the whole struct.
- `minim_gc_remembered()` is gone; tests read `minim_gc.remembered.len`.

The collector's configuration and state are spread across file-level
variables: in `gc.c`, `policy`, `scheduled`, `major_base`, `minor`, the root
stacks `locals` and `globals`, and the remembered set; in `gc.h`,
`minim_values`; in `heap.h`, `minim_heap` with the spaces and counters; and
Phases 7 and 8 then add their state (the startup flag, the chunk free
list) to these structs.

- **Configuration:** one struct holding every setting, now the policy's four
  fields; Phases 7 and 8 add the startup flag and the quarantine size.  It is what the
  environment variables, the C API, and the primitives read and write.
  `minim_gc_policy` becomes it, or part of it.
- **State:** one struct holding the configuration and all run-time state:
  spaces, root stacks, value stack, remembered set,
  counters, and the bookkeeping for the next major collection.  One global
  instance; `minim_heap` and `minim_values` become fields of it.
- Initialization and shutdown reset that one struct, so nothing is left out
  between `minim_init` and `minim_shutdown`.

Separate because it is a pure restructuring: no behaviour changes, and the
existing tests are its regression net.  It comes before Phases 7 and 8 so
their state goes straight into the new structs.

Tests: none new; the full suite in both presets.

```sh
cmake --build --preset dev && ctest --preset dev && ctest --preset stress
```

### Phase 7 - Stress the program, not startup

**Done.**  Details settled during the phase:

- The startup flag isn't part of `minim_gc_config`: it only matters during
  `minim_init`.  `minim_gc_init` reads every variable, keeps the environment's
  settings in `minim_gc.program_config`, and starts with the defaults unless
  `MINIM_GC_STARTUP` is 1.  `minim_gc_finish_startup`, last in `minim_init`,
  applies them.  `MINIM_GC_STARTUP` must be 0 or 1.
- `test_startup` is registered as `startup` (dev: `MINIM_GC_NURSERY=1`
  alone; startup runs fewer than 10 collections, the program collects) and
  `startup_stress` (labelled `stress`: full stress from the start; startup
  runs only major collections, over 1000).  `startup_stress` is the one
  stress test of startup.  Dropping the `minim_gc_finish_startup` call fails
  `startup`.
- `ctest --preset stress`, serial, 28 tests: radix 16 in 10.6 s, radix 1 in
  32.1 s (`eval_stress` 13.5 s; `repl_tail_stress`, `repl_gc_stress`, and
  `gc_stress` about 5 s each).  Radix 1 is just over the 30 s line, so the
  copies keep radix 16.
- Not yet measured with `macros`: `startup_stress` runs the expander under
  radix 1, which Phase 5 suggests takes about 28 s.

Phase 5 left every stress copy paying for startup: with `macros`, each one
spends about 3 s expanding the prelude under stress, the same code every
time.  Startup is stressed once instead, and the other copies stress only the
program.

- `src/gc.c`: the `MINIM_GC_` variables are still parsed by `minim_gc_init`,
  so a malformed value fails before anything runs, but the policy they set
  takes effect at the end of `minim_init`; startup runs with the default
  policy.  A new variable, `MINIM_GC_STARTUP=1`, applies it from the start.
- `src/object.c`: `minim_init` applies the policy last.
- `tests/CMakeLists.txt`: one stress test of startup alone, `startup_stress`,
  runs `minim` on empty input with `MINIM_GC_STARTUP=1 MINIM_GC_NURSERY=1
  MINIM_GC_RADIX=1`.
- Measure `ctest --preset stress` with radix 16 and radix 1, and pick the
  radix for the stress copies (see Open items).

Separate because it changes what the stress tests cover, which Phase 5
measured.

Tests: `tests/test_startup.c`, registered twice with different environments
and an argument giving the expectation: with `MINIM_GC_NURSERY=1` alone,
`minim_init` runs few collections and the policy afterward is the variable's;
with `MINIM_GC_STARTUP=1` too, it runs one per allocation.  A script test
for a malformed `MINIM_GC_STARTUP`.

```sh
cmake --build --preset dev && ctest --preset dev -R 'startup|gc' && ctest --preset stress
```

### Phase 8 - Reuse nursery chunks

**Done.**  Details settled during the phase:

- Profile, before: gprof with `-pg -fno-inline` distorted the run (2 minutes
  for a loop that takes 11 s), so it was measured directly instead.  A loop
  program (`loop.scm`: 20,000 iterations building a 20-element list), `-O1`
  without sanitizers, a 1-byte nursery: minor collections only 5.2 s, radix
  16 11.2 s, so about 2 µs per minor collection.  A free-list prototype took
  minor-only to 2.6 s: chunk allocation was half of that cost.
- The spares are a `minim_chunk_queue` in `minim_gc.spares`, taken in
  `space_grow` and filled by `minim_space_free`.  At most `nursery_bytes /
  MINIM_CHUNK_SIZE + 1 + MINIM_CHUNK_QUARANTINE` are kept; large chunks and
  the surplus are freed.  `minim_heap_free_spares` frees them at shutdown.
  Spare chunks count in `minim_gc.size`.
- The quarantine is a constant, `MINIM_CHUNK_QUARANTINE` in `heap.h` (8 under
  ASan, else 0), not a configuration field: nothing needs to change it at run
  time.
- `space_grow` poisons every new chunk's data, not only reused ones, and
  `minim_space_alloc` unpoisons each object; the header stays unpoisoned, as
  the barrier and the queue read it.
- `test_stale` uses `PASS_REGULAR_EXPRESSION "AddressSanitizer:
  use-after-poison"` rather than `WILL_FAIL`, so a crash of any other kind
  fails it.  Its first version passed with the quarantine at 0: the spare
  queue held older chunks, so the next allocation never reached the stale
  pair's.  It now takes every spare first.  With the quarantine at 0, or
  without poisoning, it fails.
- Measurements (ASan stress suite serial; release programs):

  | | Phase 7 | Phase 8 |
  |---|---|---|
  | stress suite, radix 16 | 10.6 s | 6.7 s |
  | stress suite, radix 1 | 32.1 s | 24.1 s |
  | `loop.scm`, minor only, no sanitizers | 5.2 s | 2.5 s |
  | long-lived program | 2.56 s | 1.36 s |
  | short-lived program | 1.76 s | 0.96 s |

- Radix 1 is now under the 30 s line, so the stress copies use it, per the
  open item.

Every collection frees the nursery's chunks and the next allocations get
fresh ones from `aligned_alloc`, a fixed cost per collection that dominates
under stress.  Generational collectors normally reuse the nursery.

- `src/heap.c`: freed standard-size chunks go on a FIFO free list, which
  `space_grow` takes from before calling `aligned_alloc`.  Large chunks are
  still freed.  The list is capped (the nursery's chunks plus the
  quarantine), and the surplus is freed.
- Under AddressSanitizer (`__SANITIZE_ADDRESS__`, or
  `__has_feature(address_sanitizer)`):
  - a freed chunk's data is poisoned (`ASAN_POISON_MEMORY_REGION`), and
    `minim_space_alloc` unpoisons each object it hands out, so a stale
    pointer into a free or unused part still faults;
  - a chunk is reused only after the free list holds more than a quarantine
    of newer chunks (8), so a stale pointer isn't immediately aliased by the
    next object, which under stress lands at the same address.
  Without it, no quarantine: a freed chunk is reused at once.
- First, profile one collection under stress to confirm chunk allocation is
  the fixed cost worth removing; record the split here.

Separate because it changes memory reuse, the risk Phase 1 avoided, and its
ASan behaviour needs its own test.

Tests:
- `tests/test_heap.c`: a chunk freed by a collection is reused after at most
  the quarantine's worth of new chunks; a large chunk isn't.
- `tests/test_stale.c`, built only with `MINIM_SANITIZE` and registered with
  `WILL_FAIL`: holds a young object unrooted across a minor collection and
  reads it, which ASan must report.  It guards the poisoning and quarantine.

Measure the stress suite, stress startup, and the two allocation programs
again.

```sh
cmake --build --preset dev && ctest --preset dev -R 'heap|gc|stale' && ctest --preset stress
```

### After the last phase

Sync the docs with the finished collector:

- `docs/design/gc.md`: stress mode as a policy, with no build option, and a
  check of every section against the code;
- `docs/design/environment.md`, `docs/design/primitives.md`: check against
  the code;
- `README.md`: stress testing with the test presets and variables;
- `docs/roadmap.md`: milestone 8's static-space item replaced by
  generations, and the chunk-reuse item noting the ASan tradeoff.

Then run everything:

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress
MINIM_GC_NURSERY=1 MINIM_GC_RADIX=1 ctest --preset dev
cmake --preset release && cmake --build --preset release
```

## Open items

### How is stressing startup selected?

- **Provisional:** the policy variables apply after startup, and
  `MINIM_GC_STARTUP=1` makes them apply from the start, as above.
- **Alternative:** they apply from the start, as now, and a variable delays
  them until after startup.  This keeps today's meaning of the variables, but
  makes the common case, stressing a program, the one that needs the extra
  variable.
- Reopen if stressing startup turns out to be the common need.

### Which radix do the stress copies use after Phase 7?

- **Radix 1** catches an unrooted old object at any site, which radix 16
  misses at sites run fewer than about 16 times (Phase 5).
- **Radix 16** is cheaper.
- **Provisional:** radix 1 if `ctest --preset stress`, serial, stays under
  about 30 s, the pre-macros suite's time; otherwise 16.

### How big is the quarantine, and does it cover old-space chunks?

- **Size:** 8 chunks keeps a stale pointer poisoned for 8 collections under
  stress, at the cost of 128 KiB held back.  Reopen if `test_stale` or an
  injected bug shows misses.
- **Scope:** every standard-size chunk goes through the same free list,
  including old-space chunks freed by a major collection.  One mechanism is
  simpler than a nursery-only one.

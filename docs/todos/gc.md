# Collector: more generations

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
  they never describe an intermediate state.  Each phase records its decisions
  in this document instead.
- **Design docs stay succinct**: only the details a reader needs to understand
  the current implementation, said in as few words as possible.

## Context

The collector (`src/gc.c`, `docs/design/gc.md`) has two generations: the
nursery, and one old generation of per-kind spaces.  A minor collection copies
the nursery's survivors into the old generation; a major collection copies
everything.  Majors run every `radix`-th collection, or when the old
generation has grown by `major_growth` past `major_min_bytes`.

Under stress (a collection at every allocation, `testing.md`), the cost is
allocations × cost per collection.  Since the previous commit, a minor
collection costs only its survivors, but every major one copies the whole old
generation, which now includes the prelude, the expander, and every macro.
Measured on the ASan `dev` build:

| Run | stress, radix 16 | minor collections only | no stress |
|---|---|---|---|
| `tests/repl/derived.scm` | 13.5 s | 0.67 s | 0.2 s |
| `test_eval` | 9.6 s | 0.52 s | 1.7 s |

`ctest --preset stress -j 8` takes 27.5 s, nearly all of it major collections
copying long-lived data.  Keeping that cost down has meant constant tuning:
the stress radix, `gc_stressed()` shrinking slow tests, `NO_STRESS`,
`MINIM_GC_STARTUP`, and which tests get stress copies.

Chez Scheme's answer is several generations collected on a radix schedule:
generation 1 every `radix` collections, generation 2 every `radix`², and so on.
Long-lived data drifts to the oldest generation, which is copied
geometrically less often.  This plan adopts it, so that a collection's cost
depends mostly on the young generations, and then removes the tuning that no
longer pays.

**Out of scope:** a static generation, aging within a generation,
mark-in-place, and the prelude bootstrap (next on the roadmap).

## Design

**Generations.**  Generation 0 is the nursery (the new space, all kinds
interleaved).  Generations 1 to `max_generation` each have a pair, a symbol,
and a typed space.  A chunk's header byte is its generation, as now.  A
compile-time cap, `MINIM_MAX_GENERATIONS`, sizes the space array.

**Collecting generation g** (0 ≤ g ≤ `max_generation`):

- The from-space is generations 0 to g; older objects stay put (`copy`
  checks the chunk's generation against g).
- Every survivor goes to generation t = min(g + 1, `max_generation`), Chez's
  default target.  When g is the oldest, t = g and its spaces start fresh.
- The scan starts where t's spaces ended before the collection, as a minor
  collection does now.
- Roots: globals, locals, the value stack, and the objects in generations
  above g that may point into 0..g: at first all of them (Phase 2), then
  only the remembered ones (Phase 3).
- Per-generation byte counts are kept as running counts: generations 1..g
  reset, t gains what is copied.  `minim_gc.live` stays the old generations'
  total.

`minim_gc_collect_minor` and `minim_gc_collect_major` remain, as collecting
generation 0 and the oldest; a new `minim_gc_collect_generation(g)` collects
any.

**Schedule.**  The n-th scheduled collection collects the largest g ≤
`max_generation` such that `radix`^g divides n.  Radix 1 makes every
collection full, as now; radix must be at least 1.

**Remembered set across generations.**  The barrier is unchanged: a write
into an object in generation ≥ 1 remembers it.  But promotion no longer
empties the younger generations, so an entry outlives a collection when its
object still points to a younger generation.

- A collection of 0..g scans the remembered objects in generations above g.
- Afterward, each remaining entry is kept only if one of its fields points to
  a generation younger than its own; the others are dropped and their bits
  cleared.
- Entries in collected generations are dropped: their objects moved, and
  everything they could point to that is younger moved with them into t.

**Settings.**  `max_generation` joins `minim_gc_config`, with
`MINIM_GC_MAX_GENERATION` and `(collect-maximum-generation [g])` after Chez.
The schedule is the only trigger: the growth trigger goes, with
`major_growth`, `major_min_bytes`, `MINIM_GC_MAJOR_GROWTH`,
`MINIM_GC_MAJOR_MIN_BYTES`, `collect-major-growth`, and
`collect-major-min-bytes`.  The defaults become Chez's: `max_generation` 4
and `radix` 4, so the oldest generation is collected every 256 collections
(256 MiB of allocation with the 1 MiB nursery).

**Stress coverage.**  An unrooted reference to an object in generation g is
caught only by a collection that moves it, every `radix`^g allocations under
stress, so prelude objects in the oldest generation are moved rarely.  The
stress copies use the default schedule anyway; a hand-run full stress
(`MINIM_GC_RADIX=1`) covers the rest, and Phase 4's injected bugs record
what the default catches.

## Phases

### Phase 1 - Spaces indexed by generation

**Done.**  Details settled during the phase:

- `minim_gc.nursery` is generation 0; `minim_gc.old[g][kind]` holds
  generation g for g ≥ 1 (row 0 unused, so the index is the chunk header's
  generation), with `minim_gc.old_bytes[g]`.  `MINIM_MAX_GENERATION` (8) in
  `heap.h` sizes them.  The `MINIM_SPACE_*` enum became `MINIM_KIND_*`.
- `collect(g)` sets `minim_gc.collecting` and `minim_gc.target`, replacing
  `minim_gc.minor`; `copy` leaves objects older than `collecting` in place.
  The oldest generation is `oldest()`, fixed at 1 until Phase 2.
- Remembered objects are scanned unless the oldest generation is collected,
  and the set is emptied after every collection: right for two generations;
  Phase 2 changes it.
- `minor_collections` counts collections of generation 0, and
  `major_collections` those of the oldest.

- `src/heap.h`, `src/gc.h`, `src/gc.c`: the old spaces become
  `old[generation][kind]`; `collect(g)` replaces `collect(bool minor)`;
  per-generation byte counts; `minim_gc_collect_generation(g)` in
  `include/minim.h`.  `max_generation` is fixed at 1, so behaviour is
  unchanged.

Separate because it reshapes the data structures without changing what
collections do; the existing tests are its regression net.

Tests: the existing suite, unchanged; `test_gc` adds a check that
`minim_gc_collect_generation(0)` and `(1)` behave as minor and major.

```sh
cmake --build --preset dev && ctest --preset dev -R 'gc|heap'
```

### Phase 2 - More generations, older ones scanned in full

**Done.**  Details settled during the phase:

- Collecting the oldest generation collects every generation present
  (`collecting` = `MINIM_MAX_GENERATION`) into fresh spaces for the oldest,
  so lowering `max_generation` at run time merges the generations above it.
- The target generation is scanned from its start, which covers its old
  objects as roots along with what is copied in; generations above it are
  scanned once each.  The remembered set is emptied, bits cleared, after
  every collection.  `end_of` and `scan_object` became unused and were
  removed; Phase 3 brings them back.
- `minim_gc.collections[g]`, collections by the oldest generation each
  collected, replaces `minor_collections` and `major_collections`.
- `minim_gc_set_config` restarts the schedule (`scheduled` = 0) as well as
  the nursery, so a test can predict it.
- `config_setting` in `prims.c` takes a maximum; `collect-maximum-generation`
  rejects values outside 1 to 8 with "bad generation".
- Tests: `test_aging`, `test_schedule` (radix 2 and radix 1), and
  `test_older_to_younger` (holders in generations 3 and 2, one per setter,
  targets aged through every level).  Not scanning the generations above the
  target, or skipping the target's old objects, each fails it.  `gc.scm` and
  the script tests cover the setting and `MINIM_GC_MAX_GENERATION`.
- `ctest --preset stress -j 8`: 1 min 40 s, from 27.5 s, as expected: every
  minor collection scans the whole old generation until Phase 3.

- `src/gc.c`: promotion to g + 1, the radix schedule over generations, and
  `max_generation` in `minim_gc_config` (default still 1).  A collection of
  0..g scans every object in generations above g as roots: correct without
  the remembered set's help, at a cost proportional to those generations.
- `src/gc.c`, `src/prims.c`: `MINIM_GC_MAX_GENERATION` and
  `collect-maximum-generation`.

Separate because it is the new behaviour, made correct the simple way, so
that Phase 3's optimization has a regression net.

Tests (`tests/test_gc.c`, with `max_generation` 3; also in `stress`):
- an object ages one generation per collection that includes it, and stops
  at the oldest;
- the schedule: with radix 2, collections 1 to 8 collect generations
  0, 1, 0, 2, 0, 1, 0, 3;
- pointers from each older generation into each younger one, made with each
  setter, keep their targets alive across collections of every level;
- a collection of g leaves objects above g where they are;
- `tests/repl/gc.scm` and the `gc-settings` script tests for the new setting.

```sh
cmake --build --preset dev && ctest --preset dev -R 'gc' && ctest --preset stress -R 'gc'
```

### Phase 3 - Remembered set across generations

**Done.**  Details settled during the phase:

- `pointer_fields` lists an object's pointer fields once, for `scan_object`
  (restored) and the new `points_younger`.  The target's scan starts at its
  end again (`end_of`, restored), taken after the from-spaces are reset:
  taking it before, in a full collection, left the cursor in a discarded
  chunk, which UBSan caught.
- A collection scans the remembered objects above the collected
  generations, then keeps those still pointing younger and clears the bits
  of the rest, including moved ones (their chunks are still allocated then).
- `test_remembered_generations`: an entry survives collections 0 and 1 while
  its target is younger, is dropped when the target reaches its generation,
  and a later write remembers it again.
- Injected bugs, each caught: dropping every entry, and not scanning the
  surviving entries (`test_older_to_younger`); keeping every entry
  (`test_remembered_generations`).
- `ctest --preset stress -j 8`: 19.5 s (Phase 2: 1 min 40 s; before this
  plan: 27.5 s).  Release benchmarks unchanged: 1.63 s and 1.09 s.

- `src/gc.c`: collections scan only remembered objects above g instead of
  whole generations; after a collection, entries are kept or dropped as in
  the design.

Separate because it replaces Phase 2's full scan with an optimization whose
failure mode, a missed root, Phase 2's tests are written to catch.

Tests (`tests/test_gc.c`):
- Phase 2's tests, unchanged;
- an entry survives a collection while its object still points younger, and
  is dropped (bit cleared) once it doesn't;
- injected-bug check, recorded here: removing the pruning or the scan of
  surviving entries fails a test.

```sh
cmake --build --preset dev && ctest --preset dev -R 'gc' && ctest --preset stress -R 'gc'
```

### Phase 4 - Defaults and stress settings

- `src/gc.c`, `include/minim.h`, `src/prims.c`: the defaults become
  `max_generation` 4 and `radix` 4; the growth trigger and its settings,
  variables, and primitives are removed, with their tests (`test_gc`'s
  `test_major_growth`, `tests/repl/gc.scm`, the `gc-settings` script tests).
- `tests/CMakeLists.txt`: the stress copies and `startup_stress` use the
  default schedule with a 1-byte nursery.
- Measure, against Context: the two allocation programs, release startup,
  the table above, and `ctest --preset stress -j 8`; and the injected bugs
  from the previous collector plan (an unrooted young object, an unrooted
  old object, a missed barrier) under the new stress settings.

Separate because it is the policy and its measurement, and because removing
API is easier to review on its own.

```sh
cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
```

### Phase 5 - Remove tuning that no longer pays

With stress cheap, remove what existed only to keep it affordable, each
measured before it goes: `gc_stressed()` and the loop sizes it shrinks,
`NO_STRESS` where a test only had it for speed, and `MINIM_GC_STARTUP` if
stressing startup in every copy is now affordable (otherwise it goes with the
prelude bootstrap).  Record each kept knob's reason here.

Separate because it changes test coverage, not the collector.

```sh
cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
```

### After the last phase

Sync the docs: `gc.md` (generations, the schedule, the remembered set,
settings, stress testing), `environment.md` (`MINIM_GC_MAX_GENERATION`, and
any removed variables), `primitives.md` (`collect-maximum-generation`, and
any removed primitives), `testing.md` (stress settings and removed knobs),
and the roadmap (milestone 8: more generations done).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

None.  Settled before Phase 1: the radix schedule is the only trigger (the
growth trigger and its settings are removed); the defaults are Chez's,
`max_generation` 4 and `radix` 4; stress copies use the default schedule,
with full stress by hand.

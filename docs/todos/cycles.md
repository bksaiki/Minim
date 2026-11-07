# Cycles: datum labels, `write-shared`, and `equal?` on cyclic data

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

Minim has no way to read, write, or compare cyclic data:

| Input | Today | R7RS |
|---|---|---|
| `(equal? c c*)`, two separately built circular lists | loops forever | "must always terminate" (6.1) |
| `(write c)` | loops forever | labels at least the objects on a cycle; no labels if there are no cycles (6.13.3) |
| `(display c)` | loops forever | must not loop on self-referencing pairs, vectors, or records |
| `(write-shared x)` | unbound | labels every pair and vector that appears more than once |
| `(write-simple c)` | loops forever, as allowed | never labels |
| `(read (open-input-string "#0=(1 . #0#)"))` | `bad # syntax` | a circular list (2.4) |

`write` and `write-simple` are both `%write` (`scheme/ports.scm`);
`(scheme write)` lacks `write-shared`.  The conformance reports fail on this
in 6.13 (`write-shared`) and Read syntax (`#0=`, twice).

The pieces involved:

- `is_equal` (`src/equal.c`) compares with an explicit stack of pending
  pairs; nothing in it allocates on the heap.
- The printer (`src/print.c`) walks a datum recursively into a sink, never
  allocating on the heap, so `minim_write` is safe on error paths.
- The reader (`src/read.c`) reads `#` syntax in `read_hash`, which pushes
  `#` and reads to a delimiter; it keeps no state between data.

## Design

**One object table.**  A new `src/table.h` / `src/table.c`: an
open-addressing hash table from objects to words, keyed by address, in
malloc'd memory.  Its keys are addresses, so it is valid only while nothing
allocates on the heap; `equal?`, the printer's walks, and the reader's patch
walk all qualify.  It serves all three phases.

**`equal?`** follows Adams and Dybvig ("Efficient Nondestructive Equality
Checking for Trees and Graphs", ICFP 2008), as Chez does, simplified to one
switch: `is_equal` compares as today until it has compared `EQUAL_BUDGET`
pairs and vectors; after that, before comparing two of them it looks them
up in a union-find over the table, skipping them if they are already in one
class and otherwise merging the classes and comparing.  Assuming equal what
is being compared is sound: if they differ, the comparison fails anyway.
Merging bounds the work by the number of objects, so it terminates.  Acyclic
data under the budget pays nothing.  `equal-hash` already terminates.

**The printer** walks a pair or vector datum once before printing it, with
an explicit stack, recording in the table:

- for `write` and `display`, the objects reached again while still on the
  walk's stack: the targets of back edges, which are on cycles;
- for `write-shared`, every pair and vector reached a second time.

Only pairs and vectors are traced; records, strings, and the rest print
without their contents' structure.  While printing, a recorded object prints
as `#n=` and its datum the first time, numbered from 0 in print order, and
as `#n#` after.  The loop over a list's cdrs stops at a recorded tail, which
prints dotted: `(1 . #0=(2 . #0#))`.  `abbreviation` doesn't abbreviate a
`(quote d)` whose second pair is recorded, which would hide it.
`write-simple` skips the walk.  The table is freed after printing, so the
printer still never allocates on the heap.

`minim_write` and `minim_port_write` label cycles, as `write`;
`minim_port_display` too.  New: `minim_port_write_shared` and
`minim_port_write_simple`, behind `%write-shared` and `%write-simple` in
`src/port.c`.  `(scheme write)` gains `write-shared`.

**The reader** reads `#n=` and `#n#` (R7RS 2.4) in `read_hash`: a `#`
followed by digits, then `=` or `#`.  Labels are scoped to the outermost
datum: `read_datum` starts an empty list of them, which the `source`
holds, rooted by its caller.  Each is `#(n placeholder value referenced?)`:

- `#n=` makes a fresh placeholder, a new pair, then reads the datum.
- `#n#` gives the value if the datum is read, else the placeholder, marking
  the label referenced.
- After the datum, if the label was referenced, a patch walk replaces the
  placeholder in the datum's pairs and vectors, with a visited table, since
  inner labels may have made it cyclic.

Errors, as read errors: `#n#` of an undefined label, `#n=#n#` (the
placeholder itself), a label defined twice in one datum (as Chez), and `#`
with digits then anything else.

**Cyclic literals in code** (R7RS 2.4 allows `'#0=(a . #0#)`).  The
evaluator returns a `quote`'s datum without walking it, but the expander's
`%strip`, which replaces aliases by their originals in a `quote`'s datum
and in a `define-library` form, recurses.  It becomes a primitive in
`src/syntax.c`, cycle-safe through the table:

- A first walk, allocating nothing, visits each pair and vector once and
  looks for an alias.  With none, the datum is returned as is, cyclic or
  not: the common case, a literal as the reader made it.
- Otherwise every pair and vector reached is copied, cycles included, each
  alias replaced by its original (through chains of aliases).  Copying
  allocates, which moves objects, so it goes in steps: count the objects;
  allocate a vector and fill it with them by walking again; allocate a
  copy of each into a second vector; only then key the table by the
  originals, which no longer move, and fill each copy's fields: an alias's
  original, another object's copy, or the field itself.

Unchanged parts aren't shared with the original once an alias is found,
which `quote` doesn't promise anyway.  Out of scope: a cyclic literal in a
`syntax-rules` template, which template instantiation walks.

## Phases

### Phase 1 - The object table, and `equal?` on cycles

- `src/table.h`, `src/table.c` (new; `CMakeLists.txt`): the table.  Find a
  key's value; add a key, its value zero; free.
- `src/equal.c`: `EQUAL_BUDGET`, and the union-find after it.

First, because the printer and reader phases reuse the table, and
`equal?` checks their results.

Tests: `tests/repl/equality.scm`: circular lists built with `set-cdr!`,
equal with different periods (`#0=(1 2 . #0#)` and `#0=(1 2 1 2 . #0#)`'s
shapes) and unequal; a vector containing itself; a cycle through cars;
a long acyclic list, past the budget, still equal and unequal.  Measure a
new `bench/equal.scm` (`equal?` of two long lists and of deep trees)
against `main`.

```sh
cmake --build --preset dev && ctest --preset dev -R 'equality|r7rs' && ctest --preset stress -R equality
```

**Done.**  The one switch at `EQUAL_BUDGET` was measured and dropped:
past it, every pair cost table lookups, and `bench/equal.scm` went from
0.16 s on `main` to 0.47 s.  `is_equal` now interleaves as Adams and
Dybvig's algorithm and Chez do: `EQUAL_FAST` (1000) pairs and vectors
compared untracked, then tracked in the union-find until `EQUAL_SLOW` (40)
in a row are new assumptions; one already assumed equal is skipped and
restarts that count.  New assumptions are finite, so the untracked phases
are too, and it terminates; acyclic data pays the table on about 4% of its
pairs.  Unlike Chez, the untracked phases have a fixed length, not a
random one, which termination doesn't need.  `bench/equal.scm`, 11 runs,
release: 0.157 s median on `main`, 0.166 here, minimum 0.154 and 0.157;
the rest of the suite unchanged.  `tests/repl/equality.scm` gained the
cases above and a pair whose car and cdr are both itself, which branches
at every step.

### Phase 2 - Labels in `write`, `display`, and `write-shared`

- `src/print.c`: the walk, labels in the sink, the cdr loop and
  `abbreviation` stopping at recorded objects.
- `include/minim.h`: `minim_port_write_shared`, `minim_port_write_simple`.
- `src/port.c`: `%write-shared`, `%write-simple`.
- `scheme/ports.scm`: `write-shared`; `write-simple` through
  `%write-simple`.  `scheme/standard.scm`: `write-shared` in `(scheme
  write)`.  `scheme/prelude.boot` regenerated.

Before the reader, so the reader's tests can print what they read.

Tests: `tests/test_print.c`: a circular list, `#0=(1 2 . #0#)`; a cycle
through the car, `#0=(#0#)`; a vector holding itself, `#0=#(1 #0#)`; a
cyclic tail, `(1 . #0=(2 . #0#))`; shared acyclic structure unlabeled by
`write`; two cycles numbered in print order.  `tests/repl/ports.scm` (or a
new `write.scm`): `write-shared` of `(let ((x (list 1 2))) (list x x))` as
`(#0=(1 2) #0#)`; `display` of a cycle; `write-simple` of shared data
unlabeled.  Measure a new `bench/write.scm` (writing a long list and a big
tree to a string port) against `main`.  The 6.13 report updated.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'print|ports|write|r7rs'
```

**Done.**  `find_labels` walks with an explicit stack of entries, each
entered and then left once its contents are walked, so an object is
WALKING until then: one reached while WALKING is a back edge's target.
Every cycle has a back edge, so printing always meets a label, and a
`#n#` never precedes its `#n=`.  Labels live in the sink, in a
`minim_table` (0, then the label + 1); `print` writes a pair's or
vector's label first, the cdr loop stops at a labeled tail, and
`abbreviation` refuses a labeled second pair, so `(quote . #0=(1 . #0#))`.
`print_labeled` frees the table after printing.  `run.scm`'s skip of the
cyclic `write` test went, its list now empty.  Tests:
`tests/test_print.c`'s `test_labels`, and a cyclic datum in `test_port`;
`tests/repl/write.scm` (new).  The 6.13 report: 63 passed, 0 failed.

The walk's own table is purpose-built (`seen` in `print.c`): one word per
object, its untagged address with the state in the low bits, which 16-byte
alignment leaves clear.  `bench/write.scm` (new; 10 writes each of a
100,000-element list and a 65,535-pair tree to string ports), release,
best of 5, against `main` at 0.147 s, with the data built alone in
0.074: `minim_table` for the walk, 0.238 s, printing 2.2 times `main`'s;
probing once per new object, 0.23; `seen`, 0.18, printing 1.4 times.
`bench/run.sh`, 11 runs: write 0.154 to 0.200 s median; the rest
unchanged.

### Phase 3 - Datum labels in the reader

- `src/read.c`: `#n=` and `#n#`; the label list in `source`; the patch walk.

Tests: `tests/test_read.c`: `#0=(1 . #0#)` whose cdr is itself;
`(#0=(1 2) #0#)` sharing one list; `#0=#(1 #0#)`; a label on an atom,
`(#0="a" #0#)`; nested labels, `#0=(#1=(a . #1#) . #0#)`; labels not
carrying over to the next datum; each error.  Round trips: each cyclic
datum Phase 2's tests print reads back `equal?` to it.  The Read syntax
report updated.

```sh
cmake --build --preset dev && ctest --preset dev -R 'read|r7rs' && ctest --preset stress -R read
```

**Done.**  Phase 2's purpose-built set moved from `print.c` to `table.h`
as `minim_marks`, beside `minim_table`, which `equal?`'s union-find and
the printer's label numbers still need: the set holds a mark from 1 to 7,
not a word.  The patch walk queues each pair and vector once through it.
`read_datum` roots the label list, an alist-like list of
`#(n placeholder value referenced?)` vectors, and points the `source` at
it; the value is MINIM_UNBOUND until read.  The errors: `undefined datum
label` and `duplicate datum label` with the number; `bad datum label` for
`#n=#n#` and `#n=` with no datum after it; `bad # syntax` for digits
followed by anything else; a label past the fixnums, `bad datum label`.
Tests: `test_read.c`'s `test_labels`, `test_label_round_trips` (each of
Phase 2's cyclic data, written and read back, `equal?`), and the errors.
The Read syntax report: 92 passed, 1 failed (`|+NaN.0abc|`, settled in
`reader-syntax.md`'s review).

### Phase 4 - Cyclic literals in code

- `src/syntax.c`: `%strip`, as above.
- `scheme/expander.scm`: the Scheme `%strip` out.  `scheme/prelude.boot`
  regenerated.

Last, because its tests read cyclic literals (Phase 3) and print them
(Phase 2).

Tests: `tests/repl/cycles.scm` (new): `'#0=(a . #0#)` and `'#0=#(1 #0#)`
evaluating to themselves, printed with labels; a macro quoting an alias
beside a cyclic argument, `(q #0=(1 . #0#))` as `(a #0=(1 . #0#))`; a
cyclic literal in a `define-library` body; a quoted datum with an alias
still stripped, and one without still `eq?` to what was read.  Measure
`bench/run.sh`'s expansion benchmark against `main`.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'cycles|syntax-rules|libraries|boot|r7rs'
```

**Done.**  `prim_strip` (`src/syntax.c`) as designed: `visit_data` walks
the pairs and vectors once, `x` first, in an order the data fixes, so the
second walk, after the originals' vector is allocated, fills it in the
first's order; copies are allocated next, then a `minim_table` maps each
original to its index and the copies' fields are filled.  An atom
argument returns its original, through chains of aliases.  Phase 3's
patch queue moved to `table.h` as `minim_walk`, which both walks share.
The Scheme `%strip` went from `expander.scm`; `tools/boot.scm` gets the
primitive.  `tests/repl/cycles.scm` (new) as planned, plus sharing kept in
a copy, `(a (#0=(1) #0#))` by `write-shared`.  `bench/run.sh`, 11 runs,
release: expand 0.236 s median on `main` and here; the rest unchanged.

### After the last phase

Sync the docs: `primitives.md` (`equal?` terminates on cycles; the table,
in Equality and hashing), `repl.md` (datum labels in the reader table; the
printer's labels; "Cyclic data loops forever" goes), `ports.md` (`write`,
`write-shared`, `write-simple`), `r7rs-small.md` (`(scheme write)` 4 of 4;
milestone 9's Cycles done; the conformance table).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

None; see Settled in review.

## Settled in review

- Cyclic literals in code are in scope: Phase 4.
- `EQUAL_BUDGET` starts at 1000, confirmed or tuned by Phase 1's
  benchmark.  Chez's precheck budget is a few hundred.  (Phase 1 replaced
  it with Adams and Dybvig's interleaving; see there.)
- `write` always walks a pair or vector datum first; reopen if Phase 2's
  benchmark shows a cost worth printing optimistically up to a budget.

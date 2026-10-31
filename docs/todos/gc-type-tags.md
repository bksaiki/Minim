# Object representation and GC: tagged pointers with a precise copying collector

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

## Context

Today the repo has only `src/main.c` (hello world) and a single-executable
`CMakeLists.txt`.  This plan adds the runtime core:

- a tagged value type `minim_obj`, using Chez Scheme's 64-bit tag assignments;
- four heap object kinds: **pairs, vectors, strings, symbols**;
- a **precise** copying garbage collector.

Chez is the reference for data representation and the collector, not for C
style.

Out of scope: non-fixnum numbers, closures, primitives, boxes, records,
bytevectors, hashtables, ports, environments, reader, printer, and evaluator.

**Decisions:**

| Decision | Choice | Reason |
|---|---|---|
| Language | C17 | every major compiler, MSVC included |
| Public names | `minim_` / `MINIM_` prefix, snake_case | avoids collisions |
| Library | static only | no export macros or ABI promises yet |
| Runtime state | file-scope globals; one runtime per process | short calls |
| Word size | 64-bit only | one tagging scheme |
| Collector | copying (Cheney) | bump allocation, no fragmentation, small |
| To-spaces | segregated by kind (pair, symbol, typed) | pairs stay 2 words |
| Roots from C | shadow stack with saved mark | no pop counting |
| String chars | `uint32_t` | O(1) `string-ref` / `string-set!` |
| Symbol names | string objects, so strings come first | nothing `malloc`'d outside the GC |
| Symbol table | strong; symbols are never freed | simple |

## C conventions

- C17 with `-Wall -Wextra`; `<stdbool.h>`, `<stdint.h>`, `static_assert`.
- One public header, `include/minim.h`.  Internal headers stay in `src/`;
  tests that inspect internals add `src/` to their include path.
- Every external symbol starts with `minim_`, internal ones included: a static
  library shares the linking program's namespace.
- `static inline` functions for predicates, accessors, and encoding.  Macros
  only where an address is taken (`MINIM_PROTECT`).
- Predicates return `bool` and are named `minim_is_*`.
- Constants are `enum`s.
- Fixed-width types where layout matters: `uint32_t` chars, `intptr_t` fixnum
  values, `size_t` lengths.
- Build configurations live in `CMakePresets.json`.

## Value representation

`typedef uintptr_t minim_obj;`.  The low 3 bits are the primary tag.

| Low 3 bits | Kind          | Notes                                              |
|------------|---------------|----------------------------------------------------|
| `000`      | fixnum        | 61-bit signed integer, `n << 3`                    |
| `001`      | pair          | pointer to `[car, cdr]`                            |
| `010`      | (flonum)      | reserved                                           |
| `011`      | symbol        | pointer to `[name, value]`                         |
| `100`      | -             | unused                                             |
| `101`      | (closure)     | reserved                                           |
| `110`      | immediate     | see below                                          |
| `111`      | typed object  | pointer to an object whose first word is a header  |

Fixnum `+` and `-` work directly on tagged values.

**Immediates** (low byte):

| Constant         | Encoding        |
|------------------|-----------------|
| `MINIM_FALSE`    | `0x06`          |
| `MINIM_TRUE`     | `0x0E`          |
| `MINIM_NIL`      | `0x26`          |
| `MINIM_VOID`     | `0x2E`          |
| `MINIM_EOF`      | `0x36`          |
| `MINIM_UNBOUND`  | `0x1E`          |
| `MINIM_FORWARD`  | `0x3E` (GC only) |
| char `c`         | `(c << 8) \| 0x16` (Unicode scalar value) |

**Heap layouts.**  Objects are 16-byte aligned and at least 2 words, so any
object can hold a forwarding record.

```
pair     (tag 001)   [ car | cdr ]                          16 B
symbol   (tag 011)   [ name : string | value ]              16 B
vector   (tag 111)   [ header | e0 | e1 | ... ]             8 + 8n B, rounded up to 16
string   (tag 111)   [ header | c0 c1 ... (uint32_t each) ] 8 + 4n B, rounded up to 16

vector header = (n << 4) | (immutable << 3) | 000
string header = (n << 4) | (immutable << 3) | 010
```

The immutable bits are reserved; nothing sets them yet.

Accessors subtract the tag:

```c
static inline minim_obj minim_car(minim_obj p) {
    return ((minim_obj *)(p - MINIM_TAG_PAIR))[0];
}
```

## Heap and collector

A simplified version of Chez's design (Dybvig, Eby, and Bruggeman 1994).

**Lifecycle.**  `minim_init()` and `minim_shutdown()` create and free the
heap.

**Chunks.**  Memory is `malloc`'d 64 KiB chunks; a larger object gets its own
chunk.  `static_assert(alignof(max_align_t) >= 16)` guarantees alignment.

**Allocation.**  New objects of every kind are interleaved in one *new area*
with a single bump pointer.

**Collection** is Cheney-style copying.  Pairs and symbols have no header, so
survivors are copied into per-kind to-spaces (`pair`, `symbol`, `typed`),
chosen by primary tag:

1. Copy every root.  The old object gets `MINIM_FORWARD` in word 0 and the new
   tagged pointer in word 1.
2. Scan each to-space linearly until all three cursors catch up: pair and
   symbol spaces in 16-byte strides, the typed space by header (vector
   elements are traced; strings are skipped).
3. Free all from-space chunks.

From-space is never scanned, so the mixed new area is never parsed.  With no
generations, every live object is in from-space at collection start, so no
segment table is needed.

**Not adopted from Chez:** generations, card marking, the segment table,
large-object handling, an unscanned data space, weak pairs, and guardians.
None affects object layouts.

**Trigger.**  Collect when bytes allocated since the last GC exceed
`max(MIN_HEAP, live_after_last_gc * GROWTH)`.

**Roots:**

- the shadow stack of `minim_obj *`:
  ```c
  size_t m = minim_gc_mark();
  MINIM_PROTECT(a);
  MINIM_PROTECT(b);
  minim_obj tail = minim_cons(b, MINIM_NIL);  // may GC; a and b are updated
  MINIM_PROTECT(tail);
  minim_obj r = minim_cons(a, tail);
  minim_gc_reset(m);
  return r;
  ```
- registered globals (`minim_gc_add_global(minim_obj *)`);
- the symbol table (Phase 6).

Rules:

- Protect any local that must survive a call that can allocate.
- Constructors protect their own arguments.
- Never nest allocating calls (`minim_cons(a, minim_cons(b, MINIM_NIL))`): C
  may read `a` before the inner call collects.

**Stress mode.**  `MINIM_GC_STRESS` collects on every allocation, exposing
unprotected roots.

## Phases

### Phase 1 - Build and test scaffolding

- `CMakeLists.txt`:
  - CMake 3.20 (test presets) and C17;
  - static library `minim_core` (`src/*.c` except `main.c`) plus the `minim`
    executable;
  - `target_include_directories(minim_core PUBLIC include PRIVATE src)`;
  - `enable_testing()` and a `tests/` subdirectory;
  - options `MINIM_SANITIZE` (ASan + UBSan) and `MINIM_GC_STRESS`.
- `CMakePresets.json`: configure, build, and test presets `dev` (Debug,
  sanitizers, `build/dev`) and `stress` (`dev` plus `MINIM_GC_STRESS`,
  `build/stress`).
- `include/minim.h`: empty, with include guards.
- `tests/test.h`: `CHECK(cond)` and `CHECK_EQ(a, b)`; failures print file and
  line and set a nonzero exit code.
- `tests/test_smoke.c`: one passing check.
- `README.md`: CMake 3.20+, C17, and the preset commands.

Separate because every later phase adds tests on top of it.

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev -R smoke
```

### Phase 2 - Value representation

- `include/minim.h`:
  - `minim_obj`, tag and immediate constants;
  - `minim_fixnum`, `minim_fixnum_value`, `MINIM_FIXNUM_MIN`,
    `MINIM_FIXNUM_MAX`, `minim_char`, `minim_char_value`;
  - `minim_is_fixnum`, `_pair`, `_symbol`, `_vector`, `_string`, `_char`,
    `_boolean`, `_null`;
  - `static_assert` on 64-bit pointers.

Separate because it is pure encoding, testable without a heap.

Tests (`tests/test_object.c`):

- fixnum round-trips at 0, ±1, `MINIM_FIXNUM_MIN`, and `MINIM_FIXNUM_MAX`;
- addition on tagged fixnums;
- char round-trips (ASCII, `U+10FFFF`);
- each immediate satisfies exactly one predicate;
- immediates and fixnums satisfy no heap predicate.

```sh
cmake --build --preset dev && ctest --preset dev -R object
```

### Phase 3 - Heap allocation (no collection)

- `src/heap.h`, `src/heap.c`: chunks, the new area, and
  `minim_heap_alloc(nbytes)` (rounds up to 16).  The heap only grows; failed
  `malloc` aborts.
- `include/minim.h`, `src/object.c`: `minim_init`, `minim_shutdown`,
  `minim_cons`, `minim_car`, `minim_cdr`, `minim_set_car`, `minim_set_cdr`,
  `minim_make_vector(n, fill)`, `minim_vector_length`, `minim_vector_ref`,
  `minim_vector_set`.

Separate so the allocator and layouts are reviewed before objects move.

Tests (`tests/test_heap.c`):

- build and read back lists and nested vectors;
- a length-0 vector takes 16 B;
- a vector larger than a chunk works;
- every object is 16-byte aligned with the right tag.

```sh
cmake --build --preset dev && ctest --preset dev -R heap
```

### Phase 4 - Precise copying collector

- `include/minim.h`: `minim_gc_mark`, `minim_gc_reset`, `MINIM_PROTECT`,
  `minim_gc_add_global`, `minim_gc_collect`.
- `src/gc.h`, `src/gc.c`: shadow stack, global roots, pair and typed
  to-spaces, forwarding, scan loop, trigger, and the `MINIM_GC_STRESS` hook in
  `minim_heap_alloc`.
- `src/object.c`: constructors protect their arguments.

The symbol to-space comes in Phase 6.

Separate because the collector is the riskiest change.

Tests (`tests/test_gc.c`, also in `stress`):

- a rooted list and vector survive intact;
- sharing is preserved (two roots to one pair stay `==`);
- cycles survive (a `set-cdr!` loop, a self-containing vector);
- unreachable data is reclaimed (live bytes drop);
- 1M allocations with a bounded live set keep the heap bounded.

```sh
cmake --build --preset dev && ctest --preset dev -R gc
cmake --preset stress && cmake --build --preset stress && ctest --preset stress -R gc
```

### Phase 5 - Strings

- `include/minim.h`, `src/object.c`: `minim_make_string(n, ch)`,
  `minim_string_length`, `minim_string_ref`, `minim_string_set`, and
  `minim_string_from_cstr` (decodes UTF-8).
- `src/gc.c`: the typed-space scan skips strings.

Separate because it is the first pointer-free typed object, so the scan loop
starts dispatching on the header.

Tests (`tests/test_string.c`, also in `stress`):

- ASCII and multi-byte UTF-8 round-trips;
- `minim_string_ref` / `minim_string_set` on non-ASCII chars;
- footprint is 8 + 4n bytes, rounded up to 16;
- strings in vectors and lists survive GC intact.

```sh
cmake --build --preset dev && ctest --preset dev -R string
cmake --build --preset stress && ctest --preset stress -R string
```

### Phase 6 - Symbols

- `include/minim.h`, `src/symbol.c`:
  - `minim_intern(minim_obj str)` and `minim_intern_cstr(const char *)`,
    backed by an open-addressing table in C memory.  It hashes by name, so no
    rehash after GC, and it is a strong root.
  - `minim_symbol_name`, `minim_symbol_value`, `minim_set_symbol_value`.  New
    symbols start as `MINIM_UNBOUND`.
- `src/gc.c`: the symbol to-space (16-byte strides, both fields traced).

Separate because the table is the first long-lived C-side root.

Tests (`tests/test_symbol.c`, also in `stress`):

- the same name interns to `==` symbols, from a string or a C string;
- distinct names give distinct symbols;
- symbols stay `==` and keep names and values across GC;
- the table grows past its initial capacity (10k symbols).

```sh
cmake --build --preset dev && ctest --preset dev -R symbol
cmake --build --preset stress && ctest --preset stress -R symbol
```

### After the last phase

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
cmake --preset stress && cmake --build --preset stress && ctest --preset stress
```

No linter is configured; the build must be warning-free.

## Open items

None.

## References

- R. K. Dybvig, D. Eby, C. Bruggeman.  *Don't Stop the BIBOP: Flexible and
  Efficient Storage Management for Dynamically-Typed Languages.*  Indiana
  University TR #400, 1994.  https://hdl.handle.net/2022/34235
- Chez Scheme `s/cmacros.ss`: tags, immediates, headers, layouts.
  https://github.com/cisco/ChezScheme/blob/main/s/cmacros.ss
- C. J. Cheney.  *A nonrecursive list compacting algorithm.*  CACM 13(11),
  1970.

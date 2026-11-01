# Object representation and garbage collection

Minim uses tagged 64-bit values and a precise, generational copying
collector.  Both are simplified from Chez Scheme
(Dybvig, Eby, and Bruggeman 1994).

## Values

A `minim_obj` is a `uintptr_t`.  Its low 3 bits are the primary tag; heap
objects are 16-byte aligned, so pointers carry the tag in their low bits.

| Low 3 bits | Kind | Notes |
|---|---|---|
| `000` | fixnum | 61-bit signed integer, `n << 3` |
| `001` | pair | `[car, cdr]` |
| `011` | symbol | `[name, value]` |
| `101` | closure | `[code, env]` |
| `110` | immediate | `#f`, `#t`, `'()`, void, eof, unbound, chars, forward marker |
| `111` | typed object | first word is a header |
| `010`, `100` | | reserved (flonum, -) |

Typed objects (vectors, strings, primitives, continuations, error objects)
start with a header, `(length << 4) | (immutable << 3) | type`.  Error objects
have type 1, the first odd code; an odd code can't equal the forwarding
marker.  Every object is at least
2 words, so any object can hold a forwarding record.

```
pair     [ car | cdr ]                        16 B
symbol   [ name : string | value ]            16 B
vector   [ header | e0 | e1 | ... ]           8 + 8n B, rounded up to 16
string   [ header | c0 c1 ... (uint32_t) ]    8 + 4n B, rounded up to 16
closure  [ code : (params . body) | env ]     16 B
primitive [ header | fn | name | min | max ]  40 B, rounded up to 48
continuation [ header | k | stack | height ]  32 B
error    [ header | kind | message | irritants ]  32 B
```

A continuation's `k` is its continuation frames, `stack` the frozen segments
of its value stack, and `height` a fixnum, where they end (`eval.md`).

A primitive's fields:

- `fn` is `minim_obj fn(minim_obj *args, size_t n)` and isn't traced.  `args`
  points into the value stack, so it stays valid across allocation
  (`eval.md`); primitives must not push onto it.
- `name` is a symbol, used for printing (`#<procedure car>`).
- `min` and `max` are fixnum arities; `max` is `MINIM_VARIADIC` (-1) for any
  number of arguments.  The evaluator checks them before calling `fn`.

## Heap

Memory is a set of 16 KiB chunks, each aligned to its size; a larger object
gets a chunk of its own, a multiple of 16 KiB, holding nothing else.  Every
object starts in its chunk's first 16 KiB, so clearing the low 14 bits of its
address finds its chunk (`minim_chunk_of`).  A chunk's header starts with its
generation.  Chunks come from `aligned_alloc` (`_aligned_malloc` on
MSVC); at 16 KiB, glibc and ASan serve them from their pools, while larger
aligned chunks would each be mapped from the OS.

A *space* is a chain of chunks in allocation order with a bump pointer into
the last one.  Generation 0, the nursery, is one space; each old generation,
1 to `max_generation`, has three:

| Space | Generation | Holds |
|---|---|---|
| nursery | 0 | every newly allocated object, all kinds interleaved |
| pair | 1 and up | pairs and closures |
| symbol | 1 and up | symbols |
| typed | 1 and up | typed objects |

Pairs and symbols have no header, so a space of mixed kinds can't be parsed.
The collector never parses the nursery; it sorts survivors by kind as it
copies them, as Chez does.

**Spare chunks.**  Chunks freed by a collection are queued as spares, oldest
first, and new chunks come from the queue before `aligned_alloc`.  After each
collection the queue is trimmed to as many chunks as are in use, plus the
nursery's worth and a quarantine, so a collection rebuilds what it copies
from spares and the heap stays within about twice what is in use.
Large chunks are freed.  Under AddressSanitizer, a chunk's data stays poisoned until each
object in it is allocated, and a spare is reused only after 8 newer ones (the
quarantine, `MINIM_CHUNK_QUARANTINE`), so a stale pointer into a collected
chunk still faults.

All collector state, including the spaces, spares, roots, and settings, is
one struct, `minim_gc` (`src/gc.h`).

## Collection

Collecting generation g (`minim_gc_collect_generation`) collects generations
0 through g and copies their survivors into g + 1, so an object ages a
generation per collection that includes it.  Collecting the oldest copies
every live object into fresh spaces for it, merging any generations above
it, left by lowering `max_generation`.  `minim_gc_collect_minor` and
`minim_gc_collect_major` collect generation 0 and the oldest.

Each collection is Cheney-style:

1. Copies every root.  An object's kind, and so its to-space, comes from the
   pointer's tag.  The old copy gets the forward marker in word 0 and the new
   pointer in word 1, so later references find it.
2. Scans the remembered objects (below) that may point into the collected
   generations.
3. Scans what was copied, from where the target's spaces ended before the
   collection, until no scan cursor moves.  Pairs, symbols, and closures are
   two traced words; a typed object is traced by its header (vector elements
   are, string chars and a primitive's C function pointer aren't).
4. Releases the from-space chunks.

**Write barrier.**  A collection must find every older object that points
into the generations it collects.  Every pointer write into an existing
object goes through `minim_set_car`, `minim_set_cdr`, `minim_vector_set`, or
`minim_set_symbol_value`, which end with the inline `minim_gc_write(obj, x)`:
if `x` is a heap object and `obj`'s chunk is old, `obj` is remembered.

Each chunk header has a byte per 16 bytes: for a remembered object starting
there, 1 + the youngest generation it may point to, otherwise 0, so an
object is in the remembered set at most once.  The barrier sets it to
generation 0.  A collection of 0 through g scans the remembered objects
above g whose youngest is at most g, then recomputes it for them, dropping
those that no longer point younger than themselves.  Remembered objects it
moved are dropped too: what they pointed to moved with them.  Constructors
and the collector write directly, without the barrier.

## Settings

`minim_gc_collect`, which allocation calls when the nursery is full, runs the
next collection on Chez's schedule: the n-th collects the largest g ≤
`max_generation` such that `radix`^g divides n.  Long-lived data drifts to
the oldest generation, collected geometrically less often.

The settings are a `minim_gc_config`, the first three after Chez's
`collect-trip-bytes`, `collect-generation-radix`, and
`collect-maximum-generation`:

| Setting | Meaning | Default |
|---|---|---|
| `nursery_bytes` | bytes allocated between collections | 1 MiB |
| `radix` | the schedule's base; 1 makes every collection collect the oldest | 4 |
| `max_generation` | the oldest generation, at most `MINIM_MAX_GENERATION` (8) | 4 |
| `stack_initial_capacity` | elements the root, value, and remembered stacks first allocate | 64 |

So by default the oldest generation is collected every 256 collections
(256 MiB of allocation).  Changing the settings restarts the schedule.

They are set by `minim_gc_set_config`, the `MINIM_GC_` environment variables
(`environment.md`), and the `collect-` primitives (`primitives.md`).  The
environment's settings apply from the start of `minim_init`.

**Stress testing.**  A 1-byte nursery collects at every allocation, so a
missing root moves a young object at the next allocation, where
AddressSanitizer reports the stale use; a missed barrier loses a young
object the same way.  An object in generation g moves only every `radix`^g
collections, so an unrooted reference to it is caught only at a site run
that often; radix 1 moves every object at every allocation.  The stress
tests are in `testing.md`.

## Roots

- **Locals.**  A shadow stack of `minim_obj *`, managed by frame macros
  modeled on OCaml's `CAMLparam` / `CAMLreturn`:
  ```c
  minim_obj list2(minim_obj a, minim_obj b) {
      MINIM_FRAME(&a, &b);
      minim_obj tail = minim_cons(b, MINIM_NIL);
      MINIM_PROTECT(&tail);
      MINIM_RETURN(minim_cons(a, tail));
  }
  ```
  Every local holding an object across an allocation must be protected, and
  allocating calls must not be nested in arguments.
- **Globals.**  Variables registered with `minim_gc_add_global`.
- **Value stack.**  The evaluator's values for calls in progress
  (`minim_gc.values`, `eval.md`): its buffer, updated in place, and its chain
  of frozen segments.  Also `minim_gc.spill`, the arguments `apply` gives a
  primitive beyond the stack's limit, `minim_gc.winders`, the
  `dynamic-wind`s in progress, and `minim_gc.handlers`, the exception
  handlers installed.
- **Remembered set.**  As above.
- **Symbol table.**  A vector in the heap, registered as a global.  It hashes
  by name, so moving symbols doesn't invalidate it.  It is strong: symbols are
  never freed, unlike Chez, which drops symbols with no references, value, or
  property list.

## Not included

Not included: a static generation, card marking, a segment table, non-copying large
objects, an unscanned space for pointer-free data, weak pairs, and guardians.

## References

- R. K. Dybvig, D. Eby, C. Bruggeman.  *Don't Stop the BIBOP: Flexible and
  Efficient Storage Management for Dynamically-Typed Languages.*  Indiana
  University TR #400, 1994.  https://hdl.handle.net/2022/34235
- Chez Scheme `s/cmacros.ss`: tags, immediates, headers, layouts.
  https://github.com/cisco/ChezScheme/blob/main/s/cmacros.ss
- C. J. Cheney.  *A nonrecursive list compacting algorithm.*  CACM 13(11),
  1970.

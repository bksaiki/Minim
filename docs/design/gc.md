# Object representation and garbage collection

Minim uses tagged 64-bit values and a precise, generational copying
collector with two generations.  Both are simplified from Chez Scheme
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

Typed objects (vectors, strings, primitives) start with a header,
`(length << 4) | (immutable << 3) | type`.  Every object is at least 2 words,
so any object can hold a forwarding record.

```
pair     [ car | cdr ]                        16 B
symbol   [ name : string | value ]            16 B
vector   [ header | e0 | e1 | ... ]           8 + 8n B, rounded up to 16
string   [ header | c0 c1 ... (uint32_t) ]    8 + 4n B, rounded up to 16
closure  [ code : (params . body) | env ]     16 B
primitive [ header | fn | name | min | max ]  40 B, rounded up to 48
```

## Heap

Memory is a set of 16 KiB chunks, each aligned to its size; a larger object
gets a chunk of its own, a multiple of 16 KiB, holding nothing else.  Every
object starts in its chunk's first 16 KiB, so clearing the low 14 bits of its
address finds its chunk (`minim_chunk_of`).  A chunk's header starts with its
generation, 0 or 1.  Chunks come from `aligned_alloc` (`_aligned_malloc` on
MSVC).

A *space* is a chain of chunks in allocation order with a bump pointer into
the last one.  There are four:

| Space | Generation | Holds |
|---|---|---|
| new | young (0) | every newly allocated object, all kinds interleaved |
| pair | old (1) | pairs and closures |
| symbol | old (1) | symbols |
| typed | old (1) | typed objects |

Pairs and symbols have no header, so a space of mixed kinds can't be parsed.
The collector never parses the new space; it sorts survivors by kind as it
copies them, as Chez does.

**Spare chunks.**  Chunks freed by a collection are queued as spares, oldest
first, and new chunks come from the queue before `aligned_alloc`.  At most the
nursery's worth plus a quarantine are kept; large chunks and the rest are
freed.  Under AddressSanitizer, a chunk's data stays poisoned until each
object in it is allocated, and a spare is reused only after 8 newer ones (the
quarantine, `MINIM_CHUNK_QUARANTINE`), so a stale pointer into a collected
chunk still faults.

All collector state, including the spaces, spares, roots, and settings, is
one struct, `minim_gc` (`src/gc.h`).

## Collection

An object is young until it survives a collection, which promotes it to the
old generation.  Both kinds of collection are Cheney-style copying:

- **Minor** (`minim_gc_collect_minor`): the from-space is the new space; old
  objects stay put.
- **Major** (`minim_gc_collect_major`): the from-space is every space, and
  the to-spaces are fresh old spaces, so every live object moves.

Each one:

1. Copies every root.  An object's kind, and so its to-space, comes from the
   pointer's tag.  The old copy gets the forward marker in word 0 and the new
   pointer in word 1, so later references find it.
2. In a minor collection, scans the remembered old objects (below).
3. Scans what was copied, from where the old spaces ended before the
   collection, until no scan cursor moves.  Pairs, symbols, and closures are
   two traced words; a typed object is traced by its header (vector elements
   are, string chars and a primitive's C function pointer aren't).
4. Releases the from-space chunks.

Afterward, nothing is young.

**Write barrier.**  A minor collection must find every old object that points
to a young one.  Every pointer write into an existing object goes through
`minim_set_car`, `minim_set_cdr`, `minim_vector_set`, or
`minim_set_symbol_value`, which end with the inline `minim_gc_write(obj, x)`:
if `x` is a heap object and `obj`'s chunk is old, `obj` is remembered.  A bit
per 16 bytes in each chunk header keeps an object in the remembered set at
most once.  A minor collection scans the set and clears it; a major one just
clears it.  Constructors and the collector write directly, without the
barrier.

## Settings

`minim_gc_collect`, which allocation calls when the nursery is full, runs the
collection the settings call for.  The settings are a `minim_gc_config`, the
first two after Chez's `collect-trip-bytes` and `collect-generation-radix`:

| Setting | Meaning | Default |
|---|---|---|
| `nursery_bytes` | bytes allocated between collections | 1 MiB |
| `radix` | every `radix`-th collection is major; 0 for none | 0 |
| `major_growth` | after a minor collection, a major one runs when the old generation exceeds this multiple of its size after the last major collection... | 2 |
| `major_min_bytes` | ...and this many bytes | 1 MiB |
| `stack_initial_capacity` | elements the root, value, and remembered stacks first allocate | 64 |

So by default long-lived data is copied once when promoted, then about once
per doubling of the old generation.

They are set by `minim_gc_set_config`, the `MINIM_GC_` environment variables
(`environment.md`), and the `collect-` primitives (`primitives.md`).  Startup
(`minim_init`) runs with the defaults; the environment's settings apply after
it, or from the start with `MINIM_GC_STARTUP=1`.

**Stress testing.**  A 1-byte nursery collects at every allocation, and radix
1 makes every collection major, so a missing root moves its object at the
next allocation, where AddressSanitizer reports the stale use.  Every test
has a copy, `<name>_stress`, run with those settings by the `stress` test
preset; `startup_stress` alone stresses startup.

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
- **Value stack.**  The evaluator's operator and operand values for calls in
  progress (`minim_gc.values`), updated in place.
- **Remembered set.**  In a minor collection, as above.
- **Symbol table.**  A vector in the heap, registered as a global.  It hashes
  by name, so moving symbols doesn't invalidate it.  It is strong: symbols are
  never freed, unlike Chez, which drops symbols with no references, value, or
  property list.

## Not included

More than two generations, card marking, a segment table, non-copying large
objects, an unscanned space for pointer-free data, weak pairs, and guardians.

## References

- R. K. Dybvig, D. Eby, C. Bruggeman.  *Don't Stop the BIBOP: Flexible and
  Efficient Storage Management for Dynamically-Typed Languages.*  Indiana
  University TR #400, 1994.  https://hdl.handle.net/2022/34235
- Chez Scheme `s/cmacros.ss`: tags, immediates, headers, layouts.
  https://github.com/cisco/ChezScheme/blob/main/s/cmacros.ss
- C. J. Cheney.  *A nonrecursive list compacting algorithm.*  CACM 13(11),
  1970.

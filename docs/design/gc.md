# Object representation and garbage collection

Minim uses tagged 64-bit values and a precise, generational copying
collector with two generations.  Both are simplified from Chez Scheme (Dybvig, Eby, and Bruggeman
1994).

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
gets a chunk of its own, a multiple of 16 KiB.  A chunk's header records its
generation: 0 in the new space, 1 in the others.  Every object starts in its
chunk's first 16 KiB, so clearing the low 14 bits of an object's address finds
its chunk (`minim_chunk_of`).  Chunks come from `aligned_alloc`
(`_aligned_malloc` on MSVC).

A *space* is a chain of chunks in allocation order with a bump pointer into
the last one.  There are four:

| Space | Holds |
|---|---|
| new | the young generation: every newly allocated object, all kinds interleaved |
| pair | old pairs and closures |
| symbol | old symbols |
| typed | old typed objects |

Pairs and symbols have no header, so a space of mixed kinds can't be parsed.
The collector never parses the new space; it sorts survivors by kind as it
copies them, as Chez does.

## Collection

An object is young until it survives a collection, which promotes it to the
old generation, the per-kind spaces.  Two kinds of collection, both
Cheney-style copying:

- **Minor** (`minim_gc_collect_minor`): the from-space is the new space.
  Old objects stay put: `copy` returns any object in an old chunk unchanged.
- **Major** (`minim_gc_collect_major`): the from-space is every space, and
  the to-spaces are fresh old spaces, so every live object moves.

Each one:

1. Copies every root.  An object's kind, and so its to-space, comes from the
   pointer's tag.  The old copy gets the forward marker in word 0 and the new
   pointer in word 1, so later references find it.
2. Scans the old spaces from their start until no scan cursor moves.  Pairs,
   symbols, and closures are two traced words; a typed object is traced
   according to its header (vector elements are traced, string chars aren't,
   and a primitive's C function pointer isn't).  Copying during the scan
   appends to the spaces being scanned.  In a minor collection, this scan
   also visits every old object that was already there, so old objects that
   point to young ones act as roots.
3. Frees every from-space chunk.

After either one, nothing is young.

**Policy.**  `minim_gc_collect` runs the collection the policy calls for, and
an allocation calls it when the budget runs out.  The policy
(`minim_gc_set_policy`, after Chez's `collect-trip-bytes` and
`collect-generation-radix`) has two settings:

| Setting | Meaning | Default |
|---|---|---|
| `trip_bytes` | bytes allocated between collections | 1 MiB |
| `radix` | every `radix`-th collection is major; 0 for none | 0 |

Otherwise the collection is minor, followed by a major one if the old
generation has grown past `max(1 MiB, 2 × its size after the last major
collection)`.  So long-lived data is copied once when promoted, then about
once per doubling of the old generation.

The build option `MINIM_GC_STRESS` instead runs a major collection every
`MINIM_GC_STRESS_INTERVAL` allocations (default 1, every allocation), which
exposes missing roots under AddressSanitizer.  Only interval 1 catches a root
missing across a single allocation.

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
  progress (`minim_values`), updated in place.
- **Symbol table.**  A vector in the heap, registered as a global.  It hashes
  by name, so moving symbols doesn't invalidate it.  It is strong: symbols are
  never freed, unlike Chez, which drops symbols with no references, value, or
  property list.

## Not included

More than two generations, a write barrier, a segment table, non-copying large objects, an
unscanned space for pointer-free data, weak pairs, and guardians.  None would
change object layouts.

## References

- R. K. Dybvig, D. Eby, C. Bruggeman.  *Don't Stop the BIBOP: Flexible and
  Efficient Storage Management for Dynamically-Typed Languages.*  Indiana
  University TR #400, 1994.  https://hdl.handle.net/2022/34235
- Chez Scheme `s/cmacros.ss`: tags, immediates, headers, layouts.
  https://github.com/cisco/ChezScheme/blob/main/s/cmacros.ss
- C. J. Cheney.  *A nonrecursive list compacting algorithm.*  CACM 13(11),
  1970.

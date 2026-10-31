# Object representation and garbage collection

Minim uses tagged 64-bit values and a precise, non-generational copying
collector.  Both are simplified from Chez Scheme (Dybvig, Eby, and Bruggeman
1994).

## Values

A `minim_obj` is a `uintptr_t`.  Its low 3 bits are the primary tag; heap
objects are 16-byte aligned, so pointers carry the tag in their low bits.

| Low 3 bits | Kind | Notes |
|---|---|---|
| `000` | fixnum | 61-bit signed integer, `n << 3` |
| `001` | pair | `[car, cdr]` |
| `011` | symbol | `[name, value]` |
| `110` | immediate | `#f`, `#t`, `'()`, void, eof, unbound, chars, forward marker |
| `111` | typed object | first word is a header |
| `010`, `100`, `101` | | reserved (flonum, -, closure) |

Typed objects (vectors, strings) start with a header,
`(length << 4) | (immutable << 3) | type`.  Every object is at least 2 words,
so any object can hold a forwarding record.

```
pair     [ car | cdr ]                        16 B
symbol   [ name : string | value ]            16 B
vector   [ header | e0 | e1 | ... ]           8 + 8n B, rounded up to 16
string   [ header | c0 c1 ... (uint32_t) ]    8 + 4n B, rounded up to 16
```

## Heap

Memory is a set of `malloc`'d 64 KiB chunks; a larger object gets a chunk of
its own.  A *space* is a chain of chunks in allocation order with a bump
pointer into the last one.  There are four:

| Space | Holds |
|---|---|
| new | every newly allocated object, all kinds interleaved |
| pair | pairs that survived a collection |
| symbol | symbols that survived a collection |
| typed | typed objects that survived a collection |

Pairs and symbols have no header, so a space of mixed kinds can't be parsed.
The collector never parses the new space; it sorts survivors by kind as it
copies them, as Chez does.

## Collection

Cheney-style copying:

1. Copy every root.  An object's kind, and so its to-space, comes from the
   pointer's tag.  The old copy gets the forward marker in word 0 and the new
   pointer in word 1, so later references find it.
2. Scan the pair, symbol, and typed spaces from their start until no scan
   cursor moves.  Pairs and symbols are two traced words; a typed object is
   traced according to its header (vector elements are traced, string chars
   aren't).  Copying during the scan appends to the spaces being scanned.
3. Free every from-space chunk.

The collector isn't generational, so every live object is in from-space when a
collection starts, and it needs no table mapping addresses to chunks.

**Trigger.**  A collection runs when an allocation would exceed the budget,
`max(1 MiB, 2 × live bytes after the last collection)`.  The build option
`MINIM_GC_STRESS` collects on every allocation instead, which exposes missing
roots under AddressSanitizer.

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
- **Symbol table.**  A vector in the heap, registered as a global.  It hashes
  by name, so moving symbols doesn't invalidate it.  It is strong: symbols are
  never freed, unlike Chez, which drops symbols with no references, value, or
  property list.

## Not included

Generations, card marking, a segment table, non-copying large objects, an
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

# Records: define-record-type, and error objects as records

Implementation plan.  The design is settled; what follows is the phase
breakdown, one phase per commit.

This starts roadmap milestone 8 with records, then rebuilds error objects on
them.

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

Minim has no way to define a new type.  R7RS 5.5 requires
`define-record-type`:

| Input | Today |
|---|---|
| `(define-record-type point (make-point x y) point? (x point-x))` | `unbound variable: define-record-type` |

Error objects (`errors.md`) are their own typed object, type code 1, with
their own collector cases, printing, and hashing: a one-off record type.

## Representation

After Chez Scheme (`s/cmacros.ss`, `c/gc.c`).

A record's first word is its *record-type descriptor* (rtd), a typed-object
pointer, so its low three bits are `111`.  As a header, that reads as type 7,
which no other type uses (`MINIM_TYPE_RECORD`): the first word is both the
record's type and its header.

```
record    [ rtd      | f0 | f1 | ... ]                8 + 8n B, rounded up to 16
rtd       [ base-rtd | name | nfields | fields ]      a record of base-rtd
base-rtd  [ itself   | name | nfields | fields ]      its own rtd
```

- An rtd is a record whose rtd is the base rtd, `minim_base_rtd`, a C global
  made at startup, whose rtd is itself.  Its fields: `name`, a symbol;
  `nfields`, a fixnum; `fields`, the list of field names.
- **Collector.**  A record's size is 1 + `nfields`, read from word 2 of its
  rtd.  Copying overwrites only words 0 and 1 (`MINIM_FORWARD` and the new
  address), so an rtd copied already, from either copy, still gives the size.
  A record's pointer fields start at word 0, so its rtd is copied and traced
  like any field.  The scan loop updates a record's first word before
  measuring it, so it reads the size from the copy.
- A record with no fields is 8 B, rounded up to 16, so it can hold a
  forwarding record like any object.
- Records are mutable: setting a field goes through the write barrier,
  `minim_gc_write`.  An rtd's fields are set only by C, at creation, so its
  `nfields` can't be corrupted.
- `equal?` compares records by identity (R7RS 6.1), and `equal-hash` hashes
  them alike, as it does closures, since their addresses move.
- A record prints as `#<name>`, an rtd as `#<record-type name>`, with one
  pair of angle brackets stripped from `name`: a type `<point>` prints as
  `#<point>`.

## define-record-type

`(define-record-type type (ctor cfield ...) pred (field accessor [modifier]) ...)`
is a `syntax-rules` macro in `src/expander.scm`.  It binds `type` to a new
rtd, as Chez does, and the rest to procedures from prelude helpers, which
resolve each field's index once, when the definition runs:

- `ctor`: a closure of fixed arity, so a wrong count is `arity mismatch`,
  that makes a record and stores its arguments.  Fields it doesn't name are
  `#f`.
- `pred`: whether its argument is a record of `type`.
- `accessor`, `modifier`: check that the record is of `type`, else raise
  `accessor: not a type` with the record as the irritant (the message built
  once, at definition), then read or set the field.  That is two primitive
  calls, `%record?` then `%record-ref`, keeping the primitives few and
  general at some cost in speed.

No inheritance, `nongenerative`, or other R6RS options: R7RS has none.

## Error objects as records

Error objects become records of a built-in rtd, `minim_error_rtd`, with the
fields `kind`, `message`, and `irritants`.  `MINIM_TYPE_ERROR`, its size, and
its collector and hashing cases go; `minim_make_error_object` and the
accessors in `minim.h` keep their signatures, so their callers don't change.
Error objects still print as `#<error message>`, by their rtd.

## Phases

### Phase 1 - Records and the collector

- `include/minim.h`: `MINIM_TYPE_RECORD`; `minim_is_record`,
  `minim_record_rtd`, `minim_record_length`, `minim_record_ref`,
  `minim_record_set` (with the barrier); `minim_make_record(rtd)` (fields
  `#f`), the only constructor; `minim_base_rtd`.
- `src/object.c`: the constructors, and the base rtd, made at startup and
  registered as a global.
- `src/gc.c`: the record cases of `typed_size` and `pointer_fields`.
- `src/print.c`, `src/prims.c`: printing, and `equal-hash`.

Separate because the collector's handling of a size read through another
object is the risky part, testable from C before anything uses it.

Tests (`tests/test_gc.c`): records and their rtds survive minor and major
collections, contents and all; a record copied before its rtd, and after it
(the rtd forwarded); a record with no fields; an old record pointing to a
young value through `minim_record_set` (the barrier); the base rtd is still
its own rtd after a collection.

```sh
cmake --build --preset dev && ctest --preset dev -R 'gc|object|print' && ctest --preset stress -R 'gc'
```

**Done.**
- One object type and one constructor: an rtd is
  `minim_make_record(minim_base_rtd)` with its fields set.  The rtd-specific C
  is the field layout (`MINIM_RTD_NAME`, `_NFIELDS`, `_FIELDS`), fixed because
  the collector reads `nfields`, and printing.  No rtd constructor or
  accessors: Scheme can't set an rtd's fields (`%record-set!` refuses), so
  only C makes rtds, `%make-record-type` and Phase 4's error rtd.
- The base rtd is made by `record_init` in `minim_init`, after the symbols;
  its `nfields` is set before anything allocates.
- The collector reads `nfields` raw (`rtd_nfields` in `gc.c`), since the
  rtd's first word may be the forwarding marker.
- The record tests pin the default collector settings: under stress, the
  allocations between the explicit collections promote the rtd early.  The
  barrier test fails, under ASan, with the barrier removed.
- Printing tests in `test_print.c`.  Ran `ctest -R 'gc|object|print'`, dev
  and stress.

### Phase 2 - Primitives

- `src/prims.c`: `(%make-record-type name fields)`, checking a symbol and a
  list of distinct symbols; `(%record? x rtd)`; `(%record-rtd r)`;
  `(%make-record rtd)`;
  `(%record-ref r i)` and `(%record-set! r i v)`, which check only that `r`
  is a record and `i` an index into it, and refuse to set an rtd's fields.

Separate because it is the whole Scheme interface, testable without the
macro.  An rtd's fields read through `%record-ref`, so no rtd accessors are
needed.

Tests: `tests/repl/records.scm` (new): each primitive and its errors;
printing; `equal?` on records; reading an rtd's name and fields.

```sh
cmake --build --preset dev && ctest --preset dev -R 'records|prims' && ctest --preset stress -R 'records'
```

**Done.**
- `%make-record` also refuses the base rtd: a record of it would be an rtd
  with `nfields` `#f`, which the collector can't size.
- `%make-record-type`'s distinctness check compares each field with the
  `len` before it, so a cyclic list repeats one and is refused rather than
  looping.  (A first version, comparing up to the current pair, looped on a
  cycle back to the head.)
- Error messages: `not a symbol`, `bad fields`, `not a record type`,
  `not a record`, `bad index`, `immutable record type`.
- Ran `ctest -R 'records|prims'`, dev and stress.

### Phase 3 - define-record-type

- `src/prelude.scm`: the helpers making the constructor, predicate,
  accessors, and modifiers.
- `src/expander.scm`: `define-record-type` among the derived forms.
  `src/prelude.boot` regenerated twice.

Separate because it is the user-facing syntax, on top of Phase 2.

Tests (`tests/repl/records.scm`): R7RS 5.5's `pare` example; a constructor
naming some fields, in another order; fields without modifiers; accessor and
modifier errors on the wrong type; constructor arity; definitions in a body;
two types with the same name are distinct; hygiene: a field named like a
global the expansion uses (`list`, `error`).

```sh
cmake --build --preset dev && ctest --preset dev -R 'records|expan|boot' && ctest --preset stress -R 'records'
```

### Phase 4 - Error objects as records

- `include/minim.h`, `src/object.c`: error objects as records of
  `minim_error_rtd`; `MINIM_TYPE_ERROR` and `MINIM_ERROR_OBJECT_SIZE` gone.
- `src/gc.c`, `src/prims.c`, `src/print.c`: their cases gone, but printing
  by the rtd.

Separate because it is a refactor, with the existing error tests as its net:
nothing they check changes, except that an error object is now a record.

Tests: the `errors` REPL test and `test_prims`, unchanged;
`tests/repl/records.scm`: an error object's rtd, from `%record-rtd`, prints
as `#<record-type error>`, and its fields read through `%record-ref`.

```sh
cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
```

### After the last phase

Sync the docs: `gc.md` (the record layout, type 7, the size through the rtd,
and why word 2; error objects no longer a type), `primitives.md` (the record
primitives), `prelude.md` (the helpers), `macros.md` (`define-record-type`),
`errors.md` (error objects as records), and the roadmap (milestone 8:
records done).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

None: the accessor's two calls and the printing of `<point>` are settled above.

# Bytevectors and binary ports

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

Minim has no bytevectors, so none of R7RS 6.9, and no binary ports, which
read and write them (6.13): 23 names, 21 in `(scheme base)` and
`open-binary-input-file` and `open-binary-output-file` in `(scheme file)`.
`binary-port?` is always false (`ports.md`).

| Report | Failing | Cause |
|---|---|---|
| 6.9 Bytevectors | 11, 28 errors | everything |
| 6.13 I/O | some of 2 failed, 18 errors | bytevector ports, `read-u8`, ... |
| Read syntax | 2 read errors | `#u8(...)` |

## Design

**A bytevector is a typed object** (`gc.md`) of type 5, the free code:
`[ header | b0 b1 ... ]`, `8 + n` bytes rounded to 16, the length in the
header, as a string's.  Its bytes aren't traced.  It lives in the typed
space, as strings do, until typed objects get spaces by type
(`beyond-r7rs.md`).  `minim.h` gets `minim_make_bytevector`,
`minim_is_bytevector`, `minim_bytevector_length`, `minim_bytevector_data`.

**Syntax.**  The reader reads `#u8(` followed by bytes, fixnums 0 to 255,
and `)`.  The printer writes `#u8(1 2 3)`.  Bytevectors are self-evaluating
like vectors, so the literal needs no quote.  `equal?` compares bytes;
`equal-hash` hashes them.

**The procedures** are C primitives in a new `src/bytevector.c`:
`bytevector`, `make-bytevector`, `bytevector?`, `bytevector-length`,
`bytevector-u8-ref`, `bytevector-u8-set!`, `bytevector-copy`,
`bytevector-copy!` (`memmove`), `bytevector-append`, and `utf8->string` and
`string->utf8`, through `char.c`'s UTF-8 coding; ranges by `range_args`.

**Binary ports** are ports with a new flag, `MINIM_PORT_BINARY`, after
Chez's.  A bytevector port's buffer is a bytevector, as a string port's is a
string: an input one reads a copy from `index`; an output one appends,
doubling when full.  A binary file port reads and writes bytes with
`getc` and `putc`.  The lookahead holds a byte or the end, as a textual
port's holds a code point.

- `textual-port?` is a port without the flag, `binary-port?` one with it.
- The textual procedures (`read-char`, `read`, `write`, ...) raise `not a
  textual port` on a binary one; the binary ones, `not a binary port` on a
  textual one.
- The current ports stay textual; `read-u8` and the others default to them,
  as R7RS has it, so they work when a binary port is parameterized in.
- A binary port prints as `#<binary input port name>`.

## Phases

### Phase 1 - The object and its syntax

- `include/minim.h`, `src/object.c`: the type, constructor, and accessors.
- `src/gc.c`: its size, and no traced fields.
- `src/read.c`, `src/print.c`: `#u8(...)`.
- `src/equal.c`: `equal?` and `equal-hash`.

First because everything after builds on the object.

Tests: `tests/test_object.c`, the accessors; `tests/test_gc.c`, a bytevector
survives collections, young and promoted, with its bytes; `tests/test_read.c`
and `tests/test_print.c`, `#u8()`, `#u8(0 255)`, a byte out of range's
error; `tests/repl/equality.scm`, `equal?` and `equal-hash` of bytevectors.
The read-syntax report updated.

```sh
cmake --build --preset dev && ctest --preset dev -R 'object|gc|read|print|equality|r7rs' && ctest --preset stress -R 'object|gc'
```

**Done.**  The reader reads `#u8` as a `#` token and requires `(` right
after it, so `#u8 (1)` is `bad # syntax`; bytes are checked as read (`bad
byte`).  `read_hash` lost its guard against pushing back non-ASCII to a C
string, which `unread` handles since the strings work.  The reports' read
errors on `#u8` became failures on the missing procedures: Read syntax 76
to 78 passed; 6.9's errors 28 to 8, the rest of them `#x41`, a hex number;
6.13's 18 errors became failures.

### Phase 2 - The procedures

- `src/bytevector.c`, in `CMakeLists.txt` and `prims.c`'s tables: the
  eleven primitives.
- `scheme/standard.scm`: the names.  `scheme/prelude.boot` regenerated.

Tests: `tests/repl/bytevectors.scm`, a new REPL test: each procedure, with
and without its range; `bytevector-copy!` within one bytevector both ways;
`utf8->string` of multibyte and malformed sequences; a byte out of range's
error.  The 6.9 report updated.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'bytevectors|r7rs' && ctest --preset stress -R bytevectors
```

**Done.**  6.9: 31 passed, none failed; its 8 errors are `#x41`, a hex
number, for flonums' syntax.  `utf8->string` decodes from a malloc'd,
NUL-terminated copy of the range, since `minim_utf8_decode` needs the NUL to
stop a truncated sequence, and its embedded NULs decode as U+0000.

### Phase 3 - Bytevector ports

- `include/minim.h`, `src/port.c`: `MINIM_PORT_BINARY`; byte reading and
  writing; `open-input-bytevector`, `open-output-bytevector`,
  `get-output-bytevector`; the textual procedures' check.
- `src/port.c`, `scheme/ports.scm`: `read-u8`, `peek-u8`, `u8-ready?`,
  `read-bytevector`, `read-bytevector!`, `write-u8`, `write-bytevector`,
  with optional ports as the textual ones have.
- `src/print.c`: binary ports' names.
- `scheme/standard.scm`: the names.  `scheme/prelude.boot` regenerated.

Tests: `tests/repl/ports.scm`: reading and peeking to the end, reading into
a range, writing then `get-output-bytevector`, `textual-port?` and
`binary-port?` of each kind, `read-char` on a binary port's error and
`read-u8` on a textual one's.  The 6.13 report updated.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'ports|port|r7rs' && ctest --preset stress -R ports
```

**Done.**  6.13: 42 to 61 passed; the last needs `write-shared`.  The
textual procedures' errors now say `not an open textual input port` (or
output); the binary ones', `not an open binary input port`.  `check_open`
checks direction, openness, and kind; `flush-output-port`, `close-port`, and
the `-open?` predicates take either kind.  `get-output-string` no longer
accepts a bytevector output port.  Growing a bytevector port's buffer
allocates, so `reserve_bytes` and `write_byte` take a reference their caller
roots.  `peek-u8` pushes back through `minim_port_unread_char`, which stores
any lookahead.  `byte_arg` moved from `bytevector.c` to `prims.h`.
`read-bytevector!` and `write-bytevector` pass their optional range to the
primitive, which checks it with `range_args`.

### Phase 4 - Binary file ports

- `src/port.c`: binary file reading and writing; `open-binary-input-file`
  and `open-binary-output-file`.
- `scheme/standard.scm`: their names in `(scheme file)`.

Separate because it is the file case, over `FILE *`, on Phase 3.

Tests: `tests/repl/ports.scm`: writing bytes, including ones that aren't
UTF-8, to a file and reading them back; `call-with-port` closing a binary
file port.

```sh
cmake --build --preset dev --target boot && cmake --build --preset dev && ctest --preset dev -R 'ports|r7rs' && ctest --preset stress -R ports
```

### After the last phase

Sync the docs: `gc.md` (the layout), `ports.md` (binary ports, no longer
waiting), `primitives.md` (Bytevectors, the port procedures), `repl.md`
(printing `#u8(...)` and binary ports), `r7rs-small.md` (coverage:
`(scheme base)`, `(scheme file)` 10 of 10; the conformance table; milestones
6 and 8).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

None.  Settled: bytevector literals are mutable, as string and vector
literals are in Minim (`primitives.md`), until literals become immutable
together.

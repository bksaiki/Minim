# Primitives

Primitive procedures are C functions, bound as global symbol values by
`minim_prims_init` (`src/prims.c`).  An area's file in `src/` (`pair.c`,
`string.c`, ..., alongside its C support in `char.c`, `error.c`, and
`system.c`) holds its primitives and a table of them, `{name, fn, min,
max}`, which `minim_prims_init` registers; `src/prims.h` has the table type
and the shared argument checks.  The evaluator calls them; see `eval.md`.

## Primitives the evaluator applies

`apply`, `load`, `values`, `call-with-values`, `%call/cc`, and
`%eval-expanded` need to evaluate code or return to the machine, which an
ordinary primitive can't do without re-entering `minim_eval`.  So they are
registered as primitives, but the evaluator recognizes their function
pointers (`minim_prim_apply`, ...) and never calls them:

- `apply` rewrites the value stack `[apply f a ... lst]` into
  `[f a ... elements of lst]` and applies `f`, so `apply` in tail position is
  a tail call.  Beyond the stack's limit (`eval.md`), a closure binds its
  parameters straight from the list, and a primitive gets its arguments in
  `minim_gc.spill`.
- `values` returns its arguments as the values (`eval.md`).
- `call-with-values` rewrites `[call-with-values producer consumer]` into
  `[consumer producer]` and applies the producer.  Its values return right
  above the consumer, which is then applied to them in tail position.
- `%call/cc` applies its argument to the continuation of the call, in tail
  position (`eval.md`).
- `load` reads the whole file with `minim_read_file` (`src/read.h`), which
  always closes it, even on a read error.  It then runs the forms as top-level
  forms (see `eval.md`) and returns void.  A read error therefore runs
  none of the file.
- `%eval-expanded` evaluates its argument, already in core forms, globally
  and in tail position, for `eval` (`macros.md`).

## Equality and hashing

- `equal?` walks pairs and vectors with an explicit stack, never C
  recursion, so arbitrarily deep structure is fine.  The stack is on the C
  stack until it outgrows 64 entries.
- A hash must agree with its equality, so `equal-hash` mirrors `equal?`:
  equal values visit the same values in the same order.  Its 64-value budget
  makes it O(1) and terminating on cyclic data.
- Every hash is stable under the moving collector: strings and symbols hash
  their chars (with a kind marker for symbols), primitives their C function
  pointer, and all closures one constant.  `eq?` hashing of heap objects, which
  needs a stable per-object identity, is left to hashtables (`beyond-r7rs.md`).
- The FNV-1a step (`minim_hash_mix`, `minim_string_hash` in `minim.h`) is
  shared with the symbol table.

## Library

### Numbers

Numbers are fixnums (exact) and flonums (inexact).  Arithmetic folds its
arguments left to right; a flonum argument makes the result a flonum from
there on, so a fixnum overflow before one still raises.

| Call | Result |
|---|---|
| `(+ n ...)` | sum; `0` with no arguments |
| `(- n)`, `(- n m ...)` | negation, or `n` minus the rest |
| `(* n ...)` | product; `1` with no arguments |
| `(/ n)`, `(/ n m ...)` | reciprocal, or `n` divided by the rest: exact if the fixnums divide evenly, else a flonum.  An exact zero divisor raises; an inexact one gives an infinity or NaN |
| `(abs n)` | absolute value |
| `(max n ...)`, `(min n ...)` | the greatest or least; inexact if any argument is, NaN if any is |
| `(quotient n m)` | `n / m`, truncated toward zero; fixnums only |
| `(remainder n m)` | remainder with the sign of `n`; fixnums only |
| `(= n ...)` | whether all are equal |
| `(< n ...)`, `(> ...)`, `(<= ...)`, `(>= ...)` | whether each adjacent pair is in order |
| `(zero? n)`, `(positive? n)`, `(negative? n)` | how `n` compares with 0 |
| `(odd? n)`, `(even? n)` | parity of the integer `n`, a fixnum or integral flonum |
| `(floor n)`, `(ceiling n)`, `(truncate n)`, `(round n)` | `n` rounded; `round` to even.  A fixnum is itself |
| `(exact n)`, `(inexact->exact n)` | the fixnum equal to `n`; raises `no exact representation` for a non-integer, infinity, or NaN, `fixnum out of range` past the range |
| `(inexact n)`, `(exact->inexact n)` | the flonum nearest `n` |
| `(number? x)`, `(complex? x)`, `(real? x)` | whether `x` is a number |
| `(rational? x)`, `(integer? x)` | whether `x` is a finite, or integral, number |
| `(exact? n)`, `(inexact? n)` | whether `n` is a fixnum, or flonum |
| `(exact-integer? x)`, `(fixnum? x)` | whether `x` is a fixnum |
| `(flonum? x)` | whether `x` is a flonum; Minim's own, after Chez, in `(minim)` |
| `(finite? n)`, `(infinite? n)`, `(nan? n)` | `(scheme inexact)`'s tests of `n` |
| `(number->string n [radix])` | `n` in radix 2, 8, 10 (default), or 16, as `write` prints it; a flonum's radix only 10 |
| `(string->number s [radix])` | the number `s` denotes, as the reader reads it, in the radix (default 10) unless a prefix gives one; `#f` if it isn't one |

Comparison is exact across types: a fixnum and a flonum compare as their
true values, though a fixnum may not fit a double.  Every comparison with
NaN is false.

### Chars

| Call | Result |
|---|---|
| `(char? x)` | whether `x` is a char |
| `(char->integer c)` | the code point of `c` |
| `(integer->char k)` | the char with code point `k`, which must be a Unicode scalar value |
| `(char=? c ...)`, `(char<? ...)`, `(char>? ...)`, `(char<=? ...)`, `(char>=? ...)` | whether each adjacent pair is in order, by code point |
| `(char-ci=? c ...)`, `(char-ci<? ...)`, ... | the same, by simple case folding |
| `(char-alphabetic? c)`, `(char-upper-case? c)`, `(char-lower-case? c)`, `(char-whitespace? c)` | Unicode's `Alphabetic`, `Uppercase`, `Lowercase`, `White_Space` |
| `(char-numeric? c)` | whether `c` is a decimal digit, general category `Nd` |
| `(digit-value c)` | the decimal digit `c`'s value, 0 to 9, or `#f` |
| `(char-upcase c)`, `(char-downcase c)`, `(char-foldcase c)` | Unicode's simple mappings |

Unicode data comes from tables (`src/unicode_data.inc`, read by
`src/unicode.c`) generated from the Unicode Character Database by
`tools/unicode.py UCD_DIR > src/unicode_data.inc`; regenerating for a new
Unicode version is rerunning it.  Lookups are binary searches over runs of
code points with the same properties, over rows of simple case mappings, and
over the few full mappings that differ from the simple ones.  A full mapping
takes a code point in its string, since lowercasing a sigma depends on its
neighbors (`Final_Sigma`); locale-specific mappings are left out.

### Pairs and lists

| Call | Result |
|---|---|
| `(cons a d)` | new pair |
| `(car p)`, `(cdr p)` | first or second field of the pair `p` |
| `(caar p)` to `(cddddr p)` | compositions of two to four `car`s and `cdr`s, `(cadr p)` being `(car (cdr p))`; a non-pair along the way raises `<name>: not a pair` with `p`.  The three- and four-level ones are `(scheme cxr)` |
| `(set-car! p x)`, `(set-cdr! p x)` | sets a field of `p`; void |
| `(pair? x)` | whether `x` is a pair |
| `(null? x)` | whether `x` is `'()` |
| `(list x ...)` | new list of the arguments |
| `(length lst)` | number of elements in the proper list `lst` |
| `(apply f x ... lst)` | `f` applied to the `x`s and the elements of `lst` |
| `(values x ...)` | the `x`s, as any number of values |
| `(call-with-values producer consumer)` | `consumer` applied to the values of `(producer)` |
| `(%call/cc f)` | `f` applied to the continuation of the call, a `#<continuation>`; for the prelude's `call/cc` |
| `(%winders)`, `(%set-winders! w)` | the `dynamic-wind`s in progress, innermost first, as `(before . after)` pairs; sets them |
| `(%handlers)`, `(%set-handlers! h)` | the exception handlers installed, innermost first; sets them |
| `(%raise-uncaught obj)` | leaves Scheme with `obj`, which no handler took (`errors.md`) |

### Errors

| Call | Result |
|---|---|
| `(error-object? x)` | whether `x` is an error object |
| `(error-object-message e)`, `(error-object-irritants e)` | the message string, or the list of irritants, of the error object `e` |
| `(read-error? x)`, `(file-error? x)` | whether `x` is an error object from reading a file, or from failing to open one, in `load` |
| `(%make-error-object message irritants)` | a new error object of the string `message` and the list `irritants`; for the prelude's `error` |

### Records

For the prelude's `define-record-type`.  An rtd's fields (`gc.md`) read
through `%record-ref`, but only `%make-record-type` sets them.

| Call | Result |
|---|---|
| `(%make-record-type name fields)` | a new rtd named by the symbol `name`, with the list of distinct symbols `fields` |
| `(%make-record rtd)` | a new record of `rtd`, its fields `#f`; not of the base rtd |
| `(%record? x rtd)` | whether `x` is a record of `rtd` |
| `(%record-rtd r)` | the rtd of the record `r` |
| `(%record-ref r i)`, `(%record-set! r i v)` | field `i` of the record `r`; sets it, refusing an rtd (`immutable record type`) |

### Identity and types

| Call | Result |
|---|---|
| `(eq? a b)` | whether `a` and `b` are the same object |
| `(eqv? a b)` | same as `eq?`, except that flonums with the same bits are `eqv?`: `(eqv? 0.0 -0.0)` is `#f` |
| `(equal? a b)` | whether `a` and `b` have the same structure: pairs, vectors, and strings compared element by element, everything else, records included, by `eqv?`.  Cyclic data loops forever |
| `(equal-hash x)` | a non-negative fixnum, equal for `equal?` values, hashing flonums' bits.  Visits at most 64 values of `x`, so it terminates on cyclic data.  Closures, continuations, error objects, and records all hash alike |
| `(not x)` | whether `x` is `#f` |
| `(symbol? x)`, `(procedure? x)`, `(boolean? x)` | whether `x` is of that type |
| `(boolean=? b ...)`, `(symbol=? s ...)` | whether the booleans, or symbols, are all the same |
| `(%closure-code x)` | the closure `x`'s form, `(lambda ...)` or `(case-lambda ...)`, shared by every closure it makes; `#f` for anything else |
| `(void)` | void; Minim's own, after Chez, in `(minim)` |

### Vectors

| Call | Result |
|---|---|
| `(vector x ...)` | new vector of the arguments |
| `(make-vector k [fill])` | new vector of length `k`, filled with `fill` (default 0) |
| `(vector-ref v i)` | element `i` of `v` |
| `(vector-set! v i x)` | sets element `i` of `v`; void |
| `(vector-length v)` | length of `v` |
| `(vector->list v [start [end]])` | the elements of `v`, as a list |
| `(list->vector lst)` | a new vector of the elements of the proper list `lst` |
| `(vector? x)` | whether `x` is a vector |
| `(vector-copy v [start [end]])` | a new vector of the elements of `v` |
| `(vector-copy! to at from [start [end]])` | copies elements of `from` into `to` at `at`, correctly when they overlap; void |
| `(vector-fill! v x [start [end]])` | sets elements of `v` to `x`; void |
| `(vector-append v ...)` | a new vector of the arguments' elements in order |
| `(string->vector s [start [end]])`, `(vector->string v [start [end]])` | a new vector of the chars of `s`; a new string of the chars in `v` |

### Bytevectors

| Call | Result |
|---|---|
| `(bytevector b ...)` | new bytevector of the bytes, fixnums 0 to 255 |
| `(make-bytevector k [b])` | new bytevector of length `k`, filled with `b` (default 0) |
| `(bytevector? x)` | whether `x` is a bytevector |
| `(bytevector-length bv)` | length of `bv` |
| `(bytevector-u8-ref bv i)`, `(bytevector-u8-set! bv i b)` | byte `i` of `bv`; sets it, void |
| `(bytevector-copy bv [start [end]])` | new bytevector of the bytes of `bv` |
| `(bytevector-copy! to at from [start [end]])` | copies bytes of `from` into `to` at `at`, correctly when they overlap; void |
| `(bytevector-append bv ...)` | new bytevector of the arguments' bytes in order |
| `(utf8->string bv [start [end]])` | new string the bytes encode in UTF-8; a malformed sequence decodes to U+FFFD |
| `(string->utf8 s [start [end]])` | new bytevector of the UTF-8 encoding of the chars of `s` |

### Strings and symbols

| Call | Result |
|---|---|
| `(string? x)` | whether `x` is a string |
| `(make-string k [c])` | new string of length `k`, filled with `c` (default `#\space`) |
| `(string-length s)` | length of `s` |
| `(string-ref s i)` | char `i` of `s` |
| `(string-set! s i c)` | sets char `i` of `s`, which must be mutable (symbol names aren't); void |
| `(string c ...)` | new string of the chars `c ...` |
| `(string-append s ...)` | new string of the arguments' chars in order |
| `(substring s start end)`, `(string-copy s [start [end]])` | new string of chars `start` through `end - 1` |
| `(string-copy! to at from [start [end]])` | copies chars of `from` into the mutable `to` at `at`, correctly when they overlap; void |
| `(string-fill! s c [start [end]])` | sets chars of the mutable `s` to `c`; void |
| `(string=? s ...)`, `(string<? ...)`, `(string>? ...)`, `(string<=? ...)`, `(string>=? ...)` | whether each adjacent pair is in order, lexicographically by code point |
| `(string-ci=? s ...)`, `(string-ci<? ...)`, ... | the same, of the strings' full case foldings |
| `(string-upcase s)`, `(string-downcase s)`, `(string-foldcase s)` | new string of Unicode's full mappings, which may change its length (`ß` upcases to `SS`); downcasing makes a capital sigma ending a word final (`ς`) |
| `(string->list s [start [end]])` | list of the chars of `s` |
| `(list->string lst)` | new string of the chars in the list `lst` |
| `(symbol->string sym)` | the name of `sym`, an immutable string (not a copy) |
| `(string->symbol s)` | the symbol named `s` |
| `(gensym [s])` | a new uninterned symbol named `s` (default `g0`, `g1`, ...): it prints like its name but is `eq?` to no other symbol |
| `(string-hash s)` | a non-negative fixnum, equal for `string=?` strings |

### Expander support

For the macro expander in the prelude; the `%` marks them internal.

| Call | Result |
|---|---|
| `(%make-alias id env)` | a new alias: an uninterned symbol named like the symbol `id`, recording `id` and `env` |
| `(%alias-original x)`, `(%alias-env x)` | the `id` or `env` an alias was made with; `#f` if `x` isn't an alias |
| `(%bound? sym)` | whether the global `sym` holds a value; `#f` for an alias |
| `(%getprop sym key default)` | the property `key` of `sym`, keys compared by `eq?`, or `default` |
| `(%putprop! sym key value)` | sets the property `key` of `sym`; void |
| `(%syntax-error message args)` | raises the string `message` with `args` as the irritant: void if empty, its element if it has one |

An alias stores `#(tag id env)` in its value slot, where `tag` is a private
uninterned symbol, so no value a program can build looks like an alias.

### Ports

The port primitives are in `src/port.c`, listed in `ports.md`.  Those whose
port is optional are Scheme wrappers (`scheme/ports.scm`) over primitives
taking it: `%read`, `%read-char`, `%peek-char`, `%read-line`,
`%read-string`, `%char-ready?`, `%write`, `%display`, `%write-char`,
`%write-string`, `%flush-output-port`, `%read-u8`, `%peek-u8`,
`%u8-ready?`, `%read-bytevector`, `%read-bytevector!`, `%write-u8`, and
`%write-bytevector`.  `(%standard-port i)` is the standard input (0), output
(1), or error (2) port.

### Storage

After Chez Scheme's storage-management procedures; see `gc.md`.

| Call | Result |
|---|---|
| `(collect)` | runs the next scheduled collection; void |
| `(collect-nursery-bytes)`, `(collect-nursery-bytes n)` | bytes allocated between collections; `n` sets it, a positive fixnum |
| `(collect-generation-radix)`, `(collect-generation-radix r)` | the schedule's base: generation g is collected every `r`^g collections; `r` sets it, a positive fixnum |
| `(collect-maximum-generation)`, `(collect-maximum-generation g)` | the oldest generation; `g` sets it, from 1 to 8 |

### Files

| Call | Result |
|---|---|
| `(load path)` | runs the forms in the file `path`, relative to the current directory, as top-level forms; void |
| `(file-exists? path)` | whether the file `path` opens for reading |
| `(delete-file path)` | deletes the file `path`; a file error if it can't; void |
| `(%read-file path)` | the forms of the file `path`, as `load` reads them; for `include` |

### System

| Call | Result |
|---|---|
| `(get-environment-variable name)` | the value of the environment variable `name`, a string, or `#f` if it is unset |
| `(get-environment-variables)` | the environment, as a list of `(name . value)` strings |
| `(command-line)` | the script's or program's path, then its arguments, as strings; `("minim")` at the REPL |
| `(%exit status)` | shuts the runtime down, flushing and closing file ports, and ends the process with the fixnum `status` |
| `(current-second)` | seconds since the epoch, UTC (R7RS allows it for TAI), a flonum |
| `(current-jiffy)` | nanoseconds since Minim started, a fixnum, on a monotonic clock (`minim_monotonic_ns`), so it never goes backward when the wall clock is set |
| `(jiffies-per-second)` | 1000000000 |

## Conventions

- Arithmetic that leaves the fixnum range raises `<name>: fixnum overflow`;
  dividing by zero raises `<name>: division by zero`.
- Type errors are `<name>: not a <type>`.
- Index errors are `<name>: bad index`, covering non-fixnum, negative, and
  out-of-range indices.
- Optional `start` and `end` arguments (`range_args`) default to the whole
  sequence; `start <= end` is checked.
- Strings are mutable except symbol names; `string-set!` on one raises
  `string-set!: immutable string`.  String literals are mutable, unlike R7RS.

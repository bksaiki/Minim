# Roadmap: the compiler

From an evaluator over expanded core forms to a compiler, written in Scheme,
that simplifies them in small passes, nanopass style, for an evaluator
optimized for the simplified forms, and eventually a bytecode compiler.  Each
milestone gets its own implementation plan in `docs/todos/` when it starts;
this document only orders the work and records the decisions each one needs.

## Where things stand

The expander (`macros.md`) turns each top-level form into the core forms:
`quote`, `if`, `define`, `set!`, `lambda`, `case-lambda`, `begin`,
`define-values`, `let-values`, and `letrec-values`.  The evaluator
(`eval.md`) runs them as lists, an explicit-stack machine, checking each form
every time it runs.  A local variable is found by name, in a chain of frames,
one per call and per `let-values`; a global in its symbol's value slot.  A
closure holds its `lambda` form and the environment it was made in.

The prelude is interpreted, so a procedure written in Scheme costs a closure
call and a lookup by name per variable: moving primitives into Scheme made
`string-append` 43 times slower (`primitives.md`).

## Target

Two core languages, both runnable:

- **The unsimplified core**: the expander's output, as today, run by today's
  machine.  It stays the fallback, for bootstrapping and for C code that
  evaluates without the prelude.
- **The simplified core**: the compiler's output, run by an evaluator that
  checks nothing, since the compiler did, and finds every variable by
  position.

The compiler is Scheme (`scheme/compile.scm`), a sequence of small passes,
each from one intermediate language to the next, checked in tests by a
checker per language.  The evaluator calls it through a hook, `%compile`,
after `%expand`, as it calls the expander; with the hook unbound, the
unsimplified core runs.  So the suite can run both ways, and must agree.
The boot file holds compiled code: `boot_gen` expands and compiles the
prelude with itself, regenerated twice, as the expander already is.

Only the last pass, `emit`, and the evaluator know the output's form.  So the
passes before it carry over when the tree-walking evaluator gives way to a
bytecode compiler and virtual machine.

## Milestones

Ordered by dependency.  Sizes are rough: S is one or two phases, M three to
five, L more.  Each milestone measures `bench/run.sh` against `main`, with
the microbenchmarks of the procedures the primitives review moved into
Scheme.

### 1. The pipeline (M)

- The simplified core, lists with symbol heads, run by the machine beside
  the unsimplified core.  Which language runs is known by context, never by
  inspecting a form: compiled code arrives only from the compiler, and a
  closure made by compiled code holds a `clambda`, a head of its own.
- `scheme/compile.scm`: `parse`, which checks and normalizes (`case-lambda`
  clauses, missing `else`s, bodies), and `emit`.  The first simplified core
  mirrors the unsimplified one, variables still by name, so the pipeline,
  the hook, and the evaluator's two languages land before any change of
  representation.
- The `%compile` hook; the suite run with it bound and unbound.

### 2. Flat closures and frame addresses (M–L)

The first optimization.  New passes:

- `uncover-free` and `uncover-assigned`, annotating each `lambda` with its
  free variables and each variable with whether it is assigned.
- `convert-assigned`: a variable both assigned and captured lives in a box,
  allocated where it is bound; one assigned but not captured stays in its
  slot.
- `convert-closures`: a closure holds its code and the values of its free
  variables, a flat closure.
- `assign-frames`: one frame per call, `[free ... params ... locals ...]`,
  the closure's free values copied in on entry and the body's `let-values`
  variables in the same frame, so `let` allocates nothing.  Every local is
  `(local n)`, every global `(top id)`.

A `letrec-values` binding only `lambda`s, which is every named `let`, would
box its variables, each captured by its own closure, and a loop would read
through a box per iteration.  So this milestone also fixes the simple case:
allocate the closures, then fill in their free values.

Depends on 1.

### 3. A compiled boot file (S)

`boot_gen` compiles the prelude as it expands it, so the prelude runs
compiled, the compiler included.  Startup measured.

Depends on 2.

### 4. Primitive calls (S–M)

An imported binding can't be changed (`macros.md`), so a call of an
imported primitive, `(car x)` in a program or library, can call it directly,
`(primcall car x)`, without looking up the variable or checking that it is
a procedure.  Later, with types, unchecked calls where the compiler proves
the arguments' types (`beyond-r7rs.md`, unchecked primitives).

Depends on 2.

### 5. Fixing letrec (M)

The full treatment of `letrec-values`, after Waddell, Sarkar, and Dybvig,
"Fixing Letrec: A Faithful Yet Efficient Implementation of Scheme's
Recursive Binding Construct" (2005), and Ghuloum and Dybvig, "Fixing Letrec
(reloaded)" (2009): partitioning bindings into unreferenced, simple,
`lambda`, and complex ones, so only the complex ones pay for boxes and
checks.

Depends on 2.

### 6. Multiple values (M)

After Ashley and Dybvig, "An Efficient Implementation of Multiple Return
Values in Scheme" (1994): a `call-with-values` whose consumer is a `lambda`,
and `let-values`, receive the producer's values straight into frame slots,
with no list or apply.

Depends on 2.

### 7. Inlining and known calls (M)

Calls of procedures the compiler can see, `((lambda ...) ...)` and calls of
a `letrec`-bound `lambda`: arity checked once, at compile time; small ones
inlined; constants folded.

Depends on 5.

### 8. Bytecode (L)

`emit` produces bytecode for a virtual machine, a loop over instructions,
instead of tree nodes; the tree-walking evaluator goes.  The passes before
`emit` are unchanged.

Depends on 2; best after 4 to 7, which shape the instructions.

## Open items

### A separate hook, or `%expand` returning compiled code?

- **A separate `%compile`**, called by the evaluator after `%expand`, is one
  place that compiles, and one switch: `eval`, programs, and the prelude
  reach the evaluator with core forms through `minim_eval_expanded` and
  `%eval-expanded`, not only through `%expand`.
- **Folding it into `%expand`** adds no hook, but each producer of core
  forms must remember to compile.
- **Provisional:** a separate hook.

### How does a closure say it is compiled?

- **By its code's head**: compiled code's `lambda` is a `clambda`, so a
  closure's code says which language made it.  No layout change, and
  `procedure-rename` and `%closure-code` (`parameter?`) work as they are, a
  `lambda`'s closures sharing its node.
- **A flag bit**: closures have no header, so it needs a fourth word.
- **Settled:** the code's head.

### Nodes: lists with symbol heads, or vectors with fixnum kinds?

- **Lists with symbol heads** read well, in the boot file too, are easy for
  the Scheme passes to build, and are the shape the evaluator handles today.
- **Vectors with fixnum kinds** let the evaluator `switch` on a kind and
  reach a node's parts without walking `cdr`s, but C and Scheme must agree
  on the numbering.
- **Settled:** lists with symbol heads for now; both are small next to
  milestone 2's gains, and bytecode (8) replaces nodes altogether.  Reopen
  if measurements show dispatch matters before then.

### Keep the unsimplified evaluator?

- **Keep it**: the fallback while the compiler is built, for C tests without
  the prelude, and the other side of running the suite both ways.
- **Retire it** once everything compiles: one machine to maintain.
- **Provisional:** keep it until bytecode (8), then reconsider.

### Write passes by hand, or a nanopass framework?

- **By hand**: each pass a recursive procedure dispatching on node kinds,
  with a checker per intermediate language run in tests.
- **A small `define-language` / `define-pass`**, as the nanopass framework,
  generates the checkers and the boilerplate, but is a project of its own.
- **Provisional:** by hand; reopen if the boilerplate grows past a few
  passes.

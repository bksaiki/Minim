# Conformance: Chibi's R7RS test suite

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

Milestone 10 (`r7rs-small.md`).  R7RS has no official conformance suite; the
de facto one is Chibi Scheme's `tests/r7rs-tests.scm`, by Alex Shinn, who
chaired the R7RS-small working group: one file, 2530 lines, 989 `test`
cases in 21 sections following the report (4.1 to 6.14, plus read syntax and
numeric syntax), "all procedures and syntax in the small language except
`delete-file`".  Picrin and rmosh carry copies; a SRFI 64 version passes on
Chibi, Gauche, Kawa, Larceny, and Sagittarius.

Today Minim measures conformance only by names (the coverage table), not
behavior.  The suite can't run as is:

| Obstacle | Where |
|---|---|
| Its `import` names libraries Minim lacks: `(scheme char)`, `(scheme lazy)`, `(scheme inexact)`, `(scheme complex)`, `(scheme time)`, `(scheme case-lambda)`, `(chibi test)` | line 3 |
| Read syntax Minim lacks would end a whole file's load: flonums, `#e`/`#x` prefixes, `#\alarm`, `\a` escapes, block comments, datum labels | throughout |
| Writing a cyclic list hangs Minim's printer | line 2147 |

Its other dependency is a small part of `(chibi test)`: `test`, `test-assert`,
`test-error`, `test-values`, `test-begin`, and `test-end`.  The file defines
its other helpers (`test-numeric-syntax`, `test-write-syntax`, ...) in the
sections that use them, so sections run independently.

## Design

**The suite is vendored verbatim**, pinned to Chibi commit `f966f15`, as
`tests/r7rs/r7rs-tests.scm`, with Chibi's license as `tests/r7rs/COPYING`
(BSD-style, which allows it with the notice).  Updating it is replacing the
file.

**`command-line`** (R7RS 6.14, `(scheme process-context)`) comes first, so
the runner can take its section as an argument.  Options are parsed only
before the file; everything after it is the script's or program's, even
what looks like an option, as in Chez and Python: `minim [--program] [file
[arg ...]]`.  `(command-line)` is the script's path, then its arguments, as
Chez gives a script's; at the REPL, `("minim")`.  `main` passes its
arguments with `minim_set_command_line(argc, argv)`, whose strings live as
long as the process.

**A runner, `tests/r7rs/run.scm`**, a Minim script, runs one section, named
by its argument, `minim run.scm "6.4 Lists"`:

- It reads the file's lines, `read-line`, and takes the section's: from its
  `(test-begin "title")` to the matching `(test-end)`, leaving out nested
  sections, which run on their own ("Read syntax" and "Numeric syntax" in
  6.13).  `test-begin` and `test-end` start lines at column 0.
- It cuts the section into chunks at lines starting a form at column 0, as
  every top-level form in the file does, and reads each chunk's data from a
  string port.  A read error ends only its chunk.  This is what keeps
  unsupported read syntax from ending the section.
- It evaluates each datum with `eval` in the interaction environment, under
  `guard`, so an error ends only its datum.
- It skips the chunks starting at the lines of a skip list, each with its
  reason: a test that would hang Minim, not one that merely fails.
- It reports one line per failing test, error, and read error, with the line
  where its chunk starts, then a count: `6.4 Lists: 80 passed, 3 failed,
  1 error`.

**The shim**, in the runner: `test` (with or without a name), `test-assert`,
`test-error`, `test-values`, `test-begin`, and `test-end`, after the file's
own sketch and `(chibi test)`'s behavior: `equal?`, a test's error counted as
a failure, `test-error` passing on any raise.

**Each section is a test**, `r7rs_<slug>` (`r7rs_4.1`, `r7rs_read-syntax`),
golden like the script tests: its expected output is today's full report,
failures included, not just counts.  A change that fixes or breaks a test
changes the report, so the diff shows exactly which tests changed, by line,
and a regression can't hide behind a fix in the same section.  A target,
`r7rs-golden`, rewrites the expected outputs after a deliberate change.
The sections have no stress copies: about 989 tests through the expander
would take minutes per section under GC stress, and the unit and REPL tests
already stress the same code.

## Phases

### Phase 1 - command-line

- `src/main.c`: options only before the file; the arguments after it, and
  the usage message.
- `include/minim.h`, `src/system.c`: `minim_set_command_line` and the
  primitive `command-line`, in `(scheme process-context)`.
  `scheme/prelude.boot` regenerated.

Separate because it is a missing procedure, and a change to the command
line, that the runner then uses.

Tests (`tests/scripts/`): `(command-line)` of a script given `a b`, of a
program given them after `--program file.scm`, and of a script given `--x`,
which isn't an option after the file; at the REPL, `("minim")`; `minim
--program` without a file still a usage error.

```sh
cmake --build --preset dev && ctest --preset dev -R 'script|libraries'
```

### Phase 2 - The runner, on one section

- `tests/r7rs/r7rs-tests.scm`, `tests/r7rs/COPYING` (new): the suite and
  its license.
- `tests/r7rs/run.scm` (new): the runner and shim.
- `tests/CMakeLists.txt`: `minim_add_r7rs_test(slug title)`, running the
  runner from `tests/r7rs/` through `run_minim.cmake`; `r7rs_4.1` and its
  expected output.

Separate because it is the machinery, checked on one section before the rest
depend on it.

Tests: `r7rs_4.1` itself.  Its report checked by hand against the section:
each failure is a real gap, not the runner's.  The runner's chunking checked
on 4.2, whose forms include tab-indented and one-space-indented lines.

```sh
cmake --build --preset dev && ctest --preset dev -R 'r7rs'
```

### Phase 3 - Every section

- `tests/CMakeLists.txt`: the other 20 sections, and the `r7rs-golden`
  target.
- `tests/r7rs/run.scm`: the skip list, with the cyclic `write` at line 2147
  and any other hang found.
- The expected outputs, one per section.

Separate because it records Minim's conformance, section by section, on
Phase 2.

Tests: every `r7rs_` test.  Each section's report read through for runner
mistakes; every skip justified by a hang.

```sh
cmake --build --preset dev && ctest --preset dev -R 'r7rs'
```

### After the last phase

Sync the docs: `testing.md` (the conformance tests, the runner, updating the
expected outputs), `repl.md` (the command line's arguments), `primitives.md`
(`command-line`), and the roadmap (`r7rs-small.md`: milestone 10, with a
table of each section's passed and total tests beside the coverage table;
`(scheme process-context)`'s coverage).

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev && ctest --preset stress -j 8
cmake --preset release && cmake --build --preset release
```

## Open items

None: the discussion settled them.  The runner takes its section through
`command-line` (Phase 1), the expected outputs are full reports, and the
sections have no stress copies.

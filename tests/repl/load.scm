; A loaded file's definitions are visible afterward.
(load "load/defs.scm")
(square 4)
loaded-value

; A loaded file can load another.
(load "load/nested.scm")
nested-value

; load returns void, and an empty file loads.
(eq? (load "load/defs.scm") (if #f #f))
(load "load/empty.scm")

; Errors; the REPL continues after each.
(load "load/missing.scm")
(load 5)
(load "load/bad-read.scm")
never-defined  ; the file is read in full before any of it runs
(load "load/bad-eval.scm")
before-error
after-error
'done

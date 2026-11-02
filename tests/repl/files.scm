; file-exists?: a file, a directory's file, and a missing one.
(file-exists? "files.scm")
(file-exists? "load/defs.scm")
(file-exists? "load/missing.scm")

; get-environment-variable: an unset variable (a set one is a script test).
(get-environment-variable "MINIM_TEST_UNSET_VARIABLE")

; %read-file: a file's forms, unevaluated.
(%read-file "load/defs.scm")
(%read-file "load/empty.scm")

; Errors.
(file-exists? 'files.scm)
(get-environment-variable 5)
(%read-file 5)

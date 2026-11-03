; The internals this file tests are in (minim system).
(import (minim system))

; Ports (R7RS 6.13): kinds and predicates.
(define in (open-input-string "abc"))
(define out (open-output-string))
(define file (open-input-file "load/defs.scm"))
(define stdin (%standard-port 0))
in
out
file
stdin
(%standard-port 1)
(%standard-port 2)
(map port? (list in out file stdin 1 "abc"))
(map input-port? (list in out file stdin 'x))
(map output-port? (list in out file stdin 'x))
(map textual-port? (list in out 1))
(map binary-port? (list in out 1))
(map input-port-open? (list in out file))
(map output-port-open? (list in out file))

; Closing, twice; a closed port stays a port.
(close-port in)
(close-port in)
(input-port-open? in)
(port? in)
(close-input-port file)
(input-port-open? file)
(close-output-port out)
(output-port-open? out)

; Closing a standard port leaves C's stream to it open.
(define err (%standard-port 2))
(close-port err)
(output-port-open? err)

; End of file.
(eof-object)
(eof-object? (eof-object))
(eof-object? '())

; Errors.
(open-input-file "load/no-such-file.scm")
(guard (e ((file-error? e) 'file-error)) (open-input-file "load/no-such-file.scm"))
(open-input-file 'defs)
(open-output-file 'defs)
(open-output-file "load/no-such-dir/x.scm")
(open-input-string 1)
(close-input-port (open-output-string))
(close-output-port (open-input-string ""))
(close-port 'x)
(input-port-open? 'x)
(%standard-port 3)

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

; Characters.
(define chars (open-input-string "aλ\nb\x0D;\nc\x0D;last"))
(peek-char chars)
(peek-char chars)
(read-char chars)
(read-char chars)
(read-line chars)
(read-line chars)
(read-line chars)
(char-ready? chars)
(read-line chars)
(read-line chars)
(read-char chars)
(peek-char chars)
(read-string 2 (open-input-string "xyz"))
(read-string 9 (open-input-string "xyz"))
(read-string 0 (open-input-string ""))
(read-string 1 (open-input-string ""))
(read-line (open-input-string ""))
(read-line (open-input-string "\n"))

; Writing to a string port, which grows as needed.
(define sink (open-output-string))
(write-char #\λ sink)
(write-string "abcdefghijklmnopqrstuvwxyz" sink)
(write-string "0123" sink 1)
(write-string "0123" sink 1 2)
(newline sink)
(get-output-string sink)
(get-output-string sink)
(flush-output-port sink)

; The current ports, which parameterize redirects.
(map (lambda (p) (p)) (list current-input-port current-output-port current-error-port))
(define captured (open-output-string))
(parameterize ((current-output-port captured))
  (write-char #\x)
  (newline)
  (write-string "y"))
(get-output-string captured)
(current-output-port)
(parameterize ((current-output-port (open-input-string ""))) 1)
(parameterize ((current-input-port 'x)) 1)

; A file's characters.
(define f (open-input-file "load/defs.scm"))
(read-line f)
(read-char f)
(read-string 6 f)
(close-port f)

; Reading data.  What follows a datum stays to be read.
(define data (open-input-string "(1 \"two\" #\\3) sym 4.5x\n  rest"))
(read data)
(read data)
(read-char data)
(read data)
(read-line data)
(read-line data)
(read data)
(read (open-input-string "   "))
(guard (e ((read-error? e) (error-object-message e))) (read (open-input-string "(1 2")))
(guard (e ((read-error? e) 'read-error)) (read (open-input-string ")")))
(parameterize ((current-input-port (open-input-string "(from current)")))
  (read))

; The REPL reads through the standard input port too, so these read what
; follows them on their own lines.
(read-line) the rest of this line
(read) (the next datum) 'and-this-one

; Printing to a port.
(define printed (open-output-string))
(write '(1 "two\n" #\3 #(sym (a . b)) λ) printed)
(write-char #\space printed)
(display '(1 "two" #\3 λ) printed)
(write-char #\space printed)
(write-simple "simple" printed)
(write-char #\space printed)
(write printed printed)
(get-output-string printed)
(parameterize ((current-output-port printed)) (display "!") (write 'x))
(get-output-string printed)

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
(read-char (open-output-string))
(write-char #\a (open-input-string ""))
(read-char f)
(write-char 1)
(read-string -1)
(write-string "abc" (current-output-port) 2 1)
(write-string "abc" (current-output-port) 0 4)
(read-char (current-input-port) 1)
(read (open-output-string))
(write 1 (open-input-string ""))
(define closed (open-output-string))
(close-port closed)
(display 1 closed)
(write 1 (current-output-port) 2)
(get-output-string (current-output-port))

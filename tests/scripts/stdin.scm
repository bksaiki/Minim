; Copies the standard input's lines to the standard output, numbered, and
; their count to the standard error.
(define (copy-lines n)
  (let ((line (read-line)))
    (cond ((eof-object? line)
           (write-string (number->string n) (current-error-port))
           (newline (current-error-port)))
          (else
           (write-string (number->string (+ n 1)))
           (write-string ": ")
           (write-string line)
           (newline)
           (copy-lines (+ n 1))))))

(copy-lines 0)

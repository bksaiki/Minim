; Writes a file, reads it back, and deletes it.  The stress copy, which may
; run at the same time, uses a file of its own.
(define path
  (if (get-environment-variable "MINIM_GC_NURSERY_BYTES") "file-ports-stress.tmp" "file-ports.tmp"))

(with-output-to-file path
  (lambda ()
    (write '(data "λ"))
    (newline)
    (display "second line")))

(write (call-with-input-file path read))
(newline)
(write (call-with-input-file path (lambda (p) (read-line p) (read-line p))))
(newline)
(write (with-input-from-file path read-line))
(newline)

; call-with-port closes the port once its procedure returns, with its values.
(define out (open-output-file path))
(call-with-values (lambda () (call-with-port out (lambda (p) (write-char #\x p) (values 1 2))))
  (lambda (a b) (write (list a b (output-port-open? out)))))
(newline)
(write (call-with-input-file path read-char))
(newline)

(delete-file path)
(write (file-exists? path))
(newline)
(guard (e ((file-error? e) (display "file error") (newline)))
  (delete-file path))

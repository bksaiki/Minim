;;; Binary file ports: bytes, some not UTF-8, written to the file named by the
;;; argument and read back.

(define path (cadr (command-line)))

(define out (open-binary-output-file path))
(display (list (binary-port? out) (textual-port? out) (output-port? out)))
(newline)
(write-u8 0 out)
(write-bytevector #u8(255 192 10 13 1 2 3) out 0 5)
(close-port out)

(define in (open-binary-input-file path))
(display (list (binary-port? in) (peek-u8 in) (read-u8 in) (read-bytevector 3 in)))
(newline)
(define rest (make-bytevector 4 9))
(display (list (read-bytevector! rest in) rest (read-u8 in) (u8-ready? in)))
(newline)
(display (guard (e (#t (error-object-message e))) (read-char in)))
(newline)
(close-port in)

;; call-with-port closes the port when its procedure returns.
(define p (open-binary-input-file path))
(display (list (call-with-port p (lambda (p) (read-bytevector 10 p))) (input-port-open? p)))
(newline)

(display (guard (e ((file-error? e) (error-object-message e)))
           (open-binary-input-file "no/such/file")))
(newline)
(delete-file path)
(display (file-exists? path))
(newline)

;;; Ports (R7RS 6.13): the current ports, and the procedures whose port
;;; argument defaults to one.

;; A parameter holding a current port, initially `port`, which only ports
;; satisfying `ok?` may replace; `msg` is the error otherwise.
(define (%current-port port ok? msg)
  (make-parameter port
                  (lambda (p)
                    (unless (and (port? p) (ok? p))
                      (error msg p))
                    p)))

(define current-input-port
  (%current-port (%standard-port 0) input-port? "current-input-port: not an input port"))

(define current-output-port
  (%current-port (%standard-port 1) output-port? "current-output-port: not an output port"))

(define current-error-port
  (%current-port (%standard-port 2) output-port? "current-error-port: not an output port"))

;; Defines `name` as `prim`, taking `arg ...` and an optional port,
;; `(current)` by default.
(define-syntax %define-with-port
  (syntax-rules ()
    ((_ (name arg ...) current prim)
     (define name
       (case-lambda
         ((arg ...) (prim arg ... (current)))
         ((arg ... port) (prim arg ... port)))))))

(%define-with-port (write x) current-output-port %write)

(%define-with-port (write-simple x) current-output-port %write)

(%define-with-port (display x) current-output-port %display)

(%define-with-port (write-char c) current-output-port %write-char)

(%define-with-port (flush-output-port) current-output-port %flush-output-port)

(%define-with-port (read) current-input-port %read)

(%define-with-port (read-char) current-input-port %read-char)

(%define-with-port (peek-char) current-input-port %peek-char)

(%define-with-port (read-line) current-input-port %read-line)

(%define-with-port (read-string k) current-input-port %read-string)

(%define-with-port (char-ready?) current-input-port %char-ready?)

(define newline
  (case-lambda
    (() (%write-char #\newline (current-output-port)))
    ((port) (%write-char #\newline port))))

;; Writes `s`, or its chars from `start` to `end`, to `port`.
(define write-string
  (case-lambda
    ((s) (%write-string s (current-output-port) 0 #f))
    ((s port) (%write-string s port 0 #f))
    ((s port start) (%write-string s port start #f))
    ((s port start end) (%write-string s port start end))))

;; If `proc` escapes, the port stays open (R7RS).
(define (call-with-port port proc)
  (call-with-values (lambda () (proc port))
    (lambda vals
      (close-port port)
      (apply values vals))))

(define (call-with-input-file path proc)
  (call-with-port (open-input-file path) proc))

(define (call-with-output-file path proc)
  (call-with-port (open-output-file path) proc))

(define (with-input-from-file path thunk)
  (call-with-input-file path
    (lambda (port)
      (parameterize ((current-input-port port))
        (thunk)))))

(define (with-output-to-file path thunk)
  (call-with-output-file path
    (lambda (port)
      (parameterize ((current-output-port port))
        (thunk)))))

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

;; The port in the optional arguments `args`, or else the current one,
;; `current`; `msg` is the error for more than one.
(define (%port-arg args current msg)
  (cond ((null? args) (current))
        ((null? (cdr args)) (car args))
        (else (error msg args))))

(define (write x . port)
  (%write x (%port-arg port current-output-port "write: too many arguments")))

(define (write-simple x . port)
  (%write x (%port-arg port current-output-port "write-simple: too many arguments")))

(define (display x . port)
  (%display x (%port-arg port current-output-port "display: too many arguments")))

(define (read . port)
  (%read (%port-arg port current-input-port "read: too many arguments")))

(define (read-char . port)
  (%read-char (%port-arg port current-input-port "read-char: too many arguments")))

(define (peek-char . port)
  (%peek-char (%port-arg port current-input-port "peek-char: too many arguments")))

(define (read-line . port)
  (%read-line (%port-arg port current-input-port "read-line: too many arguments")))

(define (read-string k . port)
  (%read-string k (%port-arg port current-input-port "read-string: too many arguments")))

(define (char-ready? . port)
  (%char-ready? (%port-arg port current-input-port "char-ready?: too many arguments")))

(define (write-char c . port)
  (%write-char c (%port-arg port current-output-port "write-char: too many arguments")))

(define (newline . port)
  (%write-char #\newline (%port-arg port current-output-port "newline: too many arguments")))

(define (flush-output-port . port)
  (%flush-output-port
   (%port-arg port current-output-port "flush-output-port: too many arguments")))

;; Writes `s`, or its chars from `start` to `end`, to `port`.
(define (write-string s . args)
  (let ((port (if (pair? args) (car args) (current-output-port)))
        (bounds (if (pair? args) (cdr args) '())))
    (cond ((null? bounds) (%write-string s port 0 #f))
          ((null? (cdr bounds)) (%write-string s port (car bounds) #f))
          ((null? (cddr bounds)) (%write-string s port (car bounds) (cadr bounds)))
          (else (error "write-string: too many arguments" args)))))

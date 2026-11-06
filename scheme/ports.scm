;;; Ports (R7RS 6.13): the current ports, the procedures whose port
;;; argument defaults to one, and those over reading and writing a char or
;;; byte at a time.

;; Raises `msg` with `p` unless it is a port open for input if `input?`, else
;; for output, and binary if `binary?`, else textual.
(define (%check-open p input? binary? msg)
  (unless (and (if input? (input-port? p) (output-port? p))
               (if input? (input-port-open? p) (output-port-open? p))
               (eq? (binary-port? p) binary?))
    (error msg p)))

;; Raises `msg` with `k` unless it is a non-negative fixnum.
(define (%check-count k msg)
  (unless (and (fixnum? k) (>= k 0))
    (error msg k)))

(define (close-input-port p)
  (%check input-port? p "close-input-port: not an input port")
  (close-port p))

(define (close-output-port p)
  (%check output-port? p "close-output-port: not an output port")
  (close-port p))

;; The next line from `port`, without its \n, \r, or \r\n; the eof object at
;; the end.
(define (%read-line port)
  (%check-open port #t #f "read-line: not an open textual input port")
  (let ((line (open-output-string)))
    (let loop ((c (%read-char port))
               (n 0))
      (cond ((eof-object? c)
             (if (= n 0) c (get-output-string line)))
            ((or (char=? c #\newline) (char=? c #\return))
             (when (and (char=? c #\return) (eqv? (%peek-char port) #\newline))
               (%read-char port))
             (get-output-string line))
            (else
             (%write-char c line)
             (loop (%read-char port) (+ n 1)))))))

;; Up to `k` chars from `port`, or the eof object if there are none before
;; the end.
(define (%read-string k port)
  (%check-count k "read-string: not a non-negative fixnum")
  (%check-open port #t #f "read-string: not an open textual input port")
  (let ((chars (open-output-string)))
    (let loop ((n 0))
      (let ((c (if (< n k) (%read-char port) #f)))
        (cond ((not c) (get-output-string chars))
              ((eof-object? c) (if (= n 0) c (get-output-string chars)))
              (else
               (%write-char c chars)
               (loop (+ n 1))))))))

;; Writes the chars of `s` from `start` to `end`, #f for its end, to `port`.
(define (%write-string s port start end)
  (%check string? s "write-string: not a string")
  (%check-open port #f #f "write-string: not an open textual output port")
  (let ((end (if end
                 (begin
                   (%check-position end (string-length s) "write-string: bad end")
                   end)
                 (string-length s))))
    (%check-position start end "write-string: bad start")
    (do ((i start (+ i 1)))
        ((= i end))
      (%write-char (string-ref s i) port))))

;; Up to `k` bytes from `port`, or the eof object if there are none before
;; the end.
(define (%read-bytevector k port)
  (%check-count k "read-bytevector: not a non-negative fixnum")
  (%check-open port #t #t "read-bytevector: not an open binary input port")
  (let ((bytes (open-output-bytevector)))
    (let loop ((n 0))
      (let ((b (if (< n k) (%read-u8 port) #f)))
        (cond ((not b) (get-output-bytevector bytes))
              ((eof-object? b) (if (= n 0) b (get-output-bytevector bytes)))
              (else
               (%write-u8 b bytes)
               (loop (+ n 1))))))))

;; Reads into `bv`, from `start` to `end`, from `port`; returns how many
;; bytes, or the eof object if there are none before the end.
(define %read-bytevector!
  (case-lambda
    ((bv port) (%read-bytevector! bv port 0))
    ((bv port start)
     (%read-bytevector! bv port start (%length-or-0 bv bytevector? bytevector-length)))
    ((bv port start end)
     (%check bytevector? bv "read-bytevector!: not a bytevector")
     (%check-open port #t #t "read-bytevector!: not an open binary input port")
     (%check-range start end (bytevector-length bv) "read-bytevector!: bad index")
     (let loop ((i start))
       (let ((b (if (< i end) (%read-u8 port) #f)))
         (cond ((not b) (- i start))
               ((eof-object? b) (if (= i start) b (- i start)))
               (else
                (bytevector-u8-set! bv i b)
                (loop (+ i 1)))))))))

;; Writes the bytes of `bv` from `start` to `end` to `port`.
(define %write-bytevector
  (case-lambda
    ((bv port) (%write-bytevector bv port 0))
    ((bv port start)
     (%write-bytevector bv port start (%length-or-0 bv bytevector? bytevector-length)))
    ((bv port start end)
     (%check bytevector? bv "write-bytevector: not a bytevector")
     (%check-open port #f #t "write-bytevector: not an open binary output port")
     (%check-range start end (bytevector-length bv) "write-bytevector: bad index")
     (do ((i start (+ i 1)))
         ((= i end))
       (%write-u8 (bytevector-u8-ref bv i) port)))))

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

(%define-with-port (read-u8) current-input-port %read-u8)

(%define-with-port (peek-u8) current-input-port %peek-u8)

(%define-with-port (u8-ready?) current-input-port %u8-ready?)

(%define-with-port (read-bytevector k) current-input-port %read-bytevector)

(%define-with-port (write-u8 byte) current-output-port %write-u8)

;; Reads into `bv`, or its bytes from `start` to `end`, from `port`.
(define read-bytevector!
  (case-lambda
    ((bv) (%read-bytevector! bv (current-input-port)))
    ((bv port) (%read-bytevector! bv port))
    ((bv port start) (%read-bytevector! bv port start))
    ((bv port start end) (%read-bytevector! bv port start end))))

;; Writes `bv`, or its bytes from `start` to `end`, to `port`.
(define write-bytevector
  (case-lambda
    ((bv) (%write-bytevector bv (current-output-port)))
    ((bv port) (%write-bytevector bv port))
    ((bv port start) (%write-bytevector bv port start))
    ((bv port start end) (%write-bytevector bv port start end))))

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

;;; Runs a section of Chibi's R7RS test suite, r7rs-tests.scm, named by its
;;; title: `minim run.scm "6.4 Lists"`.  Prints each test that fails, each
;;; other error, and each skipped form, by the line its form starts at, then
;;; the counts.  The runner's names start with `runner-`, out of the tests'
;;; way.

;; Counts and reports.

(define runner-passed 0)

(define runner-failed 0)

(define runner-errors 0)

(define runner-line 0)  ; where the form being evaluated starts

;; The message and irritants of the condition `e`, or `e` itself.
(define (runner-condition e)
  (if (error-object? e)
      (cons (error-object-message e) (error-object-irritants e))
      e))

;; Prints `what`, the line of the current form, and the list `details`.
(define (runner-report what details)
  (display what)
  (display " ")
  (display runner-line)
  (for-each (lambda (d) (display " ") (write d)) details)
  (newline))

;; The subset of (chibi test) the suite uses.

;; Whether `v` passes for `want`, as Chibi's `test-equal?`: `equal?`, or, if
;; `want` is an inexact real, a real within a relative 1e-5 of it, as the
;; suite writes such values to about 15 digits.  (Minim has no complex numbers,
;; which Chibi compares part by part.)
(define (runner-equal? want v)
  (or (equal? want v)
      (and (real? want) (inexact? want) (real? v) (runner-approx? want v 1e-5))))

(define (runner-approx? a b epsilon)
  (cond ((> (abs a) (abs b)) (runner-approx? b a epsilon))
        ((zero? a) (< (abs b) epsilon))
        (else (< (abs (/ (- a b) b)) epsilon))))

;; Counts a test of `expr` passing if `(ok? value)`, its value from `thunk`.
(define (runner-check expr ok? thunk)
  (guard (e (#t (set! runner-failed (+ runner-failed 1))
                (runner-report "FAIL" (list expr 'raised (runner-condition e)))))
    (let ((value (thunk)))
      (if (ok? value)
          (set! runner-passed (+ runner-passed 1))
          (begin
            (set! runner-failed (+ runner-failed 1))
            (runner-report "FAIL" (list expr '=> value)))))))

(define-syntax test
  (syntax-rules ()
    ((_ name expected expr) (test expected expr))
    ((_ expected expr)
     (let ((want expected))
       (runner-check 'expr (lambda (v) (runner-equal? want v)) (lambda () expr))))))

(define-syntax test-assert
  (syntax-rules ()
    ((_ name expr) (test-assert expr))
    ((_ expr) (runner-check 'expr (lambda (v) v) (lambda () expr)))))

(define-syntax test-values
  (syntax-rules ()
    ((_ name expected expr) (test-values expected expr))
    ((_ expected expr)
     (let ((want (call-with-values (lambda () expected) list)))
       (runner-check 'expr
                     (lambda (v) (equal? v want))
                     (lambda () (call-with-values (lambda () expr) list)))))))

;; Passes if `expr` raises.
(define-syntax test-error
  (syntax-rules ()
    ((_ name expr) (test-error expr))
    ((_ expr)
     (runner-check 'expr
                   (lambda (raised?) raised?)
                   (lambda () (guard (e (#t #t)) expr #f))))))

(define (test-begin . name)
  #f)

(define (test-end . name)
  #f)

;; Reading the suite.

;; The lines of the file `path`, as a list.
(define (runner-lines path)
  (call-with-input-file path
    (lambda (port)
      (let loop ((lines '()))
        (let ((line (read-line port)))
          (if (eof-object? line)
              (reverse lines)
              (loop (cons line lines))))))))

;; Whether the string `s` starts with `prefix`.
(define (runner-prefix? prefix s)
  (and (<= (string-length prefix) (string-length s))
       (string=? prefix (substring s 0 (string-length prefix)))))

;; The (number . line) pairs of the section titled `title`, between its
;; `test-begin` and matching `test-end`, without its nested sections.
(define (runner-section lines title)
  (let find ((lines lines) (n 1))
    (cond ((null? lines) (error "no such section" title))
          ((string=? (car lines) (string-append "(test-begin \"" title "\")"))
           (let collect ((lines (cdr lines)) (n (+ n 1)) (depth 0) (acc '()))
             (let ((line (car lines)))
               (cond ((runner-prefix? "(test-begin " line)
                      (collect (cdr lines) (+ n 1) (+ depth 1) acc))
                     ((runner-prefix? "(test-end" line)
                      (if (= depth 0)
                          (reverse acc)
                          (collect (cdr lines) (+ n 1) (- depth 1) acc)))
                     ((= depth 0) (collect (cdr lines) (+ n 1) depth (cons (cons n line) acc)))
                     (else (collect (cdr lines) (+ n 1) depth acc))))))
          (else (find (cdr lines) (+ n 1))))))

;; Whether `text` reads without ending inside a list, vector, or block
;; comment.  Another read error, even inside a string after syntax Minim
;; lacks, counts as complete, to be reported when it runs.
(define (runner-complete? text)
  (let ((port (open-input-string text)))
    (guard (e ((error-object? e)
               (not (member (error-object-message e)
                            '("eof in list" "eof in vector" "eof in comment")))))
      (let loop ()
        (unless (eof-object? (read port))
          (loop)))
      #t)))

;; The section's top-level forms as (number . text) pairs: a form starts at a
;; line starting with `(`, and runs to the next, unless it would end inside a
;; datum: then it takes the next one too.
(define (runner-chunks numbered)
  (let loop ((numbered numbered) (chunks '()))
    (cond ((null? numbered) (reverse chunks))
          ((and (runner-prefix? "(" (cdar numbered))
                (or (null? chunks) (runner-complete? (cdar chunks))))
           (loop (cdr numbered) (cons (cons (caar numbered) (cdar numbered)) chunks)))
          ((null? chunks) (loop (cdr numbered) chunks))
          (else
           (let ((chunk (car chunks)))
             (loop (cdr numbered)
                   (cons (cons (car chunk) (string-append (cdr chunk) "\n" (cdar numbered)))
                         (cdr chunks))))))))

;; Forms that would hang Minim, by the line they start at, with why.
(define runner-skips
  '())

;; Reads and evaluates the data of `text`; a read error ends it.
(define (runner-run-chunk text)
  (let ((port (open-input-string text)))
    (let loop ()
      (let ((datum (guard (e (#t (set! runner-errors (+ runner-errors 1))
                                 (runner-report "READ-ERROR" (list (runner-condition e)))
                                 (eof-object)))
                     (read port))))
        (unless (eof-object? datum)
          (guard (e (#t (set! runner-errors (+ runner-errors 1))
                        (runner-report "ERROR" (list (runner-condition e)))))
            (eval datum (interaction-environment)))
          (loop))))))

(define (runner-main title)
  (for-each (lambda (chunk)
              (set! runner-line (car chunk))
              (let ((skip (assv runner-line runner-skips)))
                (if skip
                    (runner-report "SKIP" (list (cdr skip)))
                    (runner-run-chunk (cdr chunk)))))
            (runner-chunks (runner-section (runner-lines "r7rs-tests.scm") title)))
  (display title)
  (display ": ")
  (display runner-passed)
  (display " passed, ")
  (display runner-failed)
  (display " failed, ")
  (display runner-errors)
  (display " errors")
  (newline))

(runner-main (cadr (command-line)))

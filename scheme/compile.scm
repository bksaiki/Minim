;;; The compiler: the core forms, the expander's output, to the simplified
;;; core, which the evaluator runs unchecked, in passes.
;;;
;;; `%parse` checks the core and normalizes it to `parsed`:
;;;
;;;   e ::= x | (quote d) | (if e e e) | (begin e e e ...) | (set! x e)
;;;       | (define x e) | (define-values formals e)
;;;       | (let-values ((formals e) ...) e) | (letrec-values ((formals e) ...) e)
;;;       | (lambda (formals e) ...) | (call e e ...)
;;;
;;; `%emit` makes each `lambda` a `clambda`, each clause `(formals required
;;; rest? e)`, its arity computed once: the simplified core.

;; Whether `formals` is a lambda list: symbols, perhaps with a rest symbol.
(define (%formals? formals)
  (if (pair? formals)
      (and (symbol? (car formals)) (%formals? (cdr formals)))
      (or (null? formals) (symbol? formals))))

;; The number of `formals` before any rest parameter.
(define (%required formals)
  (if (pair? formals)
      (+ 1 (%required (cdr formals)))
      0))

;; Whether `formals` has a rest parameter.
(define (%rest? formals)
  (if (pair? formals)
      (%rest? (cdr formals))
      (symbol? formals)))

;; Whether `ok?` holds of every element of the proper list `xs`.
(define (%all? ok? xs)
  (and (%length xs 0)
       (let loop ((xs xs))
         (or (null? xs) (and (ok? (car xs)) (loop (cdr xs)))))))

;; Whether `c` is a clause `(formals e ...)`, as `lambda` and `case-lambda` take.
(define (%lambda-clause? c)
  (let ((n (%length c 0)))
    (and n (>= n 2) (%formals? (car c)))))

;; Whether `c` is a clause `(formals e)`, as the `-values` forms take.
(define (%values-clause? c)
  (and (eqv? (%length c 0) 2) (%formals? (car c))))

(define %void-form (list 'quote (void)))

;; The parsed form of the core form `x`; definitions may be in it only if
;; `top?`, at top level or in a top-level `begin`.  Raises `bad syntax` with
;; the malformed form, as the evaluator does.
(define (%parse x top?)
  (define (sub y)
    (%parse y #f))
  (define (body forms)
    (if (null? (cdr forms))
        (sub (car forms))
        (cons 'begin (map sub forms))))
  (cond ((symbol? x) x)
        ((null? x) (%bad-syntax x))
        ((not (pair? x)) (list 'quote x))
        (else
         (let ((n (%length x 0))
               (head (car x)))
           (cond ((not n) (%bad-syntax x))
                 ((eq? head 'quote) (if (= n 2) x (%bad-syntax x)))
                 ((eq? head 'if)
                  (if (or (= n 3) (= n 4))
                      (list 'if (sub (cadr x)) (sub (caddr x))
                            (if (= n 4) (sub (cadddr x)) %void-form))
                      (%bad-syntax x)))
                 ((or (eq? head 'set!) (eq? head 'define))
                  (if (and (= n 3) (symbol? (cadr x)) (or top? (eq? head 'set!)))
                      (list head (cadr x) (sub (caddr x)))
                      (%bad-syntax x)))
                 ((eq? head 'define-values)
                  (if (and (= n 3) (%formals? (cadr x)) top?)
                      (list head (cadr x) (sub (caddr x)))
                      (%bad-syntax x)))
                 ((or (eq? head 'let-values) (eq? head 'letrec-values))
                  (if (and (>= n 3) (%all? %values-clause? (cadr x)))
                      (list head
                            (map (lambda (c) (list (car c) (sub (cadr c)))) (cadr x))
                            (body (cddr x)))
                      (%bad-syntax x)))
                 ((eq? head 'lambda)
                  (if (%lambda-clause? (cdr x))
                      (list 'lambda (list (cadr x) (body (cddr x))))
                      (%bad-syntax x)))
                 ((eq? head 'case-lambda)
                  (if (%all? %lambda-clause? (cdr x))
                      (cons 'lambda (map (lambda (c) (list (car c) (body (cdr c)))) (cdr x)))
                      (%bad-syntax x)))
                 ((eq? head 'begin)
                  (cond ((= n 1) %void-form)
                        ((= n 2) (%parse (cadr x) top?))
                        (else (cons 'begin (map (lambda (y) (%parse y top?)) (cdr x))))))
                 (else (cons 'call (map sub x))))))))

;; The simplified form of the parsed form `x`.
(define (%emit x)
  (if (symbol? x)
      x
      (case (car x)
        ((quote) x)
        ((lambda)
         (cons 'clambda
               (map (lambda (c)
                      (list (car c) (%required (car c)) (%rest? (car c)) (%emit (cadr c))))
                    (cdr x))))
        ((set! define define-values) (list (car x) (cadr x) (%emit (caddr x))))
        ((let-values letrec-values)
         (list (car x)
               (map (lambda (c) (list (car c) (%emit (cadr c)))) (cadr x))
               (%emit (caddr x))))
        (else (cons (car x) (map %emit (cdr x)))))))  ; if, begin, call

;; The simplified form of the top-level core form `x`.
(define (%compile-form x)
  (%emit (%parse x #t)))

;; Whether `x` is in `parsed`, or, if `simplified?`, in the simplified core.
(define (%in-language? x simplified?)
  (define (e? y)
    (%in-language? y simplified?))
  (define (bindings? cs)
    (%all? (lambda (c) (and (eqv? (%length c 0) 2) (%formals? (car c)) (e? (cadr c)))) cs))
  (define (clause? c)
    (if simplified?
        (and (eqv? (%length c 0) 4)
             (%formals? (car c))
             (eqv? (cadr c) (%required (car c)))
             (eq? (caddr c) (%rest? (car c)))
             (e? (cadddr c)))
        (and (eqv? (%length c 0) 2) (%formals? (car c)) (e? (cadr c)))))
  (let ((n (and (pair? x) (%length x 0))))
    (cond ((symbol? x) #t)
          ((not n) #f)
          (else
           (case (car x)
             ((quote) (= n 2))
             ((if) (and (= n 4) (%all? e? (cdr x))))
             ((begin) (and (>= n 3) (%all? e? (cdr x))))
             ((set! define) (and (= n 3) (symbol? (cadr x)) (e? (caddr x))))
             ((define-values) (and (= n 3) (%formals? (cadr x)) (e? (caddr x))))
             ((let-values letrec-values) (and (= n 3) (bindings? (cadr x)) (e? (caddr x))))
             ((lambda) (and (not simplified?) (%all? clause? (cdr x))))
             ((clambda) (and simplified? (%all? clause? (cdr x))))
             ((call) (and (>= n 2) (%all? e? (cdr x))))
             (else #f))))))

(define (%parsed? x)
  (%in-language? x #f))

(define (%simplified? x)
  (%in-language? x #t))

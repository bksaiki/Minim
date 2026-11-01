; Error objects: the predicates and accessors.
(define e (%make-error-object "bad thing" '(1 (2) "three")))
(error-object? e)
(error-object-message e)
(error-object-irritants e)
(read-error? e)
(file-error? e)
e
(error-object-irritants (%make-error-object "none" '()))

; Not error objects.
(map error-object? (list 'x "bad" 5 '() (vector 1) car))
(map read-error? (list 'x "bad" 5))
(map file-error? (list 'x "bad" 5))

; Compared by identity.
(eq? e e)
(equal? e (%make-error-object "bad thing" '(1 (2) "three")))
(= (equal-hash e) (equal-hash e))

; They survive collections, contents and all.
(define es (map (lambda (i) (%make-error-object (number->string i) (list i))) '(1 2 3)))
(collect)
(map error-object-message es)
(map error-object-irritants es)
(collect)
(error-object-message e)
(error-object-irritants e)

; Bad arguments.
(error-object-message "bad")
(error-object-irritants 5)
(%make-error-object 'bad '())
(%make-error-object "bad" 5)

; raise-continuable returns the handler's values.
(with-exception-handler
 (lambda (x) (* x 10))
 (lambda () (+ 1 (raise-continuable 4))))
(with-exception-handler
 (lambda (x) (values x x))
 (lambda () (call-with-values (lambda () (raise-continuable 'v)) list)))

; A handler escaping with call/cc, from raise and from error.
(call/cc
 (lambda (k)
   (with-exception-handler
    (lambda (x) (k (list 'caught x)))
    (lambda () (raise 'boom)))))
(define (catch-error thunk)
  (call/cc
   (lambda (k)
     (with-exception-handler
      (lambda (e) (k (list (error-object-message e) (error-object-irritants e))))
      thunk))))
(catch-error (lambda () (error "bad thing" 1 '(2))))
(catch-error (lambda () (error "no irritants")))

; Nested handlers: the inner one runs first, with the outer installed.
(with-exception-handler
 (lambda (x) (list 'outer x))
 (lambda ()
   (with-exception-handler
    (lambda (x) (raise-continuable (list 'inner x)))
    (lambda () (raise-continuable 'x)))))

; The handlers are restored after the thunk returns or escapes.
(with-exception-handler
 (lambda (x) 'outer)
 (lambda ()
   (call/cc (lambda (k) (with-exception-handler (lambda (x) (k 'escaped)) (lambda () (raise 1)))))
   (raise-continuable 2)))
(%handlers)

; A dynamic-wind left by an escaping handler runs its after.
(define log '())
(call/cc
 (lambda (k)
   (with-exception-handler
    (lambda (x) (k x))
    (lambda ()
      (dynamic-wind
       (lambda () (set! log (cons 'before log)))
       (lambda () (raise 'out))
       (lambda () (set! log (cons 'after log))))))))
(reverse log)

; A handler returning from raise is an error, raised to the outer handler.
(catch-error
 (lambda ()
   (with-exception-handler (lambda (x) 'ignored) (lambda () (raise 'r)))))

; Uncaught: an error object, another object, and a handler returning.
(error "bad thing" 1 2)
(error "one irritant" '(1))
(error "no irritants")
(raise 'boom)
(raise (list 1 2))
(with-exception-handler (lambda (x) 'ignored) (lambda () (raise 'r)))
(error 'not-a-string)
(%handlers)

; C errors raise error objects to handlers.
(define (catch thunk)
  (call/cc (lambda (k) (with-exception-handler (lambda (e) (k e)) thunk))))
(define (describe e)
  (list (error-object-message e) (error-object-irritants e) (read-error? e) (file-error? e)))
(describe (catch (lambda () (car 5))))
(describe (catch (lambda () never-bound)))
(describe (catch (lambda () ((lambda (x) x)))))
(describe (catch (lambda () (apply + 1 2))))
(describe (catch (lambda () (vector-ref (vector) 0))))
(describe (catch (lambda () (load "load/bad-read.scm"))))
(describe (catch (lambda () (load "load/missing.scm"))))
(describe (catch (lambda () (+ 1 (values 1 2)))))

; An error in a handler goes to the next handler out; with none, to the REPL.
(describe
 (catch (lambda () (with-exception-handler (lambda (e) (car e)) (lambda () (raise 'r))))))
(with-exception-handler (lambda (e) (car e)) (lambda () (raise 'r)))

; A handler returning from a C error.
(with-exception-handler (lambda (e) 'ignored) (lambda () (car 5)))

; An error a handler escapes from runs the afters it leaves; an unhandled one
; doesn't.
(set! log '())
(define (logged thunk)
  (dynamic-wind (lambda () (set! log (cons 'in log))) thunk (lambda () (set! log (cons 'out log)))))
(error-object-message (catch (lambda () (logged (lambda () (car 1))))))
(reverse log)
(set! log '())
(logged (lambda () (car 1)))
(reverse log)
(%winders)
(%handlers)

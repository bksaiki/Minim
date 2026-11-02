(define log '())
(define (logged thunk)
  (dynamic-wind (lambda () (set! log (cons 'in log))) thunk (lambda () (set! log (cons 'out log)))))

; guard: clauses with else and =>, and with a test alone.
(guard (e ((symbol? e) (list 'symbol e)) ((string? e) 'string)) (raise 'boom))
(guard (e ((symbol? e) 'symbol) (else (list 'else e))) (raise 42))
(guard (e ((assq 'a e) => cdr) ((assq 'b e))) (raise (list (cons 'a 42))))
(guard (e ((assq 'a e) => cdr) ((assq 'b e))) (raise (list (cons 'b 23))))
(guard (e (#t (error-object-message e))) (car 5))
(guard (e ((error-object? e) (error-object-irritants e))) (error "bad" 1 2))
(guard (e ((string? e) e)) 'no-raise)
(guard (e) (define x 1) (+ x 1))

; No clause matching re-raises to the outer guard, or to raise-continuable's
; handler, whose value the guard's body continues with.
(guard (e ((symbol? e) (list 'outer e)))
  (guard (e ((string? e) (list 'inner e)))
    (raise 'sym)))
(with-exception-handler
 (lambda (e) 10)
 (lambda () (+ 1 (guard (e ((string? e) 'string)) (+ 1 (raise-continuable 'c))))))
(guard (e ((string? e) 'string)) (raise 'unmatched))

; The afters of the dynamic-winds left run, before the clauses.
(set! log '())
(guard (e (#t (set! log (cons 'clause log))))
  (logged (lambda () (raise 'x))))
(reverse log)

; The body's values, any number.
(call-with-values (lambda () (guard (e (#t 0)) (values 1 2 3))) list)
(call-with-values (lambda () (guard (e (#t (values 'a 'b))) (raise 0))) list)

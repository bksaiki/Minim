; The simplified core, run by %eval-compiled: hand-written, before any compiler.
(import (minim system))

; Each form.  A global is its symbol; every constant is quoted.
(%eval-compiled '(quote (1 2)))
(%eval-compiled '(if (quote #f) (quote no) (quote yes)))
(%eval-compiled '(begin (call + (quote 1) (quote 2)) (call * (quote 3) (quote 4))))
(%eval-compiled
 '(let-values (((a b) (call values (quote 1) (quote 2))) ((c . d) (call values (quote 3))))
    (begin (set! a (call + a (quote 10))) (call list a b c d))))
(%eval-compiled
 '(letrec-values (((ev?) (clambda ((n) 1 #f (if (call = n (quote 0))
                                                 (quote #t)
                                                 (call od? (call - n (quote 1)))))))
                  ((od?) (clambda ((n) 1 #f (if (call = n (quote 0))
                                                 (quote #f)
                                                 (call ev? (call - n (quote 1))))))))
    (call ev? (quote 100))))
(%eval-compiled '(begin (define compiled-global (quote 5)) (set! compiled-global (quote 6))))
(%eval-compiled '(begin (define-values (cg-a . cg-b) (call values (quote 1) (quote 2)))
                        (call list cg-a cg-b compiled-global)))

; clambda: clauses chosen by required count and rest.
(define f
  (%eval-compiled
   '(clambda ((x) 1 #f x) ((x y . z) 2 #t (call list x y z)) (args 0 #t (call list (quote all) args)))))
(list (f 1) (f 1 2) (f 1 2 3) (f))
(car (%closure-code f))
f
((procedure-rename (%eval-compiled '(clambda ((x) 1 #f (call + x (quote 1))))) 'inc) 41)
(procedure-rename f 'renamed)

; Tail calls: a loop of ten thousand iterations.
(%eval-compiled
 '(letrec-values (((loop) (clambda ((i acc) 2 #f (if (call = i (quote 0))
                                                     acc
                                                     (call loop (call - i (quote 1))
                                                                (call + acc (quote 2))))))))
    (call loop (quote 10000) (quote 0))))

; Continuations out of, and back into, compiled code.
(%eval-compiled '(call + (quote 1) (call call/cc (clambda ((k) 1 #f (call k (quote 41)))))))
(%eval-compiled
 '(let-values (((n) (quote 0)) ((k) (quote #f)))
    (let-values (((v) (call call/cc (clambda ((c) 1 #f (begin (set! k c) (quote 0)))))))
      (begin (set! n (call + n (quote 1)))
             (if (call < v (quote 3)) (call k (call + v (quote 1))) (call list v n))))))

; Compiled and unsimplified closures call each other; values cross both ways.
(map (%eval-compiled '(clambda ((x) 1 #f (call * x x)))) '(1 2 3))
(%eval-compiled (list 'call (list 'quote (lambda (x) (* x 10))) ''4))
(%eval-compiled '(call call-with-values (clambda (() 0 #f (call values (quote 1) (quote 2)))) list))
(call-with-values (%eval-compiled '(clambda (() 0 #f (call values (quote 3) (quote 4))))) list)
(apply (%eval-compiled '(clambda (args 0 #t (call length args)))) (make-list 10000 0))

; Errors.
(%eval-compiled '(letrec-values (((a) b) ((b) (quote 1))) a))
((%eval-compiled '(clambda ((x) 1 #f x))) 1 2)
(%eval-compiled '(call (quote 5)))
(%eval-compiled '(lambda (x) x))

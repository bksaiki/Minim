; What forms expand to.  Fresh names print as the names they replace.

; Bodies, with and without definitions.
(%expand '(lambda (a) (+ a 1)))
(%expand '(lambda (a) (define x 1) (+ a x)))
(%expand '(lambda () (define-values (x y) (values 1 2)) (define z 3) (+ x y z)))
(%expand '(lambda () (define-syntax m (syntax-rules () ((_) 1))) (m)))
(%expand '(lambda () (define f (lambda () (define g 1) g)) (f)))
(%expand '(lambda () (define x 1)))

; Binding forms.
(%expand '(let ((y 1)) y))
(%expand '(let () 1))
(%expand '(let* ((a 1) (b a)) b))
(%expand '(letrec ((f (lambda () 1))) (f)))
(%expand '(letrec* ((a 1) (b a)) b))
(%expand '(let loop ((i 0)) (loop i)))
(%expand '(do ((i 0 (+ i 1))) ((= i 3) i)))
(%expand '(let-values (((a b) (values 1 2))) a))
(%expand '(let*-values (((a) 1) ((b) a)) b))
(%expand '(let-syntax ((m (syntax-rules () ((_) 1)))) (m)))
(%expand '(letrec-syntax ((m (syntax-rules () ((_) 1)))) (m)))

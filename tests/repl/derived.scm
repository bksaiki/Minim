; let, named let, let*, letrec, and letrec*.
(let ((x 1) (y 2)) (+ x y))
(let () 5)
(let loop ((i 0) (acc '()))
  (if (= i 3) acc (loop (+ i 1) (cons i acc))))
(let* ((a 1) (b (+ a 1)) (c (* b 2))) (list a b c))
(let* () 'empty)
(letrec ((even? (lambda (n) (if (= n 0) #t (odd? (- n 1)))))
         (odd? (lambda (n) (if (= n 0) #f (even? (- n 1))))))
  (list (even? 10) (odd? 7)))
(letrec* ((a 1) (b (+ a 1))) (list a b))
(let ((x 'outer)) (let ((x 'inner) (y x)) (list x y)))

; and and or return the deciding value.
(list (and) (and 1) (and 1 2 3) (and 1 #f 3))
(list (or) (or #f) (or #f 2 3) (or #f #f))

; when and unless.
(when (= 1 1) 'a 'b)
(when (= 1 2) 'a)
(unless (= 1 2) 'c)
(unless (= 1 1) 'c)

; cond with else and =>.
(define classify
  (lambda (n)
    (cond ((< n 0) 'negative)
          ((= n 0) 'zero)
          (else 'positive))))
(map classify '(-1 0 1))
(cond ((assv 2 '((1 . a) (2 . b))) => cdr) (else 'none))
(cond ((assv 3 '((1 . a))) => cdr) (else 'none))
(cond (#f 1) ((+ 1 1)))
(cond (#f 1))

; case with lists of data, =>, and else.
(define kind
  (lambda (x)
    (case x
      ((1 2 3) 'small)
      ((a b) 'letter)
      ((10) => (lambda (n) (* n 2)))
      (else => (lambda (v) (list 'other v))))))
(map kind '(2 a 10 z))
(case (* 2 3) ((2 3 5 7) 'prime) ((1 4 6 8 9) 'composite))
(case 'x ((y) 1))

; do.
(do ((i 0 (+ i 1)) (acc '() (cons i acc))) ((= i 4) acc))
(do ((vec (make-vector 3)) (i 0 (+ i 1))) ((= i 3) vec) (vector-set! vec i (* i i)))

; Temporaries don't capture user variables.
(let ((x 5)) (or #f x))
(let ((temp 'user)) (cond ((assv 2 '((2 . b))) => (lambda (p) (list p temp)))))
(let ((atom-key 'user)) (case (car '(1)) ((1) atom-key)))
(let ((loop 'user)) (do ((i 0 (+ i 1))) ((= i 2) loop)))

; Errors.
(let ((x)) x)
(cond)

;; Global references: procedures and variables defined at top level, read in
;; a loop.

(define step 1)
(define limit 500000)

(define (inc x)
  (+ x step))

(define (run i acc)
  (if (= i limit)
      acc
      (run (inc i) (+ acc (car (cons step '()))))))

(run 0 0)

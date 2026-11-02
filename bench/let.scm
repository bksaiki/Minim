;; Local frames: `let`, `let*`, and named `let`, and a body with internal
;; definitions (`letrec-values`).

(define (sum-squares n)
  (let loop ((i 0) (acc 0))
    (if (= i n)
        acc
        (let* ((sq (* i i))
               (acc2 (+ acc sq)))
          (loop (+ i 1) acc2)))))

(define (count-evens n)
  (define (even? k) (if (= k 0) #t (odd? (- k 1))))
  (define (odd? k) (if (= k 0) #f (even? (- k 1))))
  (let loop ((i 0) (count 0))
    (if (= i n)
        count
        (loop (+ i 1) (if (even? (remainder i 10)) (+ count 1) count)))))

(sum-squares 200000)
(count-evens 100000)

;; Printing: a long list and a big tree written to a string port.

(define (iota n)
  (let loop ((i (- n 1)) (acc '()))
    (if (< i 0) acc (loop (- i 1) (cons i acc)))))

(define (tree depth)
  (if (= depth 0)
      depth
      (cons (tree (- depth 1)) (tree (- depth 1)))))

(define a (iota 100000))

(define s (tree 16))

(do ((i 0 (+ i 1)))
    ((= i 10))
  (write a (open-output-string))
  (write s (open-output-string)))

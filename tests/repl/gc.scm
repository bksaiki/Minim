; Settings read back.
(collect-nursery-bytes 4096)
(collect-nursery-bytes)
(collect-generation-radix 4)
(collect-generation-radix)
(collect-major-growth 3)
(collect-major-growth)
(collect-major-min-bytes 0)
(collect-major-min-bytes)
(collect)

; Programs behave the same when every allocation collects, with every second
; collection major, and with any growth of the old generation also major.
(collect-nursery-bytes 1)
(collect-generation-radix 2)
(collect-major-growth 1)
(define build (lambda (k acc) (if (= k 0) acc (build (- k 1) (cons k acc)))))
(define lst (build 1000 '()))
(length lst)
(fold + 0 lst)
(list-tail lst 997)
(define v (make-vector 2 (list-tail lst 998)))
(vector-set! v 1 (map (lambda (x) (* x x)) (vector-ref v 0)))
v
(collect-nursery-bytes 1048576)
(collect-generation-radix 0)
(collect-major-growth 2)
(collect-major-min-bytes 1048576)

; Rejected values.
(collect-nursery-bytes 0)
(collect-nursery-bytes -1)
(collect-nursery-bytes 'a)
(collect-generation-radix -1)
(collect-generation-radix "2")
(collect-major-growth 0)
(collect-major-min-bytes -1)
(collect 1)

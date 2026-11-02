; Included by point.sld.
(define-record-type point (make-point x y) point? (x point-x) (y point-y))

(define (point->list p)
  (list (point-x p) (point-y p)))

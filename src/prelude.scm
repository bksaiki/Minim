;;; Library procedures written in Scheme, evaluated by minim_init.  Only the
;;; core forms are available: quote, if, define, set!, lambda, and begin.

;; Whether `x` is a proper list: finite and ending in '().  Floyd's cycle
;; detection makes cyclic lists return #f.
(define list?
  (lambda (x)
    (define loop
      (lambda (slow fast)
        (if (null? fast)
            #t
            (if (pair? fast)
                (if (null? (cdr fast))
                    #t
                    (if (pair? (cdr fast))
                        (if (eq? (cdr (cdr fast)) (cdr slow))
                            #f
                            (loop (cdr slow) (cdr (cdr fast))))
                        #f))
                #f))))
    (loop x x)))

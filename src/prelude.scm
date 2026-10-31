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

(define list-tail
  (lambda (lst k)
    (if (= k 0)
        lst
        (list-tail (cdr lst) (- k 1)))))

(define list-ref
  (lambda (lst k)
    (car (list-tail lst k))))

;; Copies the pairs of `lst`; an improper tail is kept.
(define list-copy
  (lambda (lst)
    (if (pair? lst)
        (cons (car lst) (list-copy (cdr lst)))
        lst)))

(define reverse
  (lambda (lst)
    (define loop
      (lambda (lst acc)
        (if (null? lst)
            acc
            (loop (cdr lst) (cons (car lst) acc)))))
    (loop lst '())))

;; Every list but the last is copied; the last is shared.
(define append
  (lambda lists
    (define append2
      (lambda (a b)
        (if (null? a)
            b
            (cons (car a) (append2 (cdr a) b)))))
    (define loop
      (lambda (lists)
        (if (null? lists)
            '()
            (if (null? (cdr lists))
                (car lists)
                (append2 (car lists) (loop (cdr lists)))))))
    (loop lists)))

;; The first pair of `lst` whose car matches `x` under `same?`, or #f.
(define member
  (lambda (x lst . compare)
    (define same? (if (null? compare) equal? (car compare)))
    (define loop
      (lambda (lst)
        (if (null? lst)
            #f
            (if (same? x (car lst))
                lst
                (loop (cdr lst))))))
    (loop lst)))

(define memq (lambda (x lst) (member x lst eq?)))
(define memv (lambda (x lst) (member x lst eqv?)))

;; The first pair of `alist` whose car matches `x` under `same?`, or #f.
(define assoc
  (lambda (x alist . compare)
    (define same? (if (null? compare) equal? (car compare)))
    (define loop
      (lambda (alist)
        (if (null? alist)
            #f
            (if (same? x (car (car alist)))
                (car alist)
                (loop (cdr alist))))))
    (loop alist)))

(define assq (lambda (x alist) (assoc x alist eq?)))
(define assv (lambda (x alist) (assoc x alist eqv?)))

;; Applies `f` to the elements of the lists in turn, stopping at the shortest.
(define map
  (lambda (f lst . lists)
    (define map1
      (lambda (lst)
        (if (pair? lst)
            (cons (f (car lst)) (map1 (cdr lst)))
            '())))
    (define any-empty?
      (lambda (ls)
        (if (null? ls)
            #f
            (if (pair? (car ls))
                (any-empty? (cdr ls))
                #t))))
    (define cars
      (lambda (ls)
        (if (null? ls) '() (cons (car (car ls)) (cars (cdr ls))))))
    (define cdrs
      (lambda (ls)
        (if (null? ls) '() (cons (cdr (car ls)) (cdrs (cdr ls))))))
    (define mapn
      (lambda (ls)
        (if (any-empty? ls)
            '()
            (cons (apply f (cars ls)) (mapn (cdrs ls))))))
    (if (null? lists)
        (map1 lst)
        (mapn (cons lst lists)))))

;; Like map, but for effect; returns void.  With several lists, it uses map,
;; which also applies `f` from left to right.
(define for-each
  (lambda (f lst . lists)
    (define loop
      (lambda (lst)
        (if (pair? lst)
            (begin (f (car lst)) (loop (cdr lst)))
            (if #f #f))))
    (if (null? lists)
        (loop lst)
        (begin (apply map f lst lists) (if #f #f)))))

(define filter
  (lambda (keep? lst)
    (if (null? lst)
        '()
        (if (keep? (car lst))
            (cons (car lst) (filter keep? (cdr lst)))
            (filter keep? (cdr lst))))))

;; SRFI-1: (kons elem acc), from the left.
(define fold
  (lambda (kons knil lst)
    (if (null? lst)
        knil
        (fold kons (kons (car lst) knil) (cdr lst)))))

;; SRFI-1: (kons elem acc), from the right.
(define fold-right
  (lambda (kons knil lst)
    (if (null? lst)
        knil
        (kons (car lst) (fold-right kons knil (cdr lst))))))

;;; The list library, in Scheme.  Startup loads it before src/expander.scm,
;;; which uses it, both expanded (src/prelude.boot).

;; Whether `x` is a proper list: finite and ending in '().  Floyd's cycle
;; detection makes cyclic lists return #f.
(define list?
  (lambda (x)
    (let loop ((slow x) (fast x))
      (cond ((null? fast) #t)
            ((not (pair? fast)) #f)
            ((null? (cdr fast)) #t)
            ((not (pair? (cdr fast))) #f)
            ((eq? (cdr (cdr fast)) (cdr slow)) #f)
            (else (loop (cdr slow) (cdr (cdr fast))))))))

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
    (let loop ((lst lst) (acc '()))
      (if (null? lst) acc (loop (cdr lst) (cons (car lst) acc))))))

;; Every list but the last is copied; the last is shared.
(define append
  (lambda lists
    (define append2
      (lambda (a b)
        (if (null? a)
            b
            (cons (car a) (append2 (cdr a) b)))))
    (let loop ((lists lists))
      (cond ((null? lists) '())
            ((null? (cdr lists)) (car lists))
            (else (append2 (car lists) (loop (cdr lists))))))))

;; The first pair of `lst` whose car matches `x` under `same?`, or #f.
(define member
  (lambda (x lst . compare)
    (let ((same? (if (null? compare) equal? (car compare))))
      (let loop ((lst lst))
        (cond ((null? lst) #f)
              ((same? x (car lst)) lst)
              (else (loop (cdr lst))))))))

(define memq (lambda (x lst) (member x lst eq?)))
(define memv (lambda (x lst) (member x lst eqv?)))

;; The first pair of `alist` whose car matches `x` under `same?`, or #f.
(define assoc
  (lambda (x alist . compare)
    (let ((same? (if (null? compare) equal? (car compare))))
      (let loop ((alist alist))
        (cond ((null? alist) #f)
              ((same? x (car (car alist))) (car alist))
              (else (loop (cdr alist))))))))

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
        (and (pair? ls) (or (not (pair? (car ls))) (any-empty? (cdr ls))))))
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
    (if (null? lists)
        (let loop ((lst lst))
          (when (pair? lst)
            (f (car lst))
            (loop (cdr lst))))
        (begin (apply map f lst lists) (if #f #f)))))

(define filter
  (lambda (keep? lst)
    (cond ((null? lst) '())
          ((keep? (car lst)) (cons (car lst) (filter keep? (cdr lst))))
          (else (filter keep? (cdr lst))))))

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

;; Continuations.
(define call/cc %call/cc)
(define call-with-current-continuation %call/cc)

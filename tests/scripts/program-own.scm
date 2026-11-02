; A program's definitions are its own: car isn't imported, so it can define it.
(import (only (scheme base) define newline quote) (scheme write))

(define (car x)
  'mine)

(display (car '(1 2)))
(newline)

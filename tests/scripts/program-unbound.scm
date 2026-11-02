; A program sees only what it imports: car is in (scheme base).
(import (scheme write))

(display "before")
(display (car "x"))

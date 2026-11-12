; Run with --interpret --program: the program's closures are uncompiled.
(import (scheme base) (scheme write) (minim system))
(write (car (%closure-code (lambda (x) x))))
(newline)

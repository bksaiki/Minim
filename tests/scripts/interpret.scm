; Run with --interpret: closures are made from the core, uncompiled.
(import (minim system))
(write (car (%closure-code (lambda (x) x))))
(newline)
(write (%compile))
(newline)

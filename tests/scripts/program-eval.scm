; A program can eval in an environment it builds.
(import (scheme base) (scheme eval) (scheme write))

(display (eval '(* 6 7) (environment '(scheme base))))
(newline)

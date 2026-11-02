; Found on MINIM_LIBRARY_PATH by library-path.scm.
(define-library (greeting)
  (export greeting)
  (import (scheme base))
  (begin
    (define greeting "hello from the library path")))

; Includes a file that doesn't exist.
(define-library (lib bad-include) (export) (import (scheme base)) (include "missing.scm"))

; A program's command line: its path, then its arguments.
(import (scheme base) (scheme process-context) (scheme write))

(write (command-line))
(newline)

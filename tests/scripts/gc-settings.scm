; Prints the collection settings, which the MINIM_GC_ variables set.
(write (list (collect-nursery-bytes)
             (collect-generation-radix)
             (collect-major-growth)
             (collect-major-min-bytes)))
(newline)

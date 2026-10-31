; Prints the collection settings, which MINIM_GC_NURSERY and MINIM_GC_RADIX set.
(write (list (collect-nursery-bytes) (collect-generation-radix)))
(newline)

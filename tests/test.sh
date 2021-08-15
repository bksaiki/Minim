#!/bin/bash

failed=0
total=0
for file in $@
do
    echo "Testing module:" $file
    eval $file
    if (( $? != 0 )); then
        echo "Module failed."
        ((failed++))
    else
        echo "Module passed."
    fi
    ((total++))
done

printf "%i/%i modules passed\n" $(expr $total - $failed) $total
if (($failed != 0)); then
    exit 1
fi

#!/bin/bash

echo "Running library tests..."

tmp="/tmp/minim-test.log"
failed=0
total=0

cd $(dirname $0)

for file in $(ls lib/); do
    ../minim "lib/$file" > $tmp
     grep -v "#t" $tmp > /dev/null
    if [ $? -eq 0 ]; then
        echo "[ FAIL ] $file"
        ((failed++))
    else
        echo "[ PASS ] $file"
    fi
    ((total++))
done

printf "%i/%i tests passed\n" $(expr $total - $failed) $total
if (($failed != 0)); then
    exit 1
fi

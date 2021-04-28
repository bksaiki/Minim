#!/bin/bash

echo "Running library tests..."

tmp="/tmp/minim-test.log"
ret=0

cd $(dirname $0)

for file in $(ls lib/); do
    ../minim "lib/$file" > $tmp
     grep -v "#t" $tmp > /dev/null
    if [ $? -eq 0 ]; then
        echo "[ FAIL ] $file"
        grep -n "#f" $tmp
        ret=1
    else
        echo "[ PASS ] $file"
    fi
done

echo "Done."
exit $ret

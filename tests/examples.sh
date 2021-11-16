#!/bin/bash

echo "Running examples..."

failed=0
total=0

cd $(dirname $0)

for file in $(ls examples/); do
    echo "] $file"
    ../minim "examples/$file" > "/dev/null"
    if [ $? -ne 0 ]; then
        echo "[ FAIL ] $file"
        ((failed++))
    fi
    ((total++))
done

printf "%i/%i examples ran\n" $(expr $total - $failed) $total
if (($failed != 0)); then
    exit 1
fi

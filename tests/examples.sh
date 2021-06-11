#!/bin/bash

echo "Running examples..."

cd $(dirname $0)

for file in $(ls examples/); do
    echo "] $file"
    ../minim "examples/$file" > "/dev/null"
done

echo "Done."

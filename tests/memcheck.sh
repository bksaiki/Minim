#!/bin/bash
echo "Running valgrind..."
for file in $@
do
    eval "valgrind -q --error-exitcode=1 --leak-check=yes --errors-for-leak-kinds=all" $file "> /dev/null"
    if (( $? != 0 )); then
        echo "$file "failed.""
        exit 1
    else
        echo $file "passed."
    fi
done
echo "Done."
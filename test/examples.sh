#!/bin/bash

RED="\e[31m"
GREEN="\e[32m"
ENDCOLOR="\e[0m"

# determine physical directory of this script
src="${BASH_SOURCE[0]}"
while [ -L "$src" ]; do
  dir="$(cd -P "$(dirname "$src")" && pwd)"
  src="$(readlink "$src")"
  [[ $src != /* ]] && src="$dir/$src"
done
MYDIR="$(cd -P "$(dirname "$src")" && pwd)"

# Find all files in Minim core library
MINIM="$MYDIR/../build/boot/minim"
EXAMPLE_SRC="$MYDIR/examples"
SRCS=$(find $EXAMPLE_SRC -type f -name "*.min")
TESTS=$(find $MYDIR/base -type f -name "*.min")

#
#   Core library
#

failed=0
total=0

echo "Running interpreter on examples..."

for file in $SRCS; do
  $MINIM -q $file
  if [ $? -ne 0 ]; then
    echo -e "[ ${RED}FAIL${ENDCOLOR} ] $file"
    ((failed++))
  else
    echo -e "[ ${GREEN}PASS${ENDCOLOR} ] $file"
  fi
  ((total++))
done

printf "%i/%i files run\n" $(expr $total - $failed) $total
if [[ $failed != 0 ]]; then
  exit 1
fi

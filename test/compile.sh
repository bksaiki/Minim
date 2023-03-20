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
MINIM_SRC="$MYDIR/../src/library"
CORE_SRC="$MINIM_SRC/private"
SRCS=$(find $CORE_SRC -type f -name "*.min")
TESTS=$(find $MYDIR/base -type f -name "*.min")

#
#   Core library
#

failed=0
total=0

for file in $SRCS; do
  $MINIM -q $MINIM_SRC/compiler.min $file
  if [ $? -ne 0 ]; then
    echo -e "[ ${RED}FAIL${ENDCOLOR} ] $file"
    ((failed++))
  else
    echo -e "[ ${GREEN}PASS${ENDCOLOR} ] $file"
  fi
  ((total++))
done

printf "%i/%i files compiled\n" $(expr $total - $failed) $total
if [[ $failed != 0 ]]; then
  exit 1
fi

#
#   Unit tests
#

# failed=0
# total=0

# for file in $TESTS; do
#   $MINIM -q $MINIM_SRC/compiler.min $file
#   if [ $? -ne 0 ]; then
#     echo -e "[ ${RED}FAIL${ENDCOLOR} ] $file"
#     ((failed++))
#   else
#     echo -e "[ ${GREEN}PASS${ENDCOLOR} ] $file"
#   fi
#   ((total++))
# done

# printf "%i/%i compiled tests passed\n" $(expr $total - $failed) $total
# if [[ $failed != 0 ]]; then
#   exit 1
# fi

#!/usr/bin/env bash
# Usage: bench/run.sh BUILD_A BUILD_B [RUNS]
# Runs each benchmark with BUILD_A/minim and BUILD_B/minim alternately, RUNS
# times each (default 5), and prints each build's median and minimum seconds.
set -euo pipefail

a=$1/minim
b=$2/minim
runs=${3:-5}
dir=$(cd "$(dirname "$0")" && pwd)

# Seconds `$1 $2` takes, its output discarded.
elapsed() {
    local TIMEFORMAT=%R
    { time "$1" "$2" > /dev/null 2>&1; } 2>&1
}

# The median and minimum of the numbers on stdin.
stats() {
    sort -n | awk '{ v[NR] = $1 } END { printf "%.3f  %.3f", v[int((NR + 1) / 2)], v[1] }'
}

printf '%-10s  %-16s  %-16s\n' benchmark "A median  min" "B median  min"
for file in "$dir"/*.scm; do
    ta=() tb=()
    for ((i = 0; i < runs; i++)); do
        ta+=("$(elapsed "$a" "$file")")
        tb+=("$(elapsed "$b" "$file")")
    done
    printf '%-10s  %-16s  %-16s\n' "$(basename "$file" .scm)" \
        "$(printf '%s\n' "${ta[@]}" | stats)" "$(printf '%s\n' "${tb[@]}" | stats)"
done

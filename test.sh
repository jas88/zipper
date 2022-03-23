#!/bin/bash

# Generate multiple test directories for the zipper repack tool
# (c) James A Sutherland/University of Dundee 2022
#
# Usage: ./test.sh [n [s]]
# n = number of directories (default 100)
# s = size of each test file (default 1M)
# Each directory will contain 100 files

set -e

DIRS="${1:-100}"
SIZE="${2:-1M}"

for i in $(seq -f "%010g" 1 "$DIRS")
do
        mkdir -p "$i"
        for j in $(seq 1 100)
        do
                dd if=/dev/urandom of="$i/$j.dcm" bs="$SIZE" count=1 status=none
        done
done

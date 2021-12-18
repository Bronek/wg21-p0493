#!/bin/bash

DIR=$(dirname "$BASH_SOURCE[0]")
DIR=$(realpath "$DIR")
NUM=$1
shift
"$DIR"/bench -c 4 $@ -i 10000 2>&1 1>/dev/null | grep -vE "iterations|Calibration|seed|Will use|core\(s\)|^$"
  for i in 1 2 4 8 16 24 32 40 48 56 64; do
    for j in $(seq $NUM); do
      "$DIR"/bench -c $i $@ 2>/dev/null
    done
done

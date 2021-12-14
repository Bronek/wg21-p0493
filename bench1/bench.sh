#!/bin/bash

DIR=$(dirname "$BASH_SOURCE[0]")
DIR=$(realpath "$DIR")
while :; do
  for i in 1 2 4 8 16 24 32 40 48 56 64; do
    "$DIR"/bench1 -c $i $@ 2>/dev/null
  done
done

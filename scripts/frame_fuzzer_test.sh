#!/usr/bin/env bash

if [ ! -s ./build/frame_fuzzer ]; then
    echo "./build/frame_fuzzer binary not found!"
    exit 1
fi

shopt -s nullglob
for fuzzcase in ./test/fuzzer_testcases/frame_fuzzer/*; do
  echo "testing with $fuzzcase..."
  ./build/frame_fuzzer --v=100 < $fuzzcase
done

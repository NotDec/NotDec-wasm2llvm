#!/bin/bash

for FILE in $(find lib tools include -name *.cpp -o -name *.h)
do
  clang-format -i --style="{BasedOnStyle: llvm, IndentWidth: 2}" $FILE
done

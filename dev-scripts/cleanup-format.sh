#!/usr/bin/env bash

style=$({ sed 's/  */ /g; s/#.*//; /^ *$/d; s/$/,/; $s/, $//' | tr -d '\n'; } <<-EOF
  BasedOnStyle: LLVM
  ColumnLimit: 80
  IndentWidth: 4
  UseTab:      Never
  AllowShortFunctionsOnASingleLine: None
  KeepEmptyLinesAtTheStartOfBlocks: false
  MaxEmptyLinesToKeep: 2
EOF
)

clang-format -style="{${style}}"  "$@"

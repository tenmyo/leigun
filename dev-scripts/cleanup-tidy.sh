#!/usr/bin/env bash
# https://matt.sh/howto-c

clang-tidy \
    -fix \
    -fix-errors \
    -header-filter=.* \
    --checks=readability-braces-around-statements,misc-macro-parentheses \
    "$@" \
    -- -I.

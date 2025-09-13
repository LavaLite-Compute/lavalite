#!/bin/bash

for file in *.c; do
  cp "$file" "$file.bak"
  sed -i -E \
    -e 's|#include "../../([^"]+\.h)"|#include "\1"|g' \
    -e 's|#include "../([^"]+\.h)"|#include "lsf/\1"|g' \
    -e 's|#include "./([^"]+\.h)"|#include "\1"|g' \
    "$file"
done


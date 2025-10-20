#!/bin/bash
#!/bin/bash

for file in *.c; do
  cp "$file" "$file.bak"

  # Remove all includes of lsf/lib/*.h
  sed -i -E '/#include *"lsf\/lib\/[^"]+\.h"/d' "$file"

  # Insert lsflib.h after the last system include or at the top
  awk '
    BEGIN { inserted = 0 }
    /^#include *<.*>$/ { print; last_include = NR; next }
    NR == last_include + 1 && !inserted {
      print "#include \"lsf/lib/lsflib.h\""
      inserted = 1
    }
    { print }
  ' "$file" > "$file.tmp" && mv "$file.tmp" "$file"
done

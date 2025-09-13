#!/usr/bin/env bash
# Run from the lib/ dir:  ./fix_includes_here.sh
set -euo pipefail
shopt -s nullglob

INC_LINE='#include "lsf/lib/lsflib.h"'

for f in *.c; do
  cp -- "$f" "$f.bak"  # backup

  tmp="$(mktemp)"
  awk -v inc="$INC_LINE" '
    BEGIN { at_start=1; in_lic=0; inserted=0 }
    function is_include(s) { return (s ~ /^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]/ ) }
    function insert_inc() {
      if (!inserted) { print inc; print ""; inserted=1 }
    }
    {
      line=$0

      # Top-of-file handling
      if (at_start) {
        # Keep leading blank lines as-is
        if (line ~ /^[[:space:]]*$/) { print line; next }

        # If we see includes *before* the license, DROP them (we will reinsert one later)
        if (is_include(line)) { next }

        # License starts
        if (line ~ /^[[:space:]]*\/\*/) {
          at_start=0; in_lic=1; print line
          # License ends on same line?
          if (line ~ /\*\//) { insert_inc(); in_lic=0 }
          next
        }

        # No license: insert include now, then continue with body
        at_start=0
        insert_inc()
        if (!is_include(line)) print line
        next
      }

      # Inside license block: print verbatim, then insert include right after closing */
      if (in_lic) {
        print line
        if (line ~ /\*\//) { insert_inc(); in_lic=0 }
        next
      }

      # Normal body: drop ALL includes (system/our/others)
      if (is_include(line)) next

      print line
    }
    END {
      # Safety: if file was empty/odd and we never inserted, add include at top
      if (!inserted) { print inc; print "" }
    } ' "$f" > "$tmp"

  mv -- "$tmp" "$f"
  echo "fixed: $f  (backup: $f.bak)"
done

echo "done."


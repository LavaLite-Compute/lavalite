#!/usr/bin/env bash
# lavalite/scripts/verify_i18n_purge.sh
# Fail if any legacy i18n bits remain in the source tree.

set -euo pipefail
cd "${1:-.}"  # allow optional path, default "."

# Directories/files to ignore
EXCLUDES=(
  --exclude-dir=.git
  --exclude-dir=build
  --exclude-dir=dist
  --exclude-dir=out
  --exclude-dir=tmp
  --exclude='*.o' --exclude='*.a' --exclude='*.so*'
  --exclude='*.bak' --exclude='*.orig' --exclude='*.rej'
  --exclude='*.patch' --exclude='*.diff'
)

# Only scan sources/headers by default; add others if you want
INCLUDES=( --include='*.c' --include='*.h' --include='*.cc' --include='*.hh' )

echo "== Verifying i18n purge in: $(pwd)"
LC_ALL=C

declare -a patterns=(
  '_i18n_msg_get'
  '\bI18N_MSG\b'
  '/\*\s*catgets\s*[0-9]+\s*\*/'
  '^[[:space:]]*#[[:space:]]*define[[:space:]]+NL_SETN\b'
  '<nl_types\.h>'
  '\bcatopen\s*\('
  '\bcatgets\s*\('
  '\bcatclose\s*\('
  '\bls_catd\b'
)

fail=0

for pat in "${patterns[@]}"; do
  echo
  echo "--- Searching for: $pat"
  if grep -RInE "${EXCLUDES[@]}" "${INCLUDES[@]}" "$pat" . >/tmp/i18n_hits.$$ 2>/dev/null; then
    hits=$(wc -l < /tmp/i18n_hits.$$)
    echo "FOUND $hits occurrence(s):"
    # show up to first 20 lines (adjust to taste)
    head -n 20 /tmp/i18n_hits.$$
    echo "… (showing up to 20)."
    fail=1
  else
    echo "OK (none)"
  fi
  rm -f /tmp/i18n_hits.$$ || true
done

echo
if [[ $fail -ne 0 ]]; then
  echo "❌ i18n purge check FAILED — leftovers found."
  exit 1
else
  echo "✅ i18n purge check PASSED — no leftovers detected."
fi


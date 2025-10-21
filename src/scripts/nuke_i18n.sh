#!/usr/bin/env bash
set -euo pipefail

# Target source files
files=$(git ls-files '*.c' '*.h' || true)
if [[ -z "${files}" ]]; then
  echo "No C/H files found (are you in a git repo?)"
  exit 1
fi

echo "[i18n] Processing $(wc -w <<<"$files") files..."

# A) _i18n_ctime(ls_catd, CTIME_FORMAT_..., &t) -> ctime(&t)  (multi-line safe)
perl -0777 -i -pe '
  s/_i18n_ctime\s*\(\s*ls_catd\s*,\s*CTIME_FORMAT_[^,]+,\s*(&?\s*[^)]+?)\s*\)/ctime($1)/g
' $files

# B) Remove ls_catd declarations (extern or plain)
perl -i -ne '
  print unless /^\s*(extern\s+)?[A-Za-z_][\w\s\*]*\bls_catd\b\s*;\s*$/
' $files

# C) Drop old i18n headers
perl -i -ne '
  print unless /^\s*#\s*include\s*(<|")\s*(?:i18n|ls_i18n|nl_types)\.h\s*(?:>|")/
' $files

# D) Replace ONLY CamelCase I18N_* tokens with string literals (keeps ALL_CAPS macros)
perl -i -pe '
  s/\bI18N_([A-Z][a-zA-Z0-9]*)\b/"$1"/g
' $files

# E) Optional: turn I18N_m(id,"text") / I18N(id,"text") -> "text" (keeps code readable)
perl -0777 -i -pe '
  s/\bI18N_m\s*\(\s*[^,]*\s*,\s*(" (?:[^"\\]|\\.)* ")\s*\)/$1/xg;
  s/\bI18N\s*\(\s*[^,]*\s*,\s*(" (?:[^"\\]|\\.)* ")\s*\)/$1/xg;
' $files

# F) Replace _i18n_printf(I18N_FUNC_S_FAIL, "text") → "text" (preserve message)
perl -0777 -i -pe '
  s/_i18n_printf\s*\(\s*I18N_FUNC_S_FAIL\s*,\s*(" (?:[^"\\]|\\.)* ")\s*\)/$1/xg;
' $files

# G) Strip _i18n_printf(...) while preserving all arguments and formatting
perl -0777 -i -pe '
  s/\blsb_mperr\s*\(\s*_i18n_printf\s*\(\s*I18N_FUNC_S_FAIL\s*,\s*([^)]+?)\)\s*\)/lsb_mperr($1)/gs;
  s/\blsb_merr\s*\(\s*_i18n_printf\s*\(\s*([^)]+?)\)\s*\)/lsb_merr($1)/gs;
' $files

echo "[i18n] Done. Preview changes with: git -c color.ui=always diff -- . ':!nuke_i18n.sh'"


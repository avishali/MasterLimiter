#!/usr/bin/env bash
# check_ui_ascii.sh
# Fail if any UI string literal in Source/ contains raw non-ASCII bytes.
#
# WHY: bare narrow literals like "-" (em/en dash), "->" (arrow) or "x" (multiply-x)
# typed as real UTF-8 glyphs render as mojibake ("aTM"/"a EUR") on the plugin UI,
# depending on source/execution charset. The proven-safe pattern (used across the
# shared mdsp_ui library) is:  juce::String::fromUTF8 (u8"<glyph>")  for real glyphs,
# and plain ASCII everywhere else.
#
# RULE enforced here: a line that (a) contains a string literal (") and (b) is not a
# pure // comment and (c) does NOT use fromUTF8, must be pure ASCII.
#
# Run standalone, from a slice gate, or wire into CI / a pre-commit hook.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="$ROOT/Source"

violations="$(
  find "$SRC" \( -name '*.cpp' -o -name '*.h' \) -print0 \
  | while IFS= read -r -d '' f; do
      perl -ne '
        my $line = $_;
        $line =~ s{//.*}{};              # drop line comments
        next if $line =~ /fromUTF8/;     # allowed glyph pattern
        next unless $line =~ /"/;        # only lines with a string literal
        print "$ARGV:$.: $_" if $line =~ /[^\x00-\x7F]/;
      ' "$f"
    done
)"

if [[ -n "$violations" ]]; then
  echo "ERROR: raw non-ASCII in UI string literal(s)." >&2
  echo "Fix: use ASCII, or juce::String::fromUTF8 (u8\"<glyph>\") for an intentional glyph." >&2
  echo "----" >&2
  printf '%s\n' "$violations" >&2
  exit 1
fi

echo "check_ui_ascii: OK (no raw non-ASCII UI string literals in Source/)."

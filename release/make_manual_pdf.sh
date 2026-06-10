#!/usr/bin/env bash
# Render docs/SIGNAL_FLOW.md -> release/MasterLimiter-Manual.pdf (pandoc + Chrome headless).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CSS=$(mktemp /tmp/manual.XXXX.css)
cat > "$CSS" <<'CSS'
@page { size:A4; margin:16mm 14mm; }
body { font-family:-apple-system,"Helvetica Neue",Arial,sans-serif; color:#1a1f26; line-height:1.45; font-size:9.5pt; }
h1 { color:#0f8c7e; border-bottom:3px solid #33d2be; padding-bottom:6px; font-size:19pt; margin-top:0; }
h2 { color:#0f8c7e; margin-top:1.2em; font-size:13pt; border-bottom:1px solid #d7e0e6; padding-bottom:3px; }
h3 { color:#16303a; font-size:11pt; } code { background:#eef3f5; padding:1px 3px; border-radius:3px; font-size:8.5pt; }
pre { background:#0d1015; color:#e8edf3; padding:8px 10px; border-radius:6px; font-size:8pt; white-space:pre-wrap; }
pre code { background:none; color:inherit; } a { color:#0f8c7e; } strong { color:#0a5e54; }
table { border-collapse:collapse; width:100%; font-size:8.5pt; margin:.5em 0; }
th,td { border:1px solid #d7e0e6; padding:3px 6px; text-align:left; vertical-align:top; } th { background:#eef3f5; color:#0a5e54; }
ul,ol { margin:.3em 0 .6em 1.1em; } li { margin:.15em 0; } hr { border:none; border-top:1px solid #d7e0e6; margin:1.1em 0; }
blockquote { border-left:3px solid #33d2be; margin:.5em 0; padding:.2em .8em; background:#f4faf9; color:#3a4a52; }
CSS
HTML=$(mktemp /tmp/manual.XXXX.html)
pandoc "$ROOT/docs/SIGNAL_FLOW.md" -s --embed-resources --css "$CSS" \
  --metadata title="MasterLimiter 0.3.1-beta — Signal Flow & Manual" -o "$HTML"
"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless --disable-gpu \
  --no-pdf-header-footer --print-to-pdf="$ROOT/release/MasterLimiter-Manual.pdf" "$HTML" 2>/dev/null || true
echo "wrote release/MasterLimiter-Manual.pdf"

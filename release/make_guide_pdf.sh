#!/usr/bin/env bash
# Render docs/BETA_TESTER_GUIDE.md -> release/MasterLimiter-Beta-Guide.pdf (pandoc + Chrome headless).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CSS=$(mktemp /tmp/guide_css.XXXX.css)
cat > "$CSS" <<'CSS'
@page { size: A4; margin: 18mm 16mm; }
body { font-family: -apple-system,"Helvetica Neue",Arial,sans-serif; color:#1a1f26; line-height:1.5; font-size:11pt; }
h1 { color:#0f8c7e; border-bottom:3px solid #33d2be; padding-bottom:6px; font-size:22pt; margin-top:0; }
h2 { color:#0f8c7e; margin-top:1.4em; font-size:14pt; border-bottom:1px solid #d7e0e6; padding-bottom:3px; }
h3 { color:#16303a; font-size:12pt; }
code { background:#eef3f5; padding:1px 4px; border-radius:3px; font-size:9.5pt; }
pre { background:#0d1015; color:#e8edf3; padding:10px 12px; border-radius:6px; font-size:9pt; }
pre code { background:none; color:inherit; } a { color:#0f8c7e; } strong { color:#0a5e54; }
ul,ol { margin:0.4em 0 0.8em 1.2em; } li { margin:0.2em 0; } hr { border:none; border-top:1px solid #d7e0e6; margin:1.4em 0; }
CSS
HTML=$(mktemp /tmp/guide.XXXX.html)
pandoc "$ROOT/docs/BETA_TESTER_GUIDE.md" -s --embed-resources --css "$CSS" \
  --metadata title="MasterLimiter 0.3.1-beta — Tester Guide" -o "$HTML"
"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless --disable-gpu \
  --no-pdf-header-footer --print-to-pdf="$ROOT/release/MasterLimiter-Beta-Guide.pdf" "$HTML" 2>/dev/null || true
echo "wrote release/MasterLimiter-Beta-Guide.pdf"

#!/usr/bin/env bash
# Full macOS release pipeline producing a signed + NOTARIZED .pkg (AnalyzerPro-style).
#
# Duplicated alongside release_macos.sh (which notarizes a DMG). This variant runs:
#   build -> PACE sign AAX -> productsign + notarize the .pkg -> staple -> signed zip/dmg
# and calls scripts/sign_and_notarize_pkg.sh.
#
# Prerequisites:
#   - JUCE_PATH, MELECHDSP_HQ_ROOT, and AAX_SDK_PATH in the shell or scripts/.env.
#   - Apple signing + notarytool profile (see scripts/sign_and_notarize_pkg.sh).
#   - For AAX: scripts/.aax_wraptool.env (see scripts/.aax_wraptool.env.example).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# shellcheck source=/dev/null
source "$SCRIPT_DIR/source_repo_env.sh"

BUILD_DIR="${MASTERLIMITER_RELEASE_BUILD_DIR:-build-release}"
PLUGIN_NAME="MasterLimiter"
ARTIFACTS_DIR="$BUILD_DIR/${PLUGIN_NAME}_artefacts/Release"
AAX_MACHO="$ARTIFACTS_DIR/AAX/${PLUGIN_NAME}.aaxplugin/Contents/MacOS/${PLUGIN_NAME}"

echo "=========================================="
echo "  MasterLimiter full macOS release (signed .pkg)"
echo "=========================================="
echo "  BUILD_DIR: $BUILD_DIR"
echo "=========================================="
echo ""

if [[ -z "${JUCE_PATH:-}" ]]; then
  echo "JUCE_PATH is not set. export JUCE_PATH=... or add it to scripts/.env" >&2
  exit 1
fi

echo "Step 1/3: Release build (no auto-install to system folders)"
echo "-----------------------"
"$SCRIPT_DIR/build_release_pkg.sh"
echo ""

if [[ -f "$AAX_MACHO" ]]; then
  echo "Step 2/3: PACE sign AAX (wraptool)"
  echo "----------------------------------"
  "$SCRIPT_DIR/wraptool_sign_aax.sh" "$AAX_MACHO"
  echo ""
  export SIGN_AND_NOTARIZE_SKIP_AAX=1
else
  echo "Step 2/3: No AAX Mach-O at"
  echo "  $AAX_MACHO"
  echo "  (skipping wraptool)"
  echo ""
  unset SIGN_AND_NOTARIZE_SKIP_AAX || true
fi

echo "Step 3/3: Apple sign + productsign .pkg + notarize + staple"
echo "-----------------------------------------------------------"
if [[ "${SIGN_AND_NOTARIZE_SKIP_AAX:-}" == "1" ]]; then
  echo "  (SIGN_AND_NOTARIZE_SKIP_AAX=1 - AAX will not be Apple re-signed)"
fi
"$SCRIPT_DIR/sign_and_notarize_pkg.sh"
echo ""

echo "=========================================="
echo "  Release pipeline finished"
echo "=========================================="
echo "  See installer/ for the signed+notarized .pkg and archives"
echo "=========================================="

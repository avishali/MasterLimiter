#!/usr/bin/env bash

set -euo pipefail

# macOS release signing helper for MasterLimiter artifacts.
# Signs Standalone/AU/VST3/AAX bundles in place when present.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

BUILD_DIR="${BUILD_DIR:-build-release}"
CONFIG="${CONFIG:-Release}"
PLUGIN_NAME="${PLUGIN_NAME:-MasterLimiter}"
ARTIFACTS_DIR="${BUILD_DIR}/${PLUGIN_NAME}_artefacts/${CONFIG}"
STANDALONE_ENTITLEMENTS="${STANDALONE_ENTITLEMENTS:-$PROJECT_ROOT/resources/MasterLimiter-standalone.entitlements}"

IDENTITY="${DEVELOPER_ID_APP:-Developer ID Application: AVISHAY LIDANI (C5UC779LGC)}"

if [[ ! -d "$ARTIFACTS_DIR" ]]; then
  echo "ERROR: Artifacts directory not found: $ARTIFACTS_DIR"
  echo "Run a release build first, for example:"
  echo "  cmake --build build-release -j"
  exit 1
fi

sign_bundle() {
  local path="$1"
  local entitlements="${2:-}"

  if [[ -n "$entitlements" ]]; then
    if [[ ! -f "$entitlements" ]]; then
      echo "ERROR: Entitlements file not found: $entitlements"
      exit 1
    fi
  fi

  if [[ -d "$path" ]]; then
    echo "Signing: $path"
    if [[ -n "$entitlements" ]]; then
      codesign --force --deep --options runtime --timestamp \
        --entitlements "$entitlements" \
        --sign "$IDENTITY" \
        "$path"
    else
      codesign --force --deep --options runtime --timestamp \
        --sign "$IDENTITY" \
        "$path"
    fi
    codesign --verify --deep --strict --verbose=2 "$path"
  else
    echo "Skip (not found): $path"
  fi
}

sign_bundle "${ARTIFACTS_DIR}/Standalone/${PLUGIN_NAME}.app" "$STANDALONE_ENTITLEMENTS"
sign_bundle "${ARTIFACTS_DIR}/AU/${PLUGIN_NAME}.component"
sign_bundle "${ARTIFACTS_DIR}/VST3/${PLUGIN_NAME}.vst3"
sign_bundle "${ARTIFACTS_DIR}/AAX/${PLUGIN_NAME}.aaxplugin"

echo ""
echo "Signing complete."
echo "Note: retail Pro Tools requires MasterLimiter-specific PACE/Avid AAX signing."

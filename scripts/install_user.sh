#!/usr/bin/env bash
# Install built MasterLimiter AU/VST3 to the USER plugin folders (what most DAWs prefer).
# Usage: ./scripts/install_user.sh [build-dir]
# Default build-dir: build-release (release script) or build (dev preset).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PLUGIN_NAME="MasterLimiter"

BUILD_DIR="${1:-}"
if [[ -z "$BUILD_DIR" ]]; then
    if [[ -d "$PROJECT_ROOT/build-release/MasterLimiter_artefacts/Release" ]]; then
        BUILD_DIR="$PROJECT_ROOT/build-release"
    else
        BUILD_DIR="$PROJECT_ROOT/build"
    fi
fi

ARTIFACTS="$BUILD_DIR/${PLUGIN_NAME}_artefacts/Release"
SRC_AU="$ARTIFACTS/AU/${PLUGIN_NAME}.component"
SRC_VST3="$ARTIFACTS/VST3/${PLUGIN_NAME}.vst3"
DST_AU="$HOME/Library/Audio/Plug-Ins/Components/${PLUGIN_NAME}.component"
DST_VST3="$HOME/Library/Audio/Plug-Ins/VST3/${PLUGIN_NAME}.vst3"

if [[ ! -d "$SRC_AU" ]]; then
    echo "Error: AU artefact not found: $SRC_AU" >&2
    exit 1
fi
if [[ ! -d "$SRC_VST3" ]]; then
    echo "Error: VST3 artefact not found: $SRC_VST3" >&2
    exit 1
fi

GIT_HEAD="$(git -C "$PROJECT_ROOT" rev-parse --short HEAD 2>/dev/null || echo nogit)"
echo "Installing MasterLimiter @ ${GIT_HEAD} from ${BUILD_DIR}"

mkdir -p "$HOME/Library/Audio/Plug-Ins/Components" "$HOME/Library/Audio/Plug-Ins/VST3"
rm -rf "$DST_AU" "$DST_VST3"
ditto "$SRC_AU" "$DST_AU"
ditto "$SRC_VST3" "$DST_VST3"

echo ""
echo "=== Verification (pre-sign) ==="
echo "HEAD: ${GIT_HEAD}"
echo "Built AU:  $(ls -la "$SRC_AU/Contents/MacOS/$PLUGIN_NAME")"
echo "Installed: $(ls -la "$DST_AU/Contents/MacOS/$PLUGIN_NAME")"
cmp "$SRC_AU/Contents/MacOS/$PLUGIN_NAME" "$DST_AU/Contents/MacOS/$PLUGIN_NAME"
echo "cmp AU: identical"

codesign --force --sign - --deep "$DST_AU" >/dev/null
codesign --force --sign - --deep "$DST_VST3" >/dev/null

if command -v auval >/dev/null 2>&1; then
    echo ""
    echo "Running auval aufx MaLm Melc ..."
    auval -v aufx MaLm Melc
fi

killall -9 AudioComponentRegistrar 2>/dev/null || true
echo ""
echo "Installed to user plugin folders. Rescan plugins in your DAW before testing."

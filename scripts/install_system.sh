#!/usr/bin/env bash
# Install Debug AU/VST3 artifacts into system-wide macOS plugin folders.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

CONFIG="${CONFIG:-Debug}"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build-debug}"
ARTIFACTS_DIR="$BUILD_DIR/MasterLimiter_artefacts/$CONFIG"

VST3_SRC="$ARTIFACTS_DIR/VST3/MasterLimiter.vst3"
AU_SRC="$ARTIFACTS_DIR/AU/MasterLimiter.component"

VST3_DEST_DIR="/Library/Audio/Plug-Ins/VST3"
AU_DEST_DIR="/Library/Audio/Plug-Ins/Components"

if [[ ! -d "$VST3_SRC" || ! -d "$AU_SRC" ]]; then
    echo "Error: MasterLimiter Debug artifacts were not found." >&2
    echo "Expected:" >&2
    echo "  $VST3_SRC" >&2
    echo "  $AU_SRC" >&2
    echo "Build first with: ./scripts/build.sh" >&2
    exit 1
fi

echo "Installing MasterLimiter to system plugin folders:"
echo "  VST3: $VST3_DEST_DIR/MasterLimiter.vst3"
echo "  AU:   $AU_DEST_DIR/MasterLimiter.component"

sudo mkdir -p "$VST3_DEST_DIR" "$AU_DEST_DIR"

sudo rm -rf "$VST3_DEST_DIR/MasterLimiter.vst3"
sudo cp -R "$VST3_SRC" "$VST3_DEST_DIR/"

sudo rm -rf "$AU_DEST_DIR/MasterLimiter.component"
sudo cp -R "$AU_SRC" "$AU_DEST_DIR/"

echo "System install complete."

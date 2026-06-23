#!/usr/bin/env bash
# DEV-enabled beta release: PLUGIN_DEV_MODE=ON, PACE AAX, notarized .pkg.
# Same pipeline as release_macos_pkg.sh with dev-mode build + post-build verification.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

export PLUGIN_DEV_MODE=ON

GIT_HEAD="$(git rev-parse --short HEAD)"
echo "=========================================="
echo "  MasterLimiter DEV beta release"
echo "  git HEAD: ${GIT_HEAD}"
echo "  PLUGIN_DEV_MODE=ON"
echo "=========================================="
echo ""

"$SCRIPT_DIR/release_macos_pkg.sh"

BUILD_DIR="${MASTERLIMITER_RELEASE_BUILD_DIR:-build-release}"
ARTIFACTS="$BUILD_DIR/MasterLimiter_artefacts/Release"
PLUGIN="MasterLimiter"

verify_binary() {
    local path="$1"
    local label="$2"
    echo "--- ${label} ---"
    ls -la "$path"
    dwarfdump --uuid "$path" 2>/dev/null | head -3 || true
    if strings "$path" | grep -q "${GIT_HEAD}"; then
        echo "git hash ${GIT_HEAD}: FOUND in binary"
    else
        echo "WARNING: git hash ${GIT_HEAD} not found in binary strings" >&2
        strings "$path" | grep -E 'main@|nogit' | head -3 || true
    fi
    echo ""
}

echo "=========================================="
echo "  Anti-stale verification"
echo "=========================================="
verify_binary "$ARTIFACTS/AU/${PLUGIN}.component/Contents/MacOS/${PLUGIN}" "AU"
verify_binary "$ARTIFACTS/VST3/${PLUGIN}.vst3/Contents/MacOS/${PLUGIN}" "VST3"
if [[ -f "$ARTIFACTS/AAX/${PLUGIN}.aaxplugin/Contents/MacOS/${PLUGIN}" ]]; then
    verify_binary "$ARTIFACTS/AAX/${PLUGIN}.aaxplugin/Contents/MacOS/${PLUGIN}" "AAX"
fi

SIGNED_PKG="installer/${PLUGIN}-0.3.1-macOS-signed.pkg"
if [[ -f "$SIGNED_PKG" ]]; then
    echo "--- Installer ---"
    ls -la "$SIGNED_PKG"
    shasum -a 256 "$SIGNED_PKG"
    xcrun stapler validate "$SIGNED_PKG" 2>&1 || true
    spctl -a -vvv -t install "$SIGNED_PKG" 2>&1 | head -5 || true
fi

echo ""
echo "DEV beta release complete. Confirm header shows main@${GIT_HEAD} in Standalone/AU."

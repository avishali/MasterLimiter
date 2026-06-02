#!/usr/bin/env bash

# Release Build Script for MasterLimiter
# Builds universal binaries (arm64 + x86_64) with optimizations for distribution.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# shellcheck source=/dev/null
source "$SCRIPT_DIR/source_repo_env.sh"

BUILD_DIR="${MASTERLIMITER_RELEASE_BUILD_DIR:-build-release}"
CONFIG="Release"

PLUGIN_NAME="MasterLimiter"
PLUGIN_VERSION="0.3.0"
COMPANY_NAME="MelechDSP"

echo "=========================================="
echo "  MasterLimiter Release Build"
echo "=========================================="
echo "  Version: $PLUGIN_VERSION"
echo "  Company: $COMPANY_NAME"
echo "  Configuration: $CONFIG"
echo "  Universal Binary: arm64 + x86_64"
echo "=========================================="
echo ""

if [ -z "${JUCE_PATH:-}" ]; then
    echo "Error: JUCE_PATH is not set."
    echo "  export JUCE_PATH=/path/to/JUCE"
    echo "  or add JUCE_PATH=... to scripts/.env"
    exit 1
fi

if [ ! -d "$JUCE_PATH" ]; then
    echo "Error: JUCE_PATH directory does not exist: $JUCE_PATH"
    exit 1
fi

if [ -z "${MELECHDSP_HQ_ROOT:-}" ]; then
    MELECHDSP_HQ_ROOT="$PROJECT_ROOT/../melechdsp-hq"
fi

if [ ! -d "$MELECHDSP_HQ_ROOT" ]; then
    echo "Error: MELECHDSP_HQ_ROOT directory does not exist: $MELECHDSP_HQ_ROOT"
    exit 1
fi

echo "JUCE path verified: $JUCE_PATH"
echo "MelechDSP HQ root: $MELECHDSP_HQ_ROOT"
if [ -n "${AAX_SDK_PATH:-}" ]; then
    echo "AAX SDK path: $AAX_SDK_PATH"
else
    echo "AAX SDK path: not set (AAX target disabled)"
fi
echo ""

if [ -d "$BUILD_DIR" ]; then
    echo "Cleaning previous release build..."
    rm -rf "$BUILD_DIR"
fi

echo "Configuring CMake for Release build..."
echo ""
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DJUCE_PATH="$JUCE_PATH" \
    -DMELECHDSP_HQ_ROOT="$MELECHDSP_HQ_ROOT" \
    -DAAX_SDK_PATH="${AAX_SDK_PATH:-}" \
    -DPLUGIN_DEV_MODE=OFF \
    -DMASTERLIMITER_COPY_AFTER_BUILD=OFF \
    -DUniversalBinary=ON \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="10.13"

echo ""
echo "CMake configuration complete"
echo ""

echo "Building release binaries..."
echo "This may take several minutes."
echo ""

NUM_CORES="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
cmake --build "$BUILD_DIR" --config "$CONFIG" -j"$NUM_CORES"

echo ""
echo "Build complete"
echo ""

ARTIFACTS_DIR="$BUILD_DIR/${PLUGIN_NAME}_artefacts/$CONFIG"
echo "=========================================="
echo "  Built Artifacts"
echo "=========================================="
echo ""

if [ -d "$ARTIFACTS_DIR" ]; then
    if [ -d "$ARTIFACTS_DIR/AU/${PLUGIN_NAME}.component" ]; then
        echo "AU:"
        echo "  $ARTIFACTS_DIR/AU/${PLUGIN_NAME}.component"
        if command -v lipo >/dev/null 2>&1; then
            lipo -info "$ARTIFACTS_DIR/AU/${PLUGIN_NAME}.component/Contents/MacOS/${PLUGIN_NAME}" 2>/dev/null || true
        fi
        echo ""
    fi

    if [ -d "$ARTIFACTS_DIR/VST3/${PLUGIN_NAME}.vst3" ]; then
        echo "VST3:"
        echo "  $ARTIFACTS_DIR/VST3/${PLUGIN_NAME}.vst3"
        if command -v lipo >/dev/null 2>&1; then
            lipo -info "$ARTIFACTS_DIR/VST3/${PLUGIN_NAME}.vst3/Contents/MacOS/${PLUGIN_NAME}" 2>/dev/null || true
        fi
        echo ""
    fi

    if [ -d "$ARTIFACTS_DIR/AAX/${PLUGIN_NAME}.aaxplugin" ]; then
        echo "AAX:"
        echo "  $ARTIFACTS_DIR/AAX/${PLUGIN_NAME}.aaxplugin"
        echo ""
    fi

    if [ -d "$ARTIFACTS_DIR/Standalone/${PLUGIN_NAME}.app" ]; then
        echo "Standalone:"
        echo "  $ARTIFACTS_DIR/Standalone/${PLUGIN_NAME}.app"
        if command -v lipo >/dev/null 2>&1; then
            lipo -info "$ARTIFACTS_DIR/Standalone/${PLUGIN_NAME}.app/Contents/MacOS/${PLUGIN_NAME}" 2>/dev/null || true
        fi
        echo ""
    fi
else
    echo "Warning: artifacts directory not found: $ARTIFACTS_DIR"
fi

echo "=========================================="
echo ""
echo "Release build successful"
echo ""
echo "Next steps:"
echo "  1. Test all plugin formats."
echo "  2. AAX, if built: ./scripts/wraptool_sign_aax.sh \\"
echo "       $BUILD_DIR/${PLUGIN_NAME}_artefacts/Release/AAX/${PLUGIN_NAME}.aaxplugin/Contents/MacOS/${PLUGIN_NAME}"
echo "  3. Then: SIGN_AND_NOTARIZE_SKIP_AAX=1 ./scripts/sign_and_notarize.sh"
echo "  4. Full pipeline: ./scripts/release_macos.sh"
echo "  5. Unsigned installer/archive only: ./scripts/create_installer.sh"
echo ""

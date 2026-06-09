#!/bin/bash
# Configure and build MasterLimiter (preset: debug, Ninja)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

if [ -z "$JUCE_PATH" ]; then
    echo "Error: JUCE_PATH environment variable is not set."
    echo "  export JUCE_PATH=/path/to/JUCE"
    exit 1
fi

if [ ! -d "$JUCE_PATH" ]; then
    echo "Error: JUCE_PATH directory does not exist: $JUCE_PATH"
    exit 1
fi

echo "Configuring MasterLimiter (preset: debug)..."
cmake --preset debug -DMASTERLIMITER_COPY_AFTER_BUILD=OFF

echo "Building..."
cmake --build build-debug -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo "Build complete."
echo "System install: run ./scripts/install_system.sh"
echo "Or set MASTERLIMITER_INSTALL_SYSTEM=1 before running this script to install after build."

if [ "${MASTERLIMITER_INSTALL_SYSTEM:-0}" = "1" ]; then
    "$SCRIPT_DIR/install_system.sh"
fi

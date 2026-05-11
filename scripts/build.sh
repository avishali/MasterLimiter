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
cmake --preset debug

echo "Building..."
cmake --build build-debug -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo "Build complete."

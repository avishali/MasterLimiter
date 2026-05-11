#!/bin/bash
# Remove local CMake build directories for MasterLimiter

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "Cleaning build directories..."

BUILD_DIRS=(
    "build"
    "build-debug"
    "build-xcode"
    "build-release-macos-universal"
    "build-release-windows-x64"
    "build-release-windows-arm64"
)

REMOVED=0

for dir in "${BUILD_DIRS[@]}"; do
    if [ -d "../$dir" ]; then
        echo "  Removing $dir/"
        rm -rf "../$dir"
        REMOVED=1
    fi
done

for dir in ../build-*; do
    if [ -d "$dir" ]; then
        echo "  Removing $(basename "$dir")/"
        rm -rf "$dir"
        REMOVED=1
    fi
done

if [ $REMOVED -eq 0 ]; then
    echo "No build directories found to remove."
else
    echo "Build cleanup complete!"
fi

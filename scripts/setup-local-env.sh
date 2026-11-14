#!/bin/bash
# Setup environment variables untuk lokal build Qt 6.12 + Android NDK

set -e

# Detect NDK version
ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}"
NDK_VERSION=$(ls -d "$ANDROID_SDK_ROOT/ndk"/* 2>/dev/null | xargs basename -a | sort -V | tail -1)
if [ -z "$NDK_VERSION" ]; then
    echo "ERROR: No NDK found in $ANDROID_SDK_ROOT/ndk/"
    exit 1
fi
echo "Found NDK: $NDK_VERSION"

# Export for cmake
export ANDROID_SDK_ROOT
export ANDROID_NDK_ROOT="$ANDROID_SDK_ROOT/ndk/$NDK_VERSION"
export QT_HOST_PATH="/usr"  # System Qt 6.12 from pacman
export PATH="$ANDROID_SDK_ROOT/platform-tools:$PATH"

# CMake helper (find system Qt 6.12)
export Qt6_DIR="/usr/lib/cmake/Qt6"

echo "Environment for lokal build:"
echo "  ANDROID_SDK_ROOT=$ANDROID_SDK_ROOT"
echo "  ANDROID_NDK_ROOT=$ANDROID_NDK_ROOT"
echo "  QT_HOST_PATH=$QT_HOST_PATH"
echo "  Qt6_DIR=$Qt6_DIR"
echo ""
echo "To use, run: source scripts/setup-local-env.sh"

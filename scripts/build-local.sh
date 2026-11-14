#!/bin/bash
# Local build wrapper: setup env + build APK

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Setup env
ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}"
NDK_VERSION=$(ls -d "$ANDROID_SDK_ROOT/ndk"/* 2>/dev/null | xargs basename -a | sort -V | tail -1)
if [ -z "$NDK_VERSION" ]; then
    echo "ERROR: No NDK found in $ANDROID_SDK_ROOT/ndk/"
    exit 1
fi

ANDROID_NDK_ROOT="$ANDROID_SDK_ROOT/ndk/$NDK_VERSION"

# Use Qt 6.12 from aqt (lokal download)
QT_INSTALL_DIR="$PROJECT_DIR/6.12.0"
if [ -d "$QT_INSTALL_DIR/gcc_64" ]; then
    QT_HOST_PATH="$QT_INSTALL_DIR/gcc_64"
else
    # Fallback system Qt (6.11)
    QT_HOST_PATH="/usr"
fi
Qt6_DIR="$QT_HOST_PATH/lib/cmake/Qt6"

echo "=== Lokal build setup ==="
echo "Android SDK: $ANDROID_SDK_ROOT"
echo "Android NDK: $ANDROID_NDK_ROOT"
echo "Qt host: $QT_HOST_PATH"
echo ""

# Build deps (OpenSSL, MariaDB)
if [ ! -d "$PROJECT_DIR/deps/out" ]; then
    echo "=== Building native deps ==="
    cd "$PROJECT_DIR"
    ANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" bash "$SCRIPT_DIR/build-deps.sh"
fi

# Build Rust ssh_bridge
echo "=== Building ssh_bridge ==="
cd "$PROJECT_DIR"
ANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" bash "$SCRIPT_DIR/build-rust.sh"

# Configure & build APK
echo "=== Configuring CMake ==="
cd "$PROJECT_DIR"
cmake -S . -B build-local -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="/usr/lib/cmake" \
    -DQt6_DIR="$Qt6_DIR" \
    -DQT_ANDROID_SIGN_APK=ON \
    -DNAWI_SSL_LIBS_DIR="$PROJECT_DIR/deps/ssl" \
    -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
    -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT"

echo "=== Building APK ==="
cmake --build build-local --target apk --parallel

APK=$(find build-local -name '*signed*.apk' -o -name '*release*.apk' | head -n1)
if [ -z "$APK" ]; then
    echo "ERROR: APK not found"
    exit 1
fi

mkdir -p out
cp "$APK" out/NawiDBMan-arm64-local.apk
echo ""
echo "✓ Build complete: out/NawiDBMan-arm64-local.apk"

#!/usr/bin/env bash
# Local APK build, mirrors .github/workflows/android-build.yml (build-android job).
# Needs: Qt 6.12 via aqt in ./6.12.0 (gcc_64 + android_arm64_v8a), NDK 27.2, rustup target aarch64-linux-android.
# Usage: scripts/build-local.sh [--install]   (--install = adb install -r + launch)
set -euo pipefail
cd "$(dirname "$0")/.."

export ANDROID_SDK_ROOT=${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}
export ANDROID_NDK_ROOT=${ANDROID_NDK_ROOT:-$ANDROID_SDK_ROOT/ndk/27.2.12479018}
QT=${QT:-$PWD/6.12.0}
KEYS=${KEYS:-$HOME/.android-keys}
[ -x "$QT/android_arm64_v8a/bin/qt-cmake" ] || { echo "Qt Android missing: aqt install-qt all_os android 6.12.0 android_arm64_v8a"; exit 1; }
[ -d "$ANDROID_NDK_ROOT" ] || { echo "NDK missing: sdkmanager 'ndk;27.2.12479018'"; exit 1; }

# Native deps + Rust: scripts skip work when outputs exist / are up to date.
./scripts/build-deps.sh
./scripts/build-rust.sh

mkdir -p deps/ssl
for l in libssl_3 libcrypto_3; do
  [ -f deps/ssl/$l.so ] || curl -fsSL -o deps/ssl/$l.so https://github.com/KDAB/android_openssl/raw/master/ssl_3/arm64-v8a/$l.so
done

# Same keystore as CI secrets, so local APK updates CI-installed app without uninstall.
export QT_ANDROID_KEYSTORE_PATH=$KEYS/nawidbman.jks QT_ANDROID_KEYSTORE_ALIAS=nawidbman
export QT_ANDROID_KEYSTORE_STORE_PASS=$(cat "$KEYS/nawidbman.pass") QT_ANDROID_KEYSTORE_KEY_PASS=$(cat "$KEYS/nawidbman.pass")

[ -f build-local/CMakeCache.txt ] || "$QT/android_arm64_v8a/bin/qt-cmake" -S . -B build-local -GNinja \
  -DCMAKE_BUILD_TYPE=Release -DQT_HOST_PATH="$QT/gcc_64" -DQT_ANDROID_SIGN_APK=ON \
  -DNAWI_SSL_LIBS_DIR="$PWD/deps/ssl" -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT"
cmake --build build-local --target apk --parallel

mkdir -p out
cp "$(find build-local -name '*signed*.apk' | head -n1)" out/NawiDBMan-arm64-local.apk
echo "APK: out/NawiDBMan-arm64-local.apk"

if [ "${1:-}" = "--install" ]; then
  adb install -r out/NawiDBMan-arm64-local.apk
  adb shell am start -n com.nawi.dbman/org.qtproject.qt.android.bindings.QtActivity
fi

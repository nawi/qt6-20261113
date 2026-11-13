#!/usr/bin/env bash
# Cross-compile OpenSSL + MariaDB Connector/C (static, arm64-v8a) into $OUT.
# Env: ANDROID_NDK_ROOT (required), OUT (default: deps/out), API (default: 31)
set -euo pipefail

OPENSSL_VER=3.5.9      # LTS branch
MARIADB_C_VER=3.4.11
API=${API:-31}
OUT=${OUT:-$(pwd)/deps/out}
SRC=${SRC:-$(pwd)/deps/src}
: "${ANDROID_NDK_ROOT:?set ANDROID_NDK_ROOT}"

TC=$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64
export PATH=$TC/bin:$PATH
JOBS=$(nproc)
mkdir -p "$OUT" "$SRC"

if [ ! -f "$OUT/lib/libssl.a" ]; then
  cd "$SRC"
  curl -fsSL "https://github.com/openssl/openssl/releases/download/openssl-$OPENSSL_VER/openssl-$OPENSSL_VER.tar.gz" | tar xz
  cd "openssl-$OPENSSL_VER"
  ./Configure android-arm64 -D__ANDROID_API__=$API no-shared no-tests no-apps no-docs no-legacy \
    --prefix="$OUT" --libdir=lib
  make -j"$JOBS" build_libs
  make install_dev
fi

if [ ! -f "$OUT/lib/mariadb/libmariadbclient.a" ]; then
  cd "$SRC"
  curl -fsSL "https://github.com/mariadb-corporation/mariadb-connector-c/archive/refs/tags/v$MARIADB_C_VER.tar.gz" | tar xz
  cmake -S "mariadb-connector-c-$MARIADB_C_VER" -B mariadbc-build -GNinja \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-$API \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_INSTALL_PREFIX="$OUT" \
    -DWITH_SSL=OPENSSL -DOPENSSL_ROOT_DIR="$OUT" -DOPENSSL_USE_STATIC_LIBS=ON \
    -DOPENSSL_INCLUDE_DIR="$OUT/include" -DOPENSSL_SSL_LIBRARY="$OUT/lib/libssl.a" \
    -DOPENSSL_CRYPTO_LIBRARY="$OUT/lib/libcrypto.a" \
    -DWITH_EXTERNAL_ZLIB=ON -DWITH_CURL=OFF -DWITH_UNIT_TESTS=OFF -DWITH_MYSQLCOMPAT=OFF \
    -DCLIENT_PLUGIN_DIALOG=STATIC -DCLIENT_PLUGIN_SHA256_PASSWORD=STATIC \
    -DCLIENT_PLUGIN_CACHING_SHA2_PASSWORD=STATIC -DCLIENT_PLUGIN_MYSQL_CLEAR_PASSWORD=STATIC \
    -DCLIENT_PLUGIN_MYSQL_OLD_PASSWORD=STATIC -DCLIENT_PLUGIN_CLIENT_ED25519=OFF \
    -DCLIENT_PLUGIN_AUTH_GSSAPI_CLIENT=OFF -DCLIENT_PLUGIN_REMOTE_IO=OFF \
    -DCLIENT_PLUGIN_PVIO_NPIPE=OFF -DCLIENT_PLUGIN_PVIO_SHMEM=OFF
  cmake --build mariadbc-build --target install
fi
echo "deps ready in $OUT"; find "$OUT" -name '*.a'

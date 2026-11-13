#!/usr/bin/env bash
# Build rust/ssh_bridge staticlib for android arm64. Env: ANDROID_NDK_ROOT, API (default 31)
set -euo pipefail
: "${ANDROID_NDK_ROOT:?}"
API=${API:-31}
BIN=$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin
export CC_aarch64_linux_android=$BIN/aarch64-linux-android$API-clang
export AR_aarch64_linux_android=$BIN/llvm-ar
export CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER=$CC_aarch64_linux_android
rustup target add aarch64-linux-android
cargo build --release --target aarch64-linux-android --manifest-path rust/ssh_bridge/Cargo.toml
ls -l rust/ssh_bridge/target/aarch64-linux-android/release/libssh_bridge.a

cmake_minimum_required(VERSION 3.21)
project(NawiDBMan VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Quick QuickControls2 Test)
qt_standard_project_setup(REQUIRES 6.8)

# --- native deps: MariaDB Connector/C + OpenSSL + Rust ssh_bridge ---------------
if(ANDROID)
    set(DEPS_DIR "${CMAKE_SOURCE_DIR}/deps/out" CACHE PATH "scripts/build-deps.sh output")
    set(SSH_BRIDGE_LIB "${CMAKE_SOURCE_DIR}/rust/ssh_bridge/target/aarch64-linux-android/release/libssh_bridge.a"
        CACHE FILEPATH "scripts/build-rust.sh output")
    set(NATIVE_INCLUDES "${DEPS_DIR}/include/mariadb" "${DEPS_DIR}/include")
    set(NATIVE_LIBS "${DEPS_DIR}/lib/mariadb/libmariadbclient.a" "${DEPS_DIR}/lib/libssl.a"
        "${DEPS_DIR}/lib/libcrypto.a" "${SSH_BRIDGE_LIB}" z dl log)
else()
    # Desktop (unit tests): system libmariadb + OpenSSL, host-built ssh_bridge
    find_package(OpenSSL REQUIRED)
    find_path(MARIADB_INC mysql.h PATH_SUFFIXES mariadb mysql REQUIRED)
    find_library(MARIADB_LIB NAMES mariadb REQUIRED)
    set(SSH_BRIDGE_LIB "${CMAKE_SOURCE_DIR}/rust/ssh_bridge/target/release/libssh_bridge.a"
        CACHE FILEPATH "cargo build --release output")
    set(NATIVE_INCLUDES "${MARIADB_INC}")
    set(NATIVE_LIBS "${MARIADB_LIB}" OpenSSL::SSL OpenSSL::Crypto "${SSH_BRIDGE_LIB}" dl pthread m)
endif()

add_library(core STATIC src/Probe.cpp)
target_include_directories(core PUBLIC src ${NATIVE_INCLUDES})
target_link_libraries(core PUBLIC Qt6::Core ${NATIVE_LIBS})

# --- app -----------------------------------------------------------------------
qt_add_executable(NawiDBMan src/main.cpp)
qt_add_qml_module(NawiDBMan
    URI NawiDBMan
    VERSION 1.0
    QML_FILES qml/Main.qml
)
target_link_libraries(NawiDBMan PRIVATE Qt6::Quick Qt6::QuickControls2 core)

set_target_properties(NawiDBMan PROPERTIES
    QT_ANDROID_PACKAGE_NAME "com.nawi.dbman"
    QT_ANDROID_APP_NAME "NawiDBMan"
    QT_ANDROID_MIN_SDK_VERSION 31
    QT_ANDROID_TARGET_SDK_VERSION 35
    QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
    QT_ANDROID_VERSION_CODE 1
    QT_ANDROID_ABIS "arm64-v8a"
)

# --- tests (desktop only) ----------------------------------------------------------
if(NOT ANDROID)
    enable_testing()
    qt_add_executable(tst_probe tests/tst_probe.cpp)
    target_link_libraries(tst_probe PRIVATE Qt6::Test core)
    add_test(NAME probe COMMAND tst_probe)
endif()

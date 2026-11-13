cmake_minimum_required(VERSION 3.21)
project(NawiDBMan VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Sql Concurrent Quick QuickControls2 Test)
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

add_library(core STATIC src/Probe.cpp src/Db.cpp src/Vault.cpp src/AppSettings.cpp src/ConnectionStore.cpp src/Platform.cpp)
target_include_directories(core PUBLIC src ${NATIVE_INCLUDES})
target_link_libraries(core PUBLIC Qt6::Core Qt6::Sql Qt6::Concurrent Qt6::Gui ${NATIVE_LIBS})
target_compile_definitions(core PUBLIC APP_VERSION="${PROJECT_VERSION}")

# --- app -----------------------------------------------------------------------
qt_add_executable(NawiDBMan src/main.cpp)
set(QML_SINGLETONS qml/Theme.qml)
set_source_files_properties(${QML_SINGLETONS} PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(NawiDBMan
    URI NawiDBMan
    VERSION 1.0
    QML_FILES ${QML_SINGLETONS}
        qml/Main.qml qml/Icon.qml qml/NavBar.qml qml/NavButton.qml qml/Group.qml qml/Cell.qml
        qml/FieldCell.qml qml/Segmented.qml qml/IOSSwitch.qml qml/PrimaryButton.qml qml/ActionSheet.qml
        qml/Toast.qml qml/FormFlickable.qml qml/Onboarding.qml qml/ConnectionList.qml qml/ConnectionForm.qml qml/SettingsPage.qml
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
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
)

if(NOT ANDROID)
    # build dir already holds the QML module dir named NawiDBMan
    set_target_properties(NawiDBMan PROPERTIES OUTPUT_NAME nawidbman)
endif()

# --- tests (desktop only) ----------------------------------------------------------
if(NOT ANDROID)
    enable_testing()
    foreach(t probe storage)
        qt_add_executable(tst_${t} tests/tst_${t}.cpp)
        target_link_libraries(tst_${t} PRIVATE Qt6::Test core)
        add_test(NAME ${t} COMMAND tst_${t})
    endforeach()
endif()

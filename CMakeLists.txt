cmake_minimum_required(VERSION 3.21)
project(NawiDBMan VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(NawiDBMan src/main.cpp)

qt_add_qml_module(NawiDBMan
    URI NawiDBMan
    VERSION 1.0
    QML_FILES qml/Main.qml
)

target_link_libraries(NawiDBMan PRIVATE Qt6::Quick Qt6::QuickControls2)

set_target_properties(NawiDBMan PROPERTIES
    QT_ANDROID_PACKAGE_NAME "com.nawi.dbman"
    QT_ANDROID_APP_NAME "NawiDBMan"
    QT_ANDROID_MIN_SDK_VERSION 31
    QT_ANDROID_TARGET_SDK_VERSION 35
    QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
    QT_ANDROID_VERSION_CODE 1
    QT_ANDROID_ABIS "arm64-v8a"
)

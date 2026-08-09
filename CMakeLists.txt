cmake_minimum_required(VERSION 3.21)

project(MusicVisualizer
    VERSION 0.1.0
    DESCRIPTION "Layered, frequency-reactive music visualizer with ffmpeg video export"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(MV_BUILD_TESTS "Build unit tests for the mvcore library" ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Multimedia
    Concurrent
)

qt_standard_project_setup(REQUIRES 6.8)

add_subdirectory(src/core)
add_subdirectory(src/app)

if(MV_BUILD_TESTS)
    find_package(Qt6 6.8 QUIET COMPONENTS Test)
    if(Qt6Test_FOUND)
        enable_testing()
        add_subdirectory(tests)
    else()
        message(STATUS "Qt6::Test not found - skipping unit tests")
    endif()
endif()

message(STATUS "MusicVisualizer ${PROJECT_VERSION}")
message(STATUS "  Qt        : ${Qt6_VERSION} (${QT6_INSTALL_PREFIX})")
message(STATUS "  Compiler  : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

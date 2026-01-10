# =============================================================================
# ESP32-S3 M5Stack Dial Remote Controller - Project Root CMakeLists.txt
# =============================================================================

cmake_minimum_required(VERSION 3.16)

# Shared default variables
set(DEFAULT_APP_TYPE "m5dial_remote_controller" CACHE STRING "Default app type")
set(DEFAULT_BUILD_TYPE "Release" CACHE STRING "Default build type")
set(DEFAULT_SOURCE_FILE "main/main.cpp" CACHE STRING "Default source file")

# Build parameter validation (expect build_app.sh)
if(NOT DEFINED APP_TYPE)
    message(FATAL_ERROR
        "APP_TYPE not defined. Please use build_app.sh to build this project.\n"
        "Example: ./scripts/build_app.sh ${DEFAULT_APP_TYPE} ${DEFAULT_BUILD_TYPE}\n"
        "Use './scripts/build_app.sh list' to see available app types."
    )
endif()

if(NOT DEFINED BUILD_TYPE)
    message(FATAL_ERROR
        "BUILD_TYPE not defined. Please use build_app.sh to build this project.\n"
        "Example: ./scripts/build_app.sh ${DEFAULT_APP_TYPE} ${DEFAULT_BUILD_TYPE}\n"
        "Use './scripts/build_app.sh list' to see available app types."
    )
endif()

message(STATUS "Building app type: ${APP_TYPE}")
message(STATUS "Build type: ${BUILD_TYPE}")

set(APP_TYPE "${APP_TYPE}" CACHE STRING "App type to build" FORCE)
set(BUILD_TYPE "${BUILD_TYPE}" CACHE STRING "Build type (Debug/Release)" FORCE)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_m5dial_remote_controller_${APP_TYPE}_app)

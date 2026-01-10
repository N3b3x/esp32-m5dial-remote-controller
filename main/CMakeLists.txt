# =============================================================================
# ESP32-S3 M5Dial Remote Controller - Main Component CMakeLists.txt
# =============================================================================

# During ESP-IDF's "requirements" phase, top-level cache variables may not be
# populated yet. Ensure sane defaults exist for local validation logic.
if(NOT DEFINED DEFAULT_APP_TYPE)
    set(DEFAULT_APP_TYPE "m5dial_remote_controller" CACHE STRING "Default app type")
endif()
if(NOT DEFINED DEFAULT_SOURCE_FILE)
    set(DEFAULT_SOURCE_FILE "main/main.cpp" CACHE STRING "Default source file")
endif()

if(NOT DEFINED APP_TYPE)
    if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
        set(APP_TYPE "${DEFAULT_APP_TYPE}")
        message(STATUS "APP_TYPE not defined during requirements phase, using default: ${APP_TYPE}")
    else()
        message(FATAL_ERROR
            "APP_TYPE not defined. Please use build_app.sh to build this project.\n"
            "Example: ./scripts/build_app.sh ${DEFAULT_APP_TYPE} Release\n"
        )
    endif()
endif()

if(NOT DEFINED APP_SOURCE_FILE)
    if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
        set(APP_SOURCE_FILE "${DEFAULT_SOURCE_FILE}")
        message(STATUS "APP_SOURCE_FILE not defined during requirements phase, using default: ${APP_SOURCE_FILE}")
    else()
        message(FATAL_ERROR
            "APP_SOURCE_FILE not defined. Please use build_app.sh to build this project.\n"
        )
    endif()
endif()

message(STATUS "Building app type: ${APP_TYPE}")
message(STATUS "Using source file: ${APP_SOURCE_FILE}")

set(MAIN_INCLUDE_DIRS
    "."
    "${CMAKE_CURRENT_SOURCE_DIR}/protocol"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/../components/EC11_Encoder/inc"
)

set(MAIN_REQUIRES
    driver
    esp_timer
    freertos
    nvs_flash
    esp_wifi
    esp_netif
    esp_event
    M5Unified
    M5GFX
    EC11_Encoder
)

string(REPLACE "main/" "" MAIN_SOURCE_FILE "${APP_SOURCE_FILE}")
get_filename_component(MAIN_SOURCE_FILE "${MAIN_SOURCE_FILE}" NAME)
set(COMPONENT_SRCS "${MAIN_SOURCE_FILE}")

if(APP_TYPE STREQUAL "m5dial_remote_controller")
    list(APPEND COMPONENT_SRCS
        "settings.cpp"
        "protocol/espnow_protocol.cpp"
        "ui/ui_controller.cpp"
    )
endif()

idf_component_register(
    SRCS ${COMPONENT_SRCS}
    INCLUDE_DIRS ${MAIN_INCLUDE_DIRS}
    REQUIRES ${MAIN_REQUIRES}
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)

if(BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${COMPONENT_LIB} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -O0
        -g3
        -DDEBUG
    )
else()
    target_compile_options(${COMPONENT_LIB} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -O2
        -g
        -DNDEBUG
    )
endif()

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    "EXAMPLE_TYPE_${APP_TYPE}=1"
)

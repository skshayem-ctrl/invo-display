cmake_minimum_required(VERSION 3.16)

# esp_wifi_remote Kconfig uses $ESP_IDF_VERSION to select the right wifi Kconfig file.
# IDF doesn't export this env var itself; derive it from version.cmake before kconfgen runs.
if(DEFINED ENV{IDF_PATH})
    include($ENV{IDF_PATH}/tools/cmake/version.cmake)
    set(ENV{ESP_IDF_VERSION} "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}")
endif()

# Single source of truth for app version — read from version.json.
# This overrides CONFIG_APP_PROJECT_VER in sdkconfig, so you only ever
# need to bump version.json; no sdkconfig edits required.
file(READ "${CMAKE_SOURCE_DIR}/version.json" _VER_JSON)
string(JSON PROJECT_VER GET "${_VER_JSON}" "version")
message(STATUS "App version: ${PROJECT_VER}")

set(EXTRA_COMPONENT_DIRS "src")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(invo-esp32)

# Generate .hex alongside .bin after every build
add_custom_command(
    TARGET ${CMAKE_PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.elf
            ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.hex
    COMMENT "Generating ${CMAKE_PROJECT_NAME}.hex"
)

# Suppress LVGL warnings that flood the build log
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE
    -Wno-attributes
    -Wno-unused-function
)

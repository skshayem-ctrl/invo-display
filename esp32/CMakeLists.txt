cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(invo-esp32)

# Suppress LVGL warnings that flood the build log
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE
    -Wno-attributes
    -Wno-unused-function
)

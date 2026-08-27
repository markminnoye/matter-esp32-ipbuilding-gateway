cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "0.1.2-e3")
set(PROJECT_VER_NUMBER 4)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(ipbuilding_gateway)

# Only install-specific devices.json goes to LittleFS — never the git example template.
set(LITTLEFS_FLASH_DIR "${CMAKE_BINARY_DIR}/littlefs_flash")
file(MAKE_DIRECTORY "${LITTLEFS_FLASH_DIR}")

if(EXISTS "${CMAKE_SOURCE_DIR}/config/devices.json")
    file(COPY "${CMAKE_SOURCE_DIR}/config/devices.json" DESTINATION "${LITTLEFS_FLASH_DIR}")
else()
    message(FATAL_ERROR
        "config/devices.json is required to build the LittleFS image.\n"
        "  cp config/examples/devices.example.json config/devices.json\n"
        "Then edit config/devices.json for your installation.")
endif()

littlefs_create_partition_image(littlefs "${LITTLEFS_FLASH_DIR}" FLASH_IN_PROJECT)

idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++17;-Os;-DCHIP_HAVE_CONFIG_H" APPEND)
idf_build_set_property(C_COMPILE_OPTIONS "-Os" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-format-nonliteral;-Wno-format-security" APPEND)

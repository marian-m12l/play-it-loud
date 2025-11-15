cmake_minimum_required(VERSION 3.13)

# Set the Pico board
set(PICO_BOARD pico_w)

# Include the Pico SDK. Must be before project()
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(cmake_test C CXX ASM)

# Set the C/C++ standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# Set playback rate
if(NOT ENABLE_DOUBLE_SPEED)
    set(ENABLE_DOUBLE_SPEED 0)
endif()
add_compile_definitions(ENABLE_DOUBLE_SPEED=${ENABLE_DOUBLE_SPEED})

add_subdirectory(gb_serial)
add_subdirectory(jpegdec)
add_subdirectory(player_sine)
add_subdirectory(player_static)
add_subdirectory(player_bt)
add_subdirectory(player_usb)
add_subdirectory(rom)
add_subdirectory(rom_new)
add_subdirectory(rom_new2)

unset(ENABLE_DOUBLE_SPEED CACHE)

set(PROJECT_NAME gb_serial)
project(${PROJECT_NAME} C CXX ASM)

add_library(${PROJECT_NAME}
    gb_serial.c
    downsampler.c
    encoder.c
    gfx.c
)

# Add PIO source files
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/spi.pio)

target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    hardware_pio
    hardware_dma
    hardware_irq
)

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")


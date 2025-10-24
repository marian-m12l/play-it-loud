set(PROJECT_NAME player_usb)
project(${PROJECT_NAME} C CXX ASM)

add_executable(${PROJECT_NAME}
    main.c
    usb_audio.c
    usb_descriptors.c
)

target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    gb_serial
    tinyusb_device
    tinyusb_board
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}   # For tinyusb config
)

# UART on GPIO16/17
target_compile_definitions(${PROJECT_NAME} PRIVATE
	PICO_DEFAULT_UART_TX_PIN=16
	PICO_DEFAULT_UART_RX_PIN=17
)

pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)

pico_add_extra_outputs(${PROJECT_NAME})

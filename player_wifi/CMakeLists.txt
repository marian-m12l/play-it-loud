set(PROJECT_NAME player_wifi)
project(${PROJECT_NAME} C CXX ASM)

add_executable(${PROJECT_NAME}
    main.c
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIFI_SSID=\"Airbox_1564\"
    WIFI_PASSWORD=\"F9QF9MYHHQ1\"
)

target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    pico_cyw43_arch_lwip_poll
    pico_lwip_http
    pico_lwip_mdns
    gb_serial
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}   # For lwip opts
    ${PICO_LWIP_CONTRIB_PATH}/apps/httpd
)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

pico_add_extra_outputs(${PROJECT_NAME})

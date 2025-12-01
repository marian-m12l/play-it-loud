set(PROJECT_NAME player_bt)
project(${PROJECT_NAME} C CXX ASM)

add_executable(${PROJECT_NAME}
    main.c
    bt.c
    sdp.c
    a2dp.c
    avrcp.c
    btstack_audio_pico_gblink.c
    ${PICO_SDK_PATH}/lib/btstack/src/btstack_audio.c
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    BT_PIN="0000"
    BT_NAME="Play it loud!"
    ENABLE_COVER_ART=0
)

target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    pico_btstack_sbc_decoder
    pico_btstack_classic
    pico_btstack_cyw43
    pico_cyw43_arch_none
    gb_serial
    jpegdec
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}   # For btstack config
)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

pico_add_extra_outputs(${PROJECT_NAME})

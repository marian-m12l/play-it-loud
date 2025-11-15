set(PROJECT_NAME rom_new)
project(${PROJECT_NAME} ASM)

if(ENABLE_DOUBLE_SPEED EQUAL 1)
  set(ROM_TITLE "PLAY IT NEWER!")
else()
  set(ROM_TITLE "PLAY IT NEW!")
endif()

add_custom_target(generate_gfx_new DEPENDS logo.2bpp)
add_custom_command(
  OUTPUT logo.2bpp
  COMMAND rgbgfx ${CMAKE_CURRENT_LIST_DIR}/logo.png -o logo.2bpp
  DEPENDS ${CMAKE_CURRENT_LIST_DIR}/logo.png
  VERBATIM)

add_custom_target(generate_obj_new DEPENDS ${PROJECT_NAME}.o generate_gfx_new)
add_custom_command(
  OUTPUT ${PROJECT_NAME}.o
  COMMAND rgbasm -o ${PROJECT_NAME}.o -I ${CMAKE_CURRENT_LIST_DIR} -D __ENABLE_DOUBLE_SPEED__=${ENABLE_DOUBLE_SPEED} -D __DEBUG__=$<CONFIG:Debug> ${CMAKE_CURRENT_LIST_DIR}/main.asm
  DEPENDS ${CMAKE_CURRENT_LIST_DIR}/main.asm logo.2bpp
  VERBATIM)

add_custom_target(${PROJECT_NAME}.gb ALL DEPENDS generate_obj_new)
add_custom_command(
  TARGET ${PROJECT_NAME}.gb
  POST_BUILD
  COMMAND rgblink -o ${PROJECT_NAME}.gb -m ${PROJECT_NAME}.map -n ${PROJECT_NAME}.sym ${PROJECT_NAME}.o
  COMMAND rgbfix -v -m 0 -c -p 0 -t "${ROM_TITLE}" ${PROJECT_NAME}.gb
  BYPRODUCTS ${PROJECT_NAME}.map ${PROJECT_NAME}.sym
  VERBATIM)

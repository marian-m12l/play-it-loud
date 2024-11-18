set(PROJECT_NAME rom)
project(${PROJECT_NAME} ASM)

add_custom_target(generate_obj DEPENDS ${PROJECT_NAME}.o)
add_custom_command(
  OUTPUT ${PROJECT_NAME}.o
  COMMAND rgbasm -o ${PROJECT_NAME}.o -I ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/main.asm
  DEPENDS ${CMAKE_CURRENT_LIST_DIR}/main.asm
  VERBATIM)

add_custom_target(${PROJECT_NAME}.gb ALL DEPENDS generate_obj)
add_custom_command(
  TARGET ${PROJECT_NAME}.gb
  COMMAND rgblink -o ${PROJECT_NAME}.gb -m ${PROJECT_NAME}.map -n ${PROJECT_NAME}.sym ${PROJECT_NAME}.o
  COMMAND rgbfix -v -m 0x01 -c -p 0 ${PROJECT_NAME}.gb
  BYPRODUCTS ${PROJECT_NAME}.map ${PROJECT_NAME}.sym
  VERBATIM)

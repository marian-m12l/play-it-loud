set(PROJECT_NAME jpegdec)
project(${PROJECT_NAME} C CXX ASM)

add_library(${PROJECT_NAME}
    JPEGDEC.cpp
    jpegdec_c_wrapper.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

cmake_minimum_required(VERSION 3.10)

if(NOT CMAKE_C_COMPILER)
    find_program(LUM_C_COMPILER   NAMES gcc)
    find_program(LUM_CXX_COMPILER NAMES g++)

    if(NOT LUM_C_COMPILER OR NOT LUM_CXX_COMPILER)
        message(FATAL_ERROR "Unable to locate GCC/G++ toolchain.")
    endif()

    set(CMAKE_C_COMPILER "${LUM_C_COMPILER}")
    set(CMAKE_CXX_COMPILER "${LUM_C_COMPILER}")
endif()
message(STATUS "C compiler:   ${CMAKE_C_COMPILER}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")

project(Luminous VERSION 0.1)

add_compile_definitions(Lum_Debug)

add_subdirectory(glfw-3.4)

INCLUDE_DIRECTORIES(include)

add_executable(Luminous_example src/example.c src/lum_math2d.c src/lum_filesystem.c src/lum_shader.c src/lum_texture.c src/lum_renderer2d.c src/lum_transform.c src/lum_camera.c src/lum_window.c src/lum_input.c src/lum_time.c glad.c)
target_link_libraries(Luminous_example glfw)

file(COPY src/shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

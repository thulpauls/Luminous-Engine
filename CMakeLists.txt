cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_COMPILER C:/mingw-w64/mingw64/bin/gcc.exe)
set(CMAKE_CXX_COMPILER C:/mingw-w64/mingw64/bin/g++.exe)

project(Luminous VERSION 0.1)

add_compile_definitions(Lum_Debug)

add_subdirectory(glfw-3.4)

INCLUDE_DIRECTORIES(include)

add_executable(Luminous_playground src/main.c src/quad.c src/math2d.c src/file_read.c src/shader.c src/texture.c src/glad.c)
target_link_libraries(Luminous_playground glfw)

file(COPY src/shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

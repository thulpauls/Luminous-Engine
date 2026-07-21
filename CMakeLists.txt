cmake_minimum_required(VERSION 3.10)

project(Luminous VERSION 0.1)

add_compile_definitions(Lum_Debug)

add_subdirectory(glfw-3.4)

INCLUDE_DIRECTORIES(include)

set(LUMINOUS_COMMON_SOURCES
  src/lum_camera.c
  src/lum_file_read.c
  src/lum_input.c
  src/lum_math2d.c
  src/lum_quad.c
  src/lum_renderer.c
  src/lum_renderer2d.c
  src/lum_shader.c
  src/lum_texture.c
  src/lum_time.c
  src/lum_transform.c
  src/lum_window.c
  glad.c
)

add_executable(Luminous_example src/example.c ${LUMINOUS_COMMON_SOURCES})
add_executable(Luminous_playground src/main.c ${LUMINOUS_COMMON_SOURCES})
target_link_libraries(Luminous_playground PRIVATE glfw m)
target_link_libraries(Luminous_example PRIVATE glfw m)

file(COPY src/shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

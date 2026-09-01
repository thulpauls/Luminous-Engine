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

INCLUDE_DIRECTORIES(include src .)

file(GLOB_RECURSE LUM_ENGINE_SOURCES CONFIGURE_DEPENDS src/*.c)

add_library(Luminous STATIC ${LUM_ENGINE_SOURCES} glad.c)

file(GLOB_RECURSE LUM_EDITOR_SOURCES CONFIGURE_DEPENDS editor/*.c)
add_library(Luminous_editor STATIC ${LUM_EDITOR_SOURCES})

add_executable(Luminous_editor_app editor_app.c)
target_link_libraries(Luminous_editor_app Luminous_editor Luminous glfw)

# ---------- 单元测试（CTest） ----------
enable_testing()
foreach(t ecs containers serial scene scene_io ui)
  add_executable(test_${t} tests/test_${t}.c)
  target_link_libraries(test_${t} Luminous glfw)
  add_test(NAME ${t} COMMAND test_${t})
endforeach()

file(COPY resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

set_tests_properties(scene_io PROPERTIES WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

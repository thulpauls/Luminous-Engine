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

file(GLOB LUM_ENGINE_SOURCES CONFIGURE_DEPENDS src/*.c)

add_library(Luminous STATIC ${LUM_ENGINE_SOURCES} glad.c)

add_executable(Luminous_example example.c)
target_link_libraries(Luminous_example Luminous glfw)

file(COPY resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

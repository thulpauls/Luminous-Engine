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

# 版本号唯一来源 = include/lum_version.h（只读取，不反向生成头文件）。
# 必须在 project() 之前解析，好让 project(VERSION) 直接采用这三个值 ——
# 这样构建系统与公共头不可能各写一份而悄悄漂移。
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/include/lum_version.h" LUM_VERSION_HEADER)
foreach(_part MAJOR MINOR PATCH)
    string(REGEX MATCH "#define LUM_VERSION_${_part}[ \t]+([0-9]+)" _hit "${LUM_VERSION_HEADER}")
    set(LUM_VERSION_${_part} "${CMAKE_MATCH_1}")
    if(NOT LUM_VERSION_${_part} MATCHES "^[0-9]+$")
        message(FATAL_ERROR "无法从 include/lum_version.h 解析 LUM_VERSION_${_part}")
    endif()
endforeach()

project(Luminous VERSION ${LUM_VERSION_MAJOR}.${LUM_VERSION_MINOR}.${LUM_VERSION_PATCH})
message(STATUS "Luminous version: ${PROJECT_VERSION}")

add_compile_definitions(Lum_Debug)

add_subdirectory(glfw-3.4)

file(GLOB_RECURSE LUM_ENGINE_SOURCES CONFIGURE_DEPENDS src/*.c)

add_library(Luminous STATIC ${LUM_ENGINE_SOURCES} glad.c)

# 公共头文件根：include/ 是唯一的对外包含根，且只放引擎自己的公共头。
# 引擎实现目录 src/ 不加入包含路径，消费者无法（也不应）包含实现文件。
target_include_directories(Luminous PUBLIC include)

# 第三方头（glad/GLFW/KHR/stb）：只有引擎实现需要，作为私有包含根，不对外暴露。
# 引擎的公共头不包含任何第三方头（已验证），所以消费者无需看到 third_party/。
target_include_directories(Luminous PRIVATE third_party)

# ---------- 示例 ----------
add_executable(Luminous_collision_demo collision_demo.c)
target_link_libraries(Luminous_collision_demo Luminous glfw)

file(COPY resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

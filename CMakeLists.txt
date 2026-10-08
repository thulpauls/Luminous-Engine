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

# ============================================================================
# 版本号唯一来源 = 两个版本头（引擎与 glimmer 分开计数，见 docs/versioning.md）：
#     include/lum_version.h              —— 引擎版本 + 它针对的 glimmer 版本
#     include/glimmer/glimmer_version.h  —— glimmer 平台自身的版本
# 必须在 project() 之前解析：CMake 的 project(VERSION) 只接受数字、塞不进阶段词，
# 所以喂给它的是【迭代次数】，阶段词单独取出供消息与 tag 使用。
# 这里顺带做一次配置期兼容检查 —— 与头文件里的 #if/#error 是同一个判据的两道门，
# 这一道报错更早、信息更全。
# ============================================================================
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/include/lum_version.h" LUM_VERSION_HEADER)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/include/glimmer/glimmer_version.h" LUM_GLIMMER_VERSION_HEADER)

# 引擎：当前版本 + 它声明的兼容 glimmer 版本
string(REGEX MATCH "#define Lum_Version_Iteration[ \t]+([0-9]+)" _hit "${LUM_VERSION_HEADER}")
set(LUM_VERSION_ITERATION "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define Lum_Version_Stage_Name[ \t]+\"([^\"]*)\"" _hit "${LUM_VERSION_HEADER}")
set(LUM_VERSION_STAGE "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define Lum_Version_Glimmer_Stage[ \t]+([0-9]+)" _hit "${LUM_VERSION_HEADER}")
set(LUM_VERSION_GLIMMER_STAGE "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define Lum_Version_Glimmer_Iteration[ \t]+([0-9]+)" _hit "${LUM_VERSION_HEADER}")
set(LUM_VERSION_GLIMMER_ITERATION "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define Lum_Version_Glimmer_Stage_Name[ \t]+\"([^\"]*)\"" _hit "${LUM_VERSION_HEADER}")
set(LUM_VERSION_GLIMMER_STAGE_NAME "${CMAKE_MATCH_1}")

# glimmer：自身版本。阶段号是符号（Lum_Glimmer_Version_Stage_Prototype），先取符号名再解析它的值。
string(REGEX MATCH "#define Lum_Glimmer_Version_Stage_Current[ \t]+([A-Za-z_0-9]+)" _hit "${LUM_GLIMMER_VERSION_HEADER}")
set(LUM_GLIMMER_STAGE_SYMBOL "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define ${LUM_GLIMMER_STAGE_SYMBOL}[ \t]+([0-9]+)" _hit "${LUM_GLIMMER_VERSION_HEADER}")
set(LUM_GLIMMER_STAGE "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define Lum_Glimmer_Version_Stage_Name[ \t]+\"([^\"]*)\"" _hit "${LUM_GLIMMER_VERSION_HEADER}")
set(LUM_GLIMMER_STAGE_NAME "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define Lum_Glimmer_Version_Iteration[ \t]+([0-9]+)" _hit "${LUM_GLIMMER_VERSION_HEADER}")
set(LUM_GLIMMER_ITERATION "${CMAKE_MATCH_1}")

foreach(_v LUM_VERSION_ITERATION LUM_VERSION_GLIMMER_STAGE LUM_VERSION_GLIMMER_ITERATION
           LUM_GLIMMER_STAGE LUM_GLIMMER_ITERATION)
    if(NOT "${${_v}}" MATCHES "^[0-9]+$")
        message(FATAL_ERROR "无法从版本头解析 ${_v}（得到 '${${_v}}'）")
    endif()
endforeach()
foreach(_v LUM_VERSION_STAGE LUM_GLIMMER_STAGE_NAME LUM_VERSION_GLIMMER_STAGE_NAME)
    if("${${_v}}" STREQUAL "")
        message(FATAL_ERROR "无法从版本头解析 ${_v}")
    endif()
endforeach()

project(Luminous VERSION ${LUM_VERSION_ITERATION})

# 配置期兼容门：引擎针对的 glimmer 版本必须与实际 glimmer 一致
if(NOT LUM_VERSION_GLIMMER_STAGE EQUAL LUM_GLIMMER_STAGE OR
   NOT LUM_VERSION_GLIMMER_ITERATION EQUAL LUM_GLIMMER_ITERATION)
    message(FATAL_ERROR
        "引擎声明兼容 glimmer ${LUM_VERSION_GLIMMER_STAGE} (迭代 ${LUM_VERSION_GLIMMER_ITERATION})，"
        "但 include/glimmer/glimmer_version.h 里是 ${LUM_GLIMMER_STAGE} (迭代 ${LUM_GLIMMER_ITERATION})。"
        "两个版本头必须成对修改 —— 见 docs/versioning.md 的兼容性矩阵")
endif()

message(STATUS "Luminous version: ${LUM_VERSION_STAGE} ${LUM_VERSION_ITERATION}  (针对 glimmer ${LUM_VERSION_GLIMMER_STAGE_NAME} ${LUM_VERSION_GLIMMER_ITERATION})")
message(STATUS "glimmer version:  ${LUM_GLIMMER_STAGE_NAME} ${LUM_GLIMMER_ITERATION}")

add_compile_definitions(Lum_Debug)

add_subdirectory(glfw-3.4)

# ============================================================================
# 三个构建目标，依赖是一条单向链（想装反都装不上）：
#
#     glad ──> glimmer ──> Luminous
#              （基础平台）  （引擎高层）
#
# 为什么这么分：
#   · glimmer 是【基础平台】：数学层（math2d/transform）+ 平台层（log/assert/filesystem/
#     window/input/time）+ 渲染器（管线/着色器/材质/纹理/渲染目标/图集/相机/文字）。
#     它自带窗口与帧循环所需的一切，因此**能独立运行** —— `glimmer_hello` 这个可执行目标
#     只链接 glimmer、不链接 libLuminous.a，就是这条边界的机器判据。
#   · glimmer 【禁止】依赖任何引擎高层模块（application/ecs/scene/serial/assets/collision/
#     ui/effects/components/containers）。第二次重组时是链接器先把这条线画出来的：
#     libglimmer.a 里的 lum_shader.c / lum_text_renderer.c 引用了 lum_fs_read*，
#     于是文件系统（以及随后的数学、日志、窗口）被并入 glimmer，而不是让 glimmer 反过来
#     依赖引擎。靠"把文件挪个目录"发现不了这类隐藏依赖。
#   · 边界可机械校验：tools/check-glimmer-boundary.ps1 检查 glimmer 源码的包含关系，
#     构建目标本身检查链接依赖。
# ============================================================================

# --- third_party/glad：GL 函数装载（glimmer 私有依赖）---
add_library(glad STATIC glad.c)
target_include_directories(glad PUBLIC third_party)

# --- glimmer：基础平台 = 数学层 + 平台层 + 渲染器（显式列源文件，不用 GLOB）---
add_library(glimmer STATIC
    # 数学层
    src/glimmer/lum_math2d.c
    src/glimmer/lum_transform.c
    # 平台层
    src/glimmer/lum_log.c
    src/glimmer/lum_filesystem.c
    src/glimmer/lum_window.c
    src/glimmer/lum_input.c
    src/glimmer/lum_time.c
    # 渲染器
    src/glimmer/lum_shader.c
    src/glimmer/lum_texture.c
    src/glimmer/lum_material.c
    src/glimmer/lum_render_target.c
    src/glimmer/lum_atlas.c
    src/glimmer/lum_camera.c
    src/glimmer/lum_renderer2d.c
    src/glimmer/lum_text_renderer.c
    # 3D（见 docs/design/glimmer3d.md；M1 目标是"带深度测试的立方体"）
    src/glimmer/lum_mesh.c
    src/glimmer/lum_renderer3d.c)

# 公共头文件根：include/ 是唯一的对外包含根，且只放公共头（引擎的 + glimmer/ 子目录）。
# 实现目录 src/ 不加入包含路径，消费者无法（也不应）包含实现文件。
target_include_directories(glimmer PUBLIC include)
# 第三方头（glad/GLFW/stb）：只有实现需要，作为私有包含根，不对外暴露。
# glimmer 的公共头不包含任何第三方头（已验证），所以消费者无需看到 third_party/。
target_include_directories(glimmer PRIVATE third_party)
# PRIVATE：静态库的链接依赖会传导到最终可执行文件，但包含路径不外泄 ——
# 消费者拿得到 GLFW/glad 的符号，却看不到 third_party/ 的头。
target_link_libraries(glimmer PRIVATE glad)
target_link_libraries(glimmer PRIVATE glfw)

# --- Luminous：引擎高层（显式列源文件）---
add_library(Luminous STATIC
    src/lum_application.c
    src/lum_assets.c
    src/lum_collision.c
    src/lum_ecs.c
    src/lum_scene.c
    src/lum_scene_io.c
    src/lum_serial.c
    src/lum_ui.c
    src/containers/lum_vector.c
    src/components/lum_component_beam.c
    src/components/lum_component_glow.c
    src/components/lum_components.c
    src/effects/lum_beam.c
    src/effects/lum_effects.c
    src/effects/lum_glow.c)

target_include_directories(Luminous PUBLIC include)
target_include_directories(Luminous PRIVATE third_party)
# glimmer 是引擎脚下的平台：PUBLIC 传导 —— 消费者（游戏/demo）拿到 Luminous，
# 就同时拿到了数学、平台与渲染器的全部公开面
target_link_libraries(Luminous PUBLIC glimmer)

# ---------- glimmer 独立运行验证（只链接 glimmer，不链接 Luminous）----------
# 这个目标的意义不是"又一个 demo"，而是**边界的机器判据**：
# 它自己开窗口、自己跑帧循环、自己画，一旦有人把引擎高层依赖引进 glimmer，
# 它会立刻链接失败。详见文件头注释。
add_executable(glimmer_hello glimmer_hello.c)
target_link_libraries(glimmer_hello glimmer glfw)

# glimmer 3D：示例 + 无窗口回归检查（都只链接 glimmer）
add_executable(glimmer_hello3d glimmer_hello3d.c)
target_link_libraries(glimmer_hello3d glimmer glfw)

# 无窗口、无 GL：只测 3D 数学 / camera3d / mesh 的纯 CPU 一半（见文件头注释）
add_executable(glimmer_check glimmer_check.c)
target_link_libraries(glimmer_check glimmer glfw)

# M2 实例化示例：同一个 mesh，实例化 vs 逐实例提交，输出 draw call 与耗时表

add_executable(glimmer_material3d glimmer_material3d.c)
target_link_libraries(glimmer_material3d glimmer glfw)

add_executable(glimmer_instancing glimmer_instancing.c)
target_link_libraries(glimmer_instancing glimmer glfw)

# ---------- 示例 ----------
add_executable(Luminous_collision_demo collision_demo.c)
target_link_libraries(Luminous_collision_demo Luminous glfw)

# ---------- RTS 示例 ----------
# 显式列出源文件（不用 GLOB）：新增文件必须显式加进来，避免"忘了加文件却静默通过"
add_executable(Luminous_rts
    games/rts/rts_main.c
    games/rts/rts_camera.c
    games/rts/rts_collision.c
    games/rts/rts_navgrid.c
    games/rts/rts_path.c
    games/rts/rts_selection.c
    games/rts/rts_orders.c
    games/rts/rts_units.c)
target_link_libraries(Luminous_rts Luminous glfw)

# ---------- RTS 回归检查（无窗口、无 GL：只跑逻辑与数学）----------
# 这是仓库里第一个回归检查。它只依赖公共头 + 纯逻辑路径（lum_flow_run_steps），
# 不碰窗口，因此可以在任何环境下运行。
add_executable(Luminous_rts_check
    games/rts/rts_check.c
    games/rts/rts_collision.c
    games/rts/rts_navgrid.c
    games/rts/rts_path.c
    games/rts/rts_selection.c
    games/rts/rts_orders.c
    games/rts/rts_units.c)
target_include_directories(Luminous_rts_check PRIVATE games/rts)
target_link_libraries(Luminous_rts_check Luminous glfw)

# ---------- A* 寻路演示 ----------
# 刻意【复用 games/rts 的 rts_navgrid + rts_path】，不另写一份教学版 A*：
# demo 里看到的搜索过程就是游戏真正在跑的那一份，两份实现不会各自漂移。
# 代价是要把 games/rts 加进包含路径（它不是引擎公共头的一部分）。
add_executable(Luminous_astar_demo
    a_star_demo.c
    games/rts/rts_navgrid.c
    games/rts/rts_path.c)
target_include_directories(Luminous_astar_demo PRIVATE games/rts)
target_link_libraries(Luminous_astar_demo Luminous glfw)

file(COPY resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

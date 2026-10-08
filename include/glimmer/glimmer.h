#ifndef Glimmer_H
#define Glimmer_H

/*
* glimmer —— 引擎的【基础平台】：数学层 + 平台层 + 渲染器，合成一个能独立运行的整体。
*
* 分层（2026-09 第二次重组：渲染器不再只是"引擎的一个模块"，而是引擎脚下的那块地）：
*
*     glad ──> glimmer ──> Luminous
*              （本模块）    （引擎高层：ecs/scene/serial/assets/ui/effects/application）
*
*   glimmer 自带：
*     · 数学      lum_math2d / lum_transform
*     · 平台      lum_log / lum_assert / lum_filesystem / lum_window / lum_input / lum_time
*     · 渲染器    lum_shader / lum_texture / lum_material / lum_render_target /
*                 lum_atlas / lum_camera / lum_renderer2d / lum_text_renderer
*
*   glimmer 【禁止】依赖任何 Luminous 高层模块：
*     application / ecs / scene / scene_io / serial / assets / collision / ui /
*     effects / components / containers。
*   方向永远是 Luminous → glimmer，由 tools/check-glimmer-boundary.ps1 强制。
*
* "能独立运行"不是口号：`glimmer_hello.c`（构建目标 `glimmer_hello`）**只链接 glimmer**，
* 不链接 libLuminous.a，自己开窗口、自己跑帧循环、自己画。它就是这条边界的机器判据 ——
* 谁把高层依赖引进 glimmer，这个目标会立刻链接失败。
*
* 命名现状：符号仍是 `lum_*` / `Lum_*`；改成 `glimmer_*` / `Glimmer_*` 是独立的阶段 2
* （触及引擎 + 游戏 + demo 约 170 处调用点，属破坏性变更）。
*/

/* --- 数学层 --- */
#include "glimmer/glimmer_version.h"   /* 平台自身的版本号（与引擎分开计数） */
#include "glimmer/lum_math2d.h"
#include "glimmer/lum_transform.h"

/* --- 平台层 --- */
#include "glimmer/lum_log.h"
#include "glimmer/lum_assert.h"
#include "glimmer/lum_filesystem.h"
#include "glimmer/lum_window.h"
#include "glimmer/lum_input_codes.h"
#include "glimmer/lum_input.h"
#include "glimmer/lum_time.h"

/* --- 渲染器 --- */
#include "glimmer/lum_shader.h"
#include "glimmer/lum_texture.h"
#include "glimmer/lum_atlas.h"
#include "glimmer/lum_camera.h"
#include "glimmer/lum_material.h"
#include "glimmer/lum_render_target.h"
#include "glimmer/lum_renderer2d.h"
#include "glimmer/lum_text_renderer.h"

/* --- 3D（见 docs/design/glimmer3d.md；M1 = 最简可用版）--- */
/* 3D 相机（lum_Camera3d）与 2D 相机同住 lum_camera.h，上面已包含 */
#include "glimmer/lum_mesh.h"
#include "glimmer/lum_renderer3d.h"

#endif //Glimmer_H

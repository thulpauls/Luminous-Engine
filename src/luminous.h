#ifndef Luminous_H
#define Luminous_H

/* ===========================================================================
 *  Luminous —— OpenGL 2D 图形框架.
 *
 *  分层组织, 依赖自底向上:
 *
 *      基础层  —— 日志, 数学
 *      平台层  —— 文件系统, 窗口, 输入, 时间
 *      渲染层  —— 着色器, 纹理, 相机, 变换, 2D 渲染器
 *      子系统  —— 文本渲染
 * =========================================================================== */

#include "lum_application.h"

/* --- 基础层 --- */
#include "lum_log.h"
#include "lum_math2d.h"

/* --- 平台层 --- */
#include "lum_filesystem.h"
#include "lum_window.h"
#include "lum_input_codes.h"
#include "lum_input.h"
#include "lum_time.h"

/* --- 渲染层 --- */
#include "lum_shader.h"
#include "lum_texture.h"
#include "lum_camera.h"
#include "lum_transform.h"
#include "lum_renderer2d.h"

/* --- 子系统 --- */
#include "lum_text_renderer.h"

#endif //Luminous_H
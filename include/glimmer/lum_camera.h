#ifndef Lum_Camera_H
#define Lum_Camera_H

#include <stdbool.h>
#include "glimmer/lum_math2d.h"

/*
* glimmer 的两个相机模型。它们【并存、互不替换】：
*
*   lum_Camera2d —— 正交相机：位置 + 旋转 + 缩放 → 正交投影。
*                    服务于 2D 路径（精灵、UI/HUD、小地图）与游戏逻辑的屏幕↔世界换算。
*   lum_Camera3d —— 透视相机：眼睛 + 注视点 + 上方向 → 透视投影。
*                    只服务于 3D 渲染（见 docs/design/glimmer3d.md）。
*
* 为什么放在同一个文件里：两者是同一件事（"从哪儿看"）的两种模型。消费者要么用其中一个，
* 要么两个都用（3D 场景 + 2D HUD 叠加就是后者，见施工大纲的 M9），拆成两个头文件
* 只会让"我该包含哪个"变成一个多余的问题。符号前缀已经把两者分开（camera2d_ / camera3d_），
* 不存在混淆。
*
* 两者的投影兜底策略一致：参数非法时返回单位矩阵并记一条 Error 日志，不产生 NaN。
*/

/* ============================ 2D：正交相机 ============================ */

typedef struct lum_Camera2d {
  lum_Vec2 position;
  float rotation;
  float zoom;

  float viewport_w, viewport_h;
  float near_plane, far_plane;
} lum_Camera2d;

void lum_camera2d_init(lum_Camera2d* camera, float viewport_w, float viewport_h);

void lum_camera2d_set_position(lum_Camera2d* camera, lum_Vec2 position);
void lum_camera2d_set_rotation(lum_Camera2d* camera, float rad);
void lum_camera2d_set_zoom(lum_Camera2d* camera, float zoom);

void lum_camera2d_translate(lum_Camera2d* camera, lum_Vec2 delta);
void lum_camera2d_rotate(lum_Camera2d* camera, float delta_rad);

void lum_camera2d_set_viewport(lum_Camera2d* camera, float w, float h);
void lum_camera2d_set_planes(lum_Camera2d* camera, float near, float far);

lum_Mat4 lum_camera2d_get_view_matrix(const lum_Camera2d* camera);
lum_Mat4 lum_camera2d_get_projection_matrix(const lum_Camera2d* camera);
lum_Mat4 lum_camera2d_get_view_projection_matrix(const lum_Camera2d* camera);

lum_Vec2 lum_camera2d_screen_to_world(const lum_Camera2d* cam, lum_Vec2 screen_pos, lum_Vec2 window_size);
lum_Vec2 lum_camera2d_world_to_screen(const lum_Camera2d* cam, lum_Vec2 world_pos, lum_Vec2 window_size);

/* ============================ 3D：透视相机 ============================ */

/*
* 投影参数非法（aspect/fov/near/far 越界）时投影返回单位矩阵并记日志，
* 视图在 up 与视线平行时同样退化 —— 都在 lum_mat4_perspective / lum_mat4_look_at 里兜底。
*/
typedef struct lum_Camera3d {
  lum_Vec3 position;          /* 眼睛位置（世界坐标） */
  lum_Vec3 target;            /* 注视点（世界坐标） */
  lum_Vec3 up;                /* 上方向（不必归一化，但不得与视线平行） */
  float    fov_y_rad;         /* 垂直视场角（弧度），默认 60° */
  float    near_plane, far_plane;
  float    aspect;            /* 宽/高；由 set_viewport 维护，也可直接写 */
} lum_Camera3d;

/* 默认：站在 (0,0,3) 看向原点、up=(0,1,0)、fov 60°、near 0.1 / far 100、aspect 由参数给 */
void lum_camera3d_init(lum_Camera3d* camera, float aspect);

/* 视口尺寸 → aspect（<= 0 的尺寸被忽略，aspect 保持原值） */
void lum_camera3d_set_viewport(lum_Camera3d* camera, float width, float height);

/* 绕注视点做球面轨道运动：yaw/pitch 是【绝对角】(弧度)，distance 是距离。
   pitch 会被夹到 ±(π/2 - 1e-3)，避免视线与 up 共线导致视图退化 —— 示例的轨道相机用它。 */
void lum_camera3d_orbit(lum_Camera3d* camera, float yaw, float pitch, float distance);

lum_Mat4 lum_camera3d_view(const lum_Camera3d* camera);
lum_Mat4 lum_camera3d_projection(const lum_Camera3d* camera);
lum_Mat4 lum_camera3d_view_projection(const lum_Camera3d* camera);

/* 相机前方向（已归一化）= normalise(target - position) */
lum_Vec3 lum_camera3d_forward(const lum_Camera3d* camera);

#endif //Lum_Camera_H

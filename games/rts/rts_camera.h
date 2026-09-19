#ifndef Rts_Camera_H
#define Rts_Camera_H

#include <stdbool.h>
#include "lum_math2d.h"
#include "lum_camera.h"

/*
* RTS 相机控制器。直接操作引擎的 app 相机（lum_app_get_camera()），
* 没有自己的相机实例 —— 引擎的 screen↔world 换算、视图默认路径都认那一份。
*
* 三种平移可同时启用：
*   - 键盘 WASD / 方向键
*   - 鼠标贴边滚动
*   - 中键拖拽（"抓住世界拖"）
* 缩放：滚轮，以【光标下的世界点】为锚 —— 缩放时该点保持不动。
*
* 单位约定：pan_speed_px / edge_speed_px 是【屏幕像素/秒】，内部按 zoom 折算成世界单位，
* 这样任意缩放下手感一致；中键拖拽天然 1:1 跟手。
*/
typedef struct rts_Camera_ctl {
  float    pan_speed_px;      /* 键盘平移（屏幕像素/秒） */
  float    edge_speed_px;     /* 边缘滚动（屏幕像素/秒） */
  float    edge_margin_px;    /* 触发边缘滚动的带宽（像素） */
  float    zoom_step;         /* 滚轮每格的倍率 */
  float    zoom_min, zoom_max;
  bool     edge_scroll_enabled;

  bool     dragging;          /* 中键拖拽中 */
  lum_Vec2 drag_anchor_px;    /* 拖拽起点（屏幕像素、左上原点） */
  lum_Vec2 drag_cam_pos;      /* 拖拽起点时的相机位置（世界） */
} rts_Camera_ctl;

void rts_camera_ctl_init(rts_Camera_ctl* c);

/* 每帧一次（在 Input 阶段调用）。camera 传 lum_app_get_camera()。
   ui_captured 为真时吞掉【鼠标相关】的平移与缩放；键盘平移仍然生效。 */
void rts_camera_ctl_update(rts_Camera_ctl* c, lum_Camera2d* camera, bool ui_captured);

#endif //Rts_Camera_H

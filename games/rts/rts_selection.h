#ifndef Rts_Selection_H
#define Rts_Selection_H

#include <stdbool.h>
#include "luminous.h"

/* 拖拽位移小于这个像素数就当作"单击"而不是框选 */
#define Rts_Selection_Click_Slop 4.0f

/*
* 鼠标状态快照：由调用方从引擎输入层填好再传进来。
* 这样 selection 的逻辑【不依赖任何全局单例】，可以脱离窗口做回归测试。
*/
typedef struct rts_Mouse_state {
  lum_Vec2 position;      /* 屏幕像素、左上原点 */
  bool     left_pressed;  /* 边沿：本帧刚按下 */
  bool     left_down;     /* 状态：本帧按住 */
} rts_Mouse_state;

/*
* 框选与选中状态。
*
* 分两个阶段是刻意的，也是引擎数据契约要求的：
*   - Input  阶段只记录"框住哪块屏幕矩形 / 点了哪里"，不碰 ECS
*   - Post   阶段才真正结算命中 —— 那时 Transform 阶段已经跑完，
*            transform.world 才是本步最新的（在 Input 结算会用到上一帧的矩阵）
*/
typedef struct rts_Selection {
  /* 输入侧（屏幕像素、左上原点） */
  bool     dragging;
  lum_Vec2 start_px;
  lum_Vec2 current_px;

  /* 待结算请求：Input 置位，Post 消费 */
  bool     pending;
  bool     pending_is_click;
  lum_Vec2 pending_point_px;

  /* 供 HUD 读取的统计（每次结算后刷新） */
  uint32_t selected_count;
  uint32_t total_count;
} rts_Selection;

void rts_selection_init(rts_Selection* s);

/* Input 阶段：起手 / 更新 / 收手。只记意图，不查 ECS。
   window_size 用于判断指针是否在窗口内并把拖动端点夹进窗口 ——
   指针跑到窗口外时 GLFW 的客户区坐标会大幅越界（实测拿到过 (1996,1218)，
   而窗口只有 1280x720），不夹的话框会被画到画面外，看起来就像"框瞬间消失"。 */
void rts_selection_input(rts_Selection* s, rts_Mouse_state mouse,
                         lum_Vec2 window_size, bool ui_captured);

/* Post 阶段：world 矩阵已更新，在这里结算命中并刷新统计。
   相机与窗口尺寸【显式传入】而不是内部去取全局单例 —— 这样这段逻辑不需要
   窗口就能跑（headless 测试），也不必知道 lum_app_* 的存在。 */
void rts_selection_resolve(rts_Selection* s, lum_Ecs* ecs,
                           const lum_Camera2d* camera, lum_Vec2 window_size);

/* Scene 阶段：给选中单位画世界空间角标 */
void rts_selection_draw_marks(const rts_Selection* s, lum_Ecs* ecs);

/* Screen 阶段：画框选矩形（像素空间、左上原点，直接给像素坐标） */
void rts_selection_draw_box(const rts_Selection* s);

#endif //Rts_Selection_H

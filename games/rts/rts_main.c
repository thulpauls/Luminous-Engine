/*
* Luminous RTS —— M1：相机 + 框选；M2：单位移动指令；M3：网格 A* 寻路。
*
* 目标是【打通并验证引擎契约】，不是做游戏：
*   - 相机控制（键盘/边缘/中键拖拽 + 滚轮以光标为锚缩放）
*   - 框选与单击选中，命中结算放在 Post 阶段（world 矩阵最新的时刻）
*   - 单位走 ECS（transform + sprite + custom unit 组件），由引擎内置 sprite 系统绘制
*   - 右键下达移动指令（Input），单位在 Pre 阶段沿 A* 路点推进
*     —— 这条链路就是将来 AI 的输出口
*
* 刻意没有引入的东西：单位互斥（M3 后半）、战斗、经济、UI、事件。
*/
#include "luminous.h"
#include "rts_camera.h"
#include "rts_layers.h"
#include "rts_navgrid.h"
#include "rts_orders.h"
#include "rts_selection.h"
#include "rts_units.h"

#include <stdio.h>

static rts_Camera_ctl g_cam;
static rts_Selection  g_sel;
static lum_Font*      g_font;
static lum_Texture2d  g_white;    /* 单位贴图：1x1 白，靠 sprite.color 染色 */

/* 导航网格 = 游戏侧的地图数据。它【不是】每个实体的数据，所以不进 ECS，
   而是像相机一样由 main 持有、显式传给需要它的函数（这样那些函数仍可 headless 测）。 */
static rts_Navgrid g_nav;

/* [P] 键切换：关掉就等于回到 M2 的直线趋近。留着是为了能看到寻路的差别。 */
static bool g_nav_enabled = true;

/* 最后一次下令点（仅用于画个方框给玩家反馈）。这是 UI 状态，所以留在 main 里，
   不进 rts_orders —— 那一层要保持"纯逻辑、无全局状态"，才能脱离窗口测试。 */
static lum_Vec2 g_last_cmd;
static bool     g_has_cmd;

/* 指针在窗口外时 GLFW 的客户区坐标会大幅越界（实测拿到过 (1996,1218)，
   而窗口只有 1280x720）。不判断的话右键会把单位派到画面外的世界坐标去。 */
static bool pointer_in_window(lum_Vec2 p, lum_Vec2 win) {
  return p.x >= 0.0f && p.x <= win.x && p.y >= 0.0f && p.y <= win.y;
}

/* Screen 阶段画 HUD：用 lum_tr_draw_screen —— 它负责把"左上角像素"换算成
   该阶段 Y 向下 VP 所需的基线位置。直接用 lum_tr_draw_layer 会得到颠倒的文字。 */
static void hud_text(const char* text, float px, float py, lum_Vec4 color) {
  if (!g_font) return;
  lum_tr_draw_screen(g_font, text, lum_vec2_create(px, py), Rts_Layer_Hud, color);
}

/* 确定性随机：同一场景每次跑都一致，便于复现问题 */
static uint32_t g_rng = 20260916u;
static float rts_rand01(void) {
  g_rng = g_rng * 1664525u + 1013904223u;
  return (float)((g_rng >> 8) & 0xFFFFFFu) / 16777216.0f;
}

static void spawn_armies(lum_Ecs* ecs) {
  const float side = 22.0f;
  const lum_Vec4 player_c = lum_vec4_create(0.35f, 0.65f, 1.00f, 1.0f);
  const lum_Vec4 enemy_c  = lum_vec4_create(1.00f, 0.42f, 0.38f, 1.0f);

  /* 玩家：左侧 6x3 方阵，带一点抖动（顺便验证 AABB 不会全部整齐重合） */
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 6; ++col) {
      lum_Vec2 p = lum_vec2_create(
          -320.0f + (float)col * 46.0f + (rts_rand01() - 0.5f) * 10.0f,
          -80.0f  + (float)row * 46.0f + (rts_rand01() - 0.5f) * 10.0f);
      rts_units_spawn(ecs, p, side, player_c, 0, &g_white);
    }
  }
  /* 敌方：右侧 4x4 */
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      lum_Vec2 p = lum_vec2_create(260.0f + (float)col * 44.0f,
                                   -90.0f + (float)row * 44.0f);
      rts_units_spawn(ecs, p, side, enemy_c, 1, &g_white);
    }
  }
}

static void sys_start(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ctx; (void)user_data;
  lum_renderer2d_set_clear_color(lum_vec4_create(0.055f, 0.075f, 0.095f, 1.0f));

  g_font = lum_assets_font("resources/FiraCode-Regular.ttf", 15);
  if (!g_font) Lum_Log_Warn("rts: font load failed, HUD disabled.");

  if (!lum_texture2d_create_white(&g_white)) {
    Lum_Log_Error("rts: white texture creation failed.");
  }

  rts_units_register(ecs);
  rts_navgrid_setup_default(&g_nav);
  spawn_armies(ecs);
  rts_selection_init(&g_sel);
  rts_camera_ctl_init(&g_cam);
}

static void sys_input(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ctx; (void)user_data;
  if (lum_input_is_key_pressed(Lum_Key_Q) || lum_input_is_key_pressed(Lum_Key_Escape)) {
    lum_app_quit();
    return;
  }
  if (lum_input_is_key_pressed(Lum_Key_E)) {
    g_cam.edge_scroll_enabled = !g_cam.edge_scroll_enabled;
  }
  if (lum_input_is_key_pressed(Lum_Key_P)) {
    g_nav_enabled = !g_nav_enabled;   /* 关掉寻路 = 回到 M2 的直线趋近，用于对比 */
  }
  /* 引擎 UI 占用输入时世界交互必须跳过。M1 还没挂 UI，但先把这条契约接好 */
  bool ui_captured = lum_ui_input_captured();
  rts_camera_ctl_update(&g_cam, lum_app_get_camera(), ui_captured);

  const lum_Vec2 win = lum_vec2_create((float)lum_window_get_width(),
                                       (float)lum_window_get_height());
  rts_Mouse_state mouse;
  mouse.position = lum_input_get_mouse_position();
  mouse.left_pressed = lum_input_is_mouse_pressed(Lum_Mouse_Button_Left);
  mouse.left_down = lum_input_is_mouse_down(Lum_Mouse_Button_Left);
  rts_selection_input(&g_sel, mouse, win, ui_captured);

  /* 右键 = 移动指令。
     放在 Input 而不是 Post 是刻意的：屏幕→世界只需要 camera + window_size，
     不依赖 transform.world，所以本步的 Pre 阶段就能开始走，没有一帧延迟。
     （框选必须放 Post，因为它要靠 lum_scene_pick 读 world 矩阵。） */
  if (!ui_captured && lum_input_is_mouse_pressed(Lum_Mouse_Button_Right) &&
      pointer_in_window(mouse.position, win)) {
    const lum_Vec2 wp = lum_camera2d_screen_to_world(lum_app_get_camera(),
                                                     mouse.position, win);
    if (rts_orders_move_selected(ecs, g_nav_enabled ? &g_nav : NULL, wp) > 0) {
      g_last_cmd = wp;
      g_has_cmd = true;
    }
  }
}

/* 推进移动指令。放 Pre 是因为它写 transform.position（local）——
   Pre 及更早只能写 local，而 Transform 阶段随后把它算成 world。
   若放到 Post 之后写 local，要到下一步才反映到 world，看起来就是"慢一帧"。 */
static void sys_pre(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)user_data;
  rts_orders_update(ecs, g_nav_enabled ? &g_nav : NULL, ctx->dt);
}

/* 结算命中放在 Post：Transform 阶段已跑完，transform.world 才是本步最新的。
   放在 Input 会用到上一帧矩阵 —— 镜头一动，框选位置就偏。 */
static void sys_post(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ctx; (void)user_data;
  lum_Vec2 win = lum_vec2_create((float)lum_window_get_width(), (float)lum_window_get_height());
  rts_selection_resolve(&g_sel, ecs, lum_app_get_camera(), win);
}

/* 单位由引擎内置 sprite 系统绘制（挂在 Scene、priority -1000，比本系统先跑）。
   这里只补画世界空间的选中角标。 */
static void sys_scene(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ctx; (void)user_data;
  lum_renderer2d_set_blend_mode(Lum_Blend_Alpha);
  /* 顺序就是 layer 顺序：障碍 50 < 单位 100 < 角标 900 < 路径 940 < 下令标记 950。
     注意引擎是把所有绘制命令攒到 Frame_End 才按 layer 排序画的，
     所以这里的调用顺序其实无所谓 —— 真正决定压盖关系的是 layer。 */
  if (g_nav_enabled) rts_navgrid_draw(&g_nav, Rts_Layer_Obstacles);
  rts_orders_draw_paths(ecs, Rts_Layer_Path);
  if (g_has_cmd) rts_orders_draw_mark(g_last_cmd, Rts_Layer_Command);
  rts_selection_draw_marks(&g_sel, ecs);
}

static void sys_screen(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ctx; (void)user_data;
  lum_renderer2d_set_blend_mode(Lum_Blend_Alpha);
  rts_selection_draw_box(&g_sel);

  const lum_Vec4 white = lum_vec4_create(0.92f, 0.94f, 0.98f, 1.0f);
  const lum_Vec4 dim   = lum_vec4_create(0.60f, 0.65f, 0.74f, 1.0f);
  const lum_Vec4 hot   = lum_vec4_create(1.00f, 0.78f, 0.35f, 1.0f);
  const lum_Camera2d* cam = lum_app_get_camera();
  char line[192];
  float y = 26.0f;

  hud_text("Luminous RTS  -  M3: grid A* pathfinding", 16.0f, y, white); y += 22.0f;
  snprintf(line, sizeof(line), "cam (%.0f, %.0f)   zoom %.2f",
           (double)cam->position.x, (double)cam->position.y, (double)cam->zoom);
  hud_text(line, 16.0f, y, dim); y += 20.0f;
  snprintf(line, sizeof(line), "units %u   selected %u   moving %u   waypoints %u",
           (unsigned)g_sel.total_count, (unsigned)g_sel.selected_count,
           (unsigned)rts_orders_active_count(ecs),
           (unsigned)rts_orders_remaining_waypoints(ecs));
  hud_text(line, 16.0f, y, hot); y += 20.0f;
  snprintf(line, sizeof(line), "nav: %s   (grid %dx%d, cell %.0f)",
           g_nav_enabled ? "ON  (P toggles)" : "OFF (straight line)",
           Rts_Nav_Cells_X, Rts_Nav_Cells_Y, (double)Rts_Nav_Cell_Size);
  hud_text(line, 16.0f, y, dim); y += 20.0f;
  hud_text("WASD/arrows pan   MMB drag   wheel zoom   LMB box/click   RMB move   P nav   Q quit",
           16.0f, y, dim);
}

/* Stop 阶段释放自建 GL 资源：此时渲染器与 GL 上下文都还在 */
static void sys_stop(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ecs; (void)ctx; (void)user_data;
  lum_texture2d_destroy(&g_white);
}

int main(void) {
  lum_App_desc desc;
  lum_app_init_desc(&desc);
  desc.title  = "Luminous RTS";
  desc.width  = 1280;
  desc.height = 720;

  lum_app_add_system(Lum_Stage_Start,  0, sys_start,  NULL);
  lum_app_add_system(Lum_Stage_Input,  0, sys_input,  NULL);
  lum_app_add_system(Lum_Stage_Pre,    0, sys_pre,    NULL);
  lum_app_add_system(Lum_Stage_Post,   0, sys_post,   NULL);
  lum_app_add_system(Lum_Stage_Scene,  0, sys_scene,  NULL);
  lum_app_add_system(Lum_Stage_Screen, 0, sys_screen, NULL);
  lum_app_add_system(Lum_Stage_Stop,   0, sys_stop,   NULL);

  lum_app_run(&desc);
  return 0;
}

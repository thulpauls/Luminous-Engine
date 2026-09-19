/*
* Luminous RTS —— M1：相机 + 框选。
*
* 这一期的目标是【打通并验证引擎契约】，不是做游戏：
*   - 相机控制（键盘/边缘/中键拖拽 + 滚轮以光标为锚缩放）
*   - 框选与单击选中，命中结算放在 Post 阶段（world 矩阵最新的时刻）
*   - 单位走 ECS（transform + sprite + 自定义 unit 组件），由引擎内置 sprite 系统绘制
*
* 刻意没有引入的东西：寻路、战斗、经济、UI、事件。它们属于 M2+。
*/
#include "luminous.h"
#include "rts_camera.h"
#include "rts_layers.h"
#include "rts_selection.h"
#include "rts_units.h"

#include <stdio.h>

static rts_Camera_ctl g_cam;
static rts_Selection  g_sel;
static lum_Font*      g_font;
static lum_Texture2d  g_white;    /* 单位贴图：1x1 白，靠 sprite.color 染色 */

/* Screen 阶段 VP 已是像素空间、左上原点 —— 这里直接收像素坐标 */
static void hud_text(const char* text, float px, float py, lum_Vec4 color) {
  if (!g_font) return;
  lum_tr_draw_layer(g_font, text, lum_vec2_create(px, py), Rts_Layer_Hud, color);
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
  spawn_armies(ecs);
  rts_selection_init(&g_sel);
  rts_camera_ctl_init(&g_cam);
}

static void sys_input(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ecs; (void)ctx; (void)user_data;
  if (lum_input_is_key_pressed(Lum_Key_Q) || lum_input_is_key_pressed(Lum_Key_Escape)) {
    lum_app_quit();
    return;
  }
  if (lum_input_is_key_pressed(Lum_Key_E)) {
    g_cam.edge_scroll_enabled = !g_cam.edge_scroll_enabled;
  }
  /* 引擎 UI 占用输入时世界交互必须跳过。M1 还没挂 UI，但先把这条契约接好 */
  bool ui_captured = lum_ui_input_captured();
  rts_camera_ctl_update(&g_cam, lum_app_get_camera(), ui_captured);
  rts_selection_input(&g_sel, ui_captured);
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
  rts_selection_draw_marks(&g_sel, ecs);
}

static void sys_screen(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ecs; (void)ctx; (void)user_data;
  lum_renderer2d_set_blend_mode(Lum_Blend_Alpha);
  rts_selection_draw_box(&g_sel);

  const lum_Vec4 white = lum_vec4_create(0.92f, 0.94f, 0.98f, 1.0f);
  const lum_Vec4 dim   = lum_vec4_create(0.60f, 0.65f, 0.74f, 1.0f);
  const lum_Vec4 hot   = lum_vec4_create(1.00f, 0.78f, 0.35f, 1.0f);
  const lum_Camera2d* cam = lum_app_get_camera();
  char line[192];
  float y = 26.0f;

  hud_text("Luminous RTS  -  M1: camera + box selection", 16.0f, y, white); y += 22.0f;
  snprintf(line, sizeof(line), "cam (%.0f, %.0f)   zoom %.2f",
           (double)cam->position.x, (double)cam->position.y, (double)cam->zoom);
  hud_text(line, 16.0f, y, dim); y += 20.0f;
  snprintf(line, sizeof(line), "units %u   selected %u",
           (unsigned)g_sel.total_count, (unsigned)g_sel.selected_count);
  hud_text(line, 16.0f, y, hot); y += 20.0f;
  hud_text("WASD/arrows pan   edge scroll   MMB drag   wheel zoom   LMB box/click   Q quit",
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
  lum_app_add_system(Lum_Stage_Post,   0, sys_post,   NULL);
  lum_app_add_system(Lum_Stage_Scene,  0, sys_scene,  NULL);
  lum_app_add_system(Lum_Stage_Screen, 0, sys_screen, NULL);
  lum_app_add_system(Lum_Stage_Stop,   0, sys_stop,   NULL);

  lum_app_run(&desc);
  return 0;
}

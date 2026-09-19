/* M1 无窗口验证：框选/单击命中的数学与阶段契约。
   不需要 GL、不需要窗口 —— 直接建 ECS、跑一步步进（含 Transform）再结算选择。 */
#include "luminous.h"
#include "rts_selection.h"
#include "rts_units.h"

#include <stdio.h>
#include <math.h>

#define WIN_W 1280.0f
#define WIN_H 720.0f
#define SIDE  20.0f

static int g_fail = 0;

static void check(const char* what, uint32_t got, uint32_t want) {
  int ok = (got == want);
  if (!ok) ++g_fail;
  printf("  %-46s got %u  want %u   %s\n", what, (unsigned)got, (unsigned)want,
         ok ? "ok" : "★FAIL★");
}

/* 相机在原点、zoom 1 时：世界 (0,0) 在屏幕中心 (640,360)；1 世界单位 = 1 像素 */
static lum_Vec2 screen_of_world(lum_Vec2 w) {
  return lum_vec2_create(WIN_W * 0.5f + w.x, WIN_H * 0.5f - w.y);
}

static rts_Mouse_state mouse_at(lum_Vec2 p, bool pressed, bool down) {
  rts_Mouse_state m;
  m.position = p;
  m.left_pressed = pressed;
  m.left_down = down;
  return m;
}

static void box_select(rts_Selection* s, lum_Ecs* ecs, const lum_Camera2d* cam,
                       lum_Vec2 win, lum_Vec2 w0, lum_Vec2 w1) {
  rts_selection_init(s);
  s->pending = true;
  s->pending_is_click = false;
  s->start_px = screen_of_world(w0);
  s->current_px = screen_of_world(w1);
  rts_selection_resolve(s, ecs, cam, win);
}

/* 直接给屏幕像素（用于"整个画面"这类与相机无关的框） */
static void box_select_screen(rts_Selection* s, lum_Ecs* ecs, const lum_Camera2d* cam,
                              lum_Vec2 win, lum_Vec2 px0, lum_Vec2 px1) {
  rts_selection_init(s);
  s->pending = true;
  s->pending_is_click = false;
  s->start_px = px0;
  s->current_px = px1;
  rts_selection_resolve(s, ecs, cam, win);
}

int main(void) {
  lum_log_init();
  lum_log_set_min_level(Lum_Log_Level_Error);

  lum_Ecs* ecs = lum_ecs_create();
  if (!ecs) { printf("ecs create failed\n"); return 1; }
  lum_scene_init(ecs);
  rts_units_register(ecs);

  /* 4 个已知位置的单位：三个在视野内，一个在视野外（x=1000 > 视野半宽 640）。
     单位边长 20 → 半宽 10。 */
  const lum_Vec2 pos[4] = { {0,0}, {50,0}, {100,0}, {1000,0} };
  const lum_Vec4 col = lum_vec4_create(1,1,1,1);
  for (int i = 0; i < 4; ++i) {
    if (rts_units_spawn(ecs, pos[i], SIDE, col, 0, NULL) == Lum_Entity_Null) {
      printf("  spawn %d failed\n", i); return 1;
    }
  }

  /* 跑一步：Input..Commit，其中包含 Transform —— world 矩阵在这里被算出来 */
  lum_flow_run_steps(ecs, 1u, 1.0f / 60.0f);

  /* 验证 world 矩阵确实已派生（这也是"结算必须放 Post"的前提） */
  {
    lum_Ecs_view v = lum_ecs_view_create2(ecs, lum_Component_transform_id, rts_units_type());
    lum_Component_transform* t; rts_Unit* u;
    int bad = 0, n = 0;
    while (lum_ecs_view_each2(&v, (void**)&t, (void**)&u) != Lum_Entity_Null) {
      float wx = t->world.m[12], wy = t->world.m[13];
      if (fabsf(wx - pos[n].x) > 0.01f || fabsf(wy - pos[n].y) > 0.01f) ++bad;
      ++n;
    }
    printf("  %-46s %s\n", "Transform 阶段已算出 world 平移",
           (n == 4 && bad == 0) ? "ok" : "★FAIL★");
    if (n != 4 || bad != 0) ++g_fail;
  }

  lum_Camera2d cam;
  lum_camera2d_init(&cam, WIN_W, WIN_H);
  lum_Vec2 win = lum_vec2_create(WIN_W, WIN_H);
  rts_Selection s;

  printf("\n[1] 全屏框选：应命中视野内 3 个（x=1000 那个在画面外）\n");
  box_select_screen(&s, ecs, &cam, win, lum_vec2_create(0, 0), lum_vec2_create(WIN_W, WIN_H));
  check("total_count", s.total_count, 4);
  check("selected_count", s.selected_count, 3);

  printf("\n[2] 小框 world x∈[-20,30]：只罩住 (0,0) 那个\n");
  box_select(&s, ecs, &cam, win, lum_vec2_create(-20, 20), lum_vec2_create(30, -20));
  check("selected_count", s.selected_count, 1);

  printf("\n[3] 反向拖拽（从右下拖到左上）结果应相同\n");
  box_select(&s, ecs, &cam, win, lum_vec2_create(30, -20), lum_vec2_create(-20, 20));
  check("selected_count", s.selected_count, 1);

  printf("\n[4] 边界语义：框右边界正好压在 (50,0) 的 AABB 左边缘(x=40)\n");
  printf("      与 lum_scene_pick 一致 —— 闭区间，相切算命中，所以是 2 个\n");
  box_select(&s, ecs, &cam, win, lum_vec2_create(-20, 20), lum_vec2_create(40, -20));
  check("selected_count", s.selected_count, 2);

  printf("\n[5] 单击 (0,0) 那个\n");
  {
    rts_selection_init(&s);
    s.pending = true; s.pending_is_click = true;
    s.pending_point_px = screen_of_world(lum_vec2_create(0, 0));
    rts_selection_resolve(&s, ecs, &cam, win);
    check("selected_count", s.selected_count, 1);
  }

  printf("\n[6] 单击空白处：清空选择\n");
  {
    rts_selection_init(&s);
    s.pending = true; s.pending_is_click = true;
    s.pending_point_px = screen_of_world(lum_vec2_create(-500, -300));
    rts_selection_resolve(&s, ecs, &cam, win);
    check("selected_count", s.selected_count, 0);
  }

  printf("\n[7] 相机平移后同一个屏幕框应命中不同单位（证明用的是传入的相机，不是全局）\n");
  {
    cam.position = lum_vec2_create(100.0f, 0.0f);   /* 镜头右移 100 世界单位 */
    box_select(&s, ecs, &cam, win, lum_vec2_create(-20, 20), lum_vec2_create(30, -20));
    /* 该屏幕框对应的世界区间整体右移 100 → 命中 (100,0) */
    check("selected_count", s.selected_count, 1);
  }

  printf("\n[8] 缩小到 zoom 0.5：视野半宽变 1280，x=1000 也进来了\n");
  {
    cam.position = lum_vec2_0();
    cam.zoom = 0.5f;
    box_select_screen(&s, ecs, &cam, win, lum_vec2_create(0, 0), lum_vec2_create(WIN_W, WIN_H));
    check("selected_count", s.selected_count, 4);
    cam.zoom = 1.0f;
  }

  /* ---- 以下是输入状态机：拖拽序列与指针越界（用显式鼠标状态，故无需窗口）---- */

  printf("\n[9] 完整拖拽序列（按下→移动→松开）应选中 (0,0) 那个\n");
  {
    lum_Vec2 from = screen_of_world(lum_vec2_create(-20, -20));   /* 左下 */
    lum_Vec2 to   = screen_of_world(lum_vec2_create(30, 20));     /* 右上 */
    lum_Vec2 mid  = lum_vec2_scale(lum_vec2_add(from, to), 0.5f);
    rts_selection_init(&s);
    rts_selection_input(&s, mouse_at(from, true, true),  win, false);   /* 按下 */
    rts_selection_input(&s, mouse_at(mid,  false, true), win, false);   /* 拖动中 */
    check("拖动中 dragging", (uint32_t)s.dragging, 1);
    rts_selection_input(&s, mouse_at(to,   false, true), win, false);
    rts_selection_input(&s, mouse_at(to,   false, false), win, false);  /* 松开 */
    check("松开后 dragging", (uint32_t)s.dragging, 0);
    check("pending 已置位", (uint32_t)s.pending, 1);
    rts_selection_resolve(&s, ecs, &cam, win);
    check("selected_count", s.selected_count, 1);
  }

  printf("\n[10] 指针被拖到窗口外：端点应夹到窗口边界，仍能选中全屏内 3 个\n");
  {
    rts_selection_init(&s);
    rts_selection_input(&s, mouse_at(lum_vec2_create(0, 0), true, true), win, false);
    /* 越界坐标（实测真实环境里拿到过 (1996,1218)，而窗口只有 1280x720） */
    rts_selection_input(&s, mouse_at(lum_vec2_create(1996.0f, 1218.0f), false, true), win, false);
    check("current 被夹到窗口右下",
          (uint32_t)(s.current_px.x == WIN_W && s.current_px.y == WIN_H), 1);
    rts_selection_input(&s, mouse_at(lum_vec2_create(1996.0f, 1218.0f), false, false), win, false);
    rts_selection_resolve(&s, ecs, &cam, win);
    check("selected_count", s.selected_count, 3);
  }

  printf("\n[11] 指针在窗口外按下：不应起手\n");
  {
    rts_selection_init(&s);
    rts_selection_input(&s, mouse_at(lum_vec2_create(1996.0f, 1218.0f), true, true), win, false);
    check("dragging 应保持 0", (uint32_t)s.dragging, 0);
  }

  printf("\n[12] 位移小于 slop 算单击而不是框选\n");
  {
    rts_selection_init(&s);
    rts_selection_input(&s, mouse_at(lum_vec2_create(640, 360), true, true), win, false);
    rts_selection_input(&s, mouse_at(lum_vec2_create(641, 361), false, true), win, false);
    rts_selection_input(&s, mouse_at(lum_vec2_create(641, 361), false, false), win, false);
    check("pending_is_click", (uint32_t)s.pending_is_click, 1);
  }

  lum_ecs_destroy(ecs);
  printf("\n==== %s（失败 %d 项）====\n", g_fail == 0 ? "全部通过" : "有失败", g_fail);
  return g_fail == 0 ? 0 : 1;
}

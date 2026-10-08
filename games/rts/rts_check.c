/* M1 + M2 无窗口验证：框选/单击命中的数学、阶段契约，以及单位移动指令。
   不需要 GL、不需要窗口 —— 直接建 ECS、跑一步步进（含 Transform）再结算选择。
   移动指令直接调 rts_orders_* 并把 dt 显式传进去，所以每一步的位移都是确定的。 */
#include "luminous.h"
#include "rts_collision.h"
#include "rts_navgrid.h"
#include "rts_orders.h"
#include "rts_path.h"
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

/* 浮点断言：位置/距离这类量不能按整数比 */
static void checkf(const char* what, float got, float want, float tol) {
  int ok = (fabsf(got - want) <= tol);
  if (!ok) ++g_fail;
  printf("  %-46s got %.4f  want %.4f (±%.4f)   %s\n", what, (double)got,
         (double)want, (double)tol, ok ? "ok" : "★FAIL★");
}

/* 自算一份 octile（八向网格的精确距离，单位 = 格），用来独立验证实现发出来的 h。
   刻意在测试里重写一遍公式，而不是调实现内部的 rts__h —— 否则等于拿实现验实现。 */
static float octile_cells(int32_t ax, int32_t ay, int32_t bx, int32_t by) {
  const int32_t dx = (ax > bx) ? (ax - bx) : (bx - ax);
  const int32_t dy = (ay > by) ? (ay - by) : (by - ay);
  const int32_t mn = (dx < dy) ? dx : dy;
  const int32_t mx = (dx < dy) ? dy : dx;
  return (float)(mx - mn) + 1.41421356f * (float)mn;
}

/* ---------- 移动指令用的小工具 ---------- */

/* 清掉全部选中与指令，让每个用例从干净状态开始 */
static void reset_selection_and_orders(lum_Ecs* ecs) {
  lum_Ecs_view v = lum_ecs_view_create(ecs, rts_units_type());
  rts_Unit* u;
  while (lum_ecs_view_each(&v, (void**)&u) != Lum_Entity_Null) {
    u->selected = 0;
    u->has_move = 0;
  }
}

static uint32_t select_all(lum_Ecs* ecs) {
  uint32_t n = 0;
  lum_Ecs_view v = lum_ecs_view_create(ecs, rts_units_type());
  rts_Unit* u;
  while (lum_ecs_view_each(&v, (void**)&u) != Lum_Entity_Null) { u->selected = 1; ++n; }
  return n;
}

/* 取 dense 序第 idx 个单位的 transform 与 unit。
   返回的指针在下一次【同类型组件 add】之前有效 —— 这些用例都不 add，所以安全。 */
static bool unit_by_index(lum_Ecs* ecs, uint32_t idx,
                          lum_Component_transform** out_t, rts_Unit** out_u) {
  uint32_t i = 0;
  lum_Ecs_view v = lum_ecs_view_create2(ecs, lum_Component_transform_id, rts_units_type());
  lum_Component_transform* t;
  rts_Unit* u;
  while (lum_ecs_view_each2(&v, (void**)&t, (void**)&u) != Lum_Entity_Null) {
    if (i == idx) { *out_t = t; *out_u = u; return true; }
    ++i;
  }
  return false;
}

/* ---------- 寻路用的小工具 ---------- */

/* 一堵带缺口的竖墙（和默认布局同一套，但不放方柱）：唯一通路在 cy 46..48。
   cy 46 → 世界 y 448；cy 49 → 544。两军都在 y ≈ -90..42，所以必须北上绕。 */
static void nav_wall(rts_Navgrid* nav) {
  rts_navgrid_init(nav);
  rts_navgrid_add_obstacle(nav, 31,  0, 1, 45);   /* cy 0..44  */
  rts_navgrid_add_obstacle(nav, 31, 50, 1, 14);   /* cy 50..63 */
}

/* 把某个单位放到指定世界位置 */
static void place_unit_at(lum_Ecs* ecs, uint32_t idx, lum_Vec2 pos) {
  lum_Component_transform* t; rts_Unit* u;
  if (unit_by_index(ecs, idx, &t, &u)) t->position = pos;
}

/* 只选中某一个单位（单单位编队 = 1×1，目标点就是要下令的点，便于精确断言） */
static void select_only(lum_Ecs* ecs, uint32_t idx) {
  reset_selection_and_orders(ecs);
  lum_Component_transform* t; rts_Unit* u;
  if (unit_by_index(ecs, idx, &t, &u)) u->selected = 1;
}

/* 从 start 出发，沿路点把总长度算出来 */
static float path_length(const rts_Path* p, lum_Vec2 start) {
  float len = 0.0f;
  lum_Vec2 prev = start;
  for (uint8_t i = 0; i < p->count; ++i) {
    const float dx = p->points[i].x - prev.x;
    const float dy = p->points[i].y - prev.y;
    len += sqrtf(dx * dx + dy * dy);
    prev = p->points[i];
  }
  return len;
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

  printf("\n[13] 选中必须【跨帧保持】—— 这是\"只选中一瞬间\"的回归\n");
  {
    cam.position = lum_vec2_0();
    cam.zoom = 1.0f;
    box_select(&s, ecs, &cam, win, lum_vec2_create(-20, 20), lum_vec2_create(30, -20));
    check("第 1 帧（有请求）selected", s.selected_count, 1);

    /* 之后连续几帧都没有新请求。早先的实现把"清空"放在 resolve 开头无条件执行，
       所以这几帧会把选择抹掉 —— 必须仍然保持 1。 */
    for (int i = 0; i < 3; ++i) rts_selection_resolve(&s, ecs, &cam, win);
    check("第 4 帧（无请求）selected 保持", s.selected_count, 1);

    /* 不只查计数，直接查组件里的标志 */
    uint32_t flagged = 0;
    {
      lum_Ecs_view v = lum_ecs_view_create(ecs, rts_units_type());
      rts_Unit* u;
      while (lum_ecs_view_each(&v, (void**)&u) != Lum_Entity_Null) {
        if (u->selected) ++flagged;
      }
    }
    check("组件 selected 标志仍是 1", flagged, 1);
  }

  printf("\n[14] 单击选中同样要跨帧保持\n");
  {
    rts_selection_init(&s);
    s.pending = true; s.pending_is_click = true;
    s.pending_point_px = screen_of_world(lum_vec2_create(0, 0));
    rts_selection_resolve(&s, ecs, &cam, win);
    check("第 1 帧 selected", s.selected_count, 1);
    for (int i = 0; i < 3; ++i) rts_selection_resolve(&s, ecs, &cam, win);
    check("第 4 帧 selected 保持", s.selected_count, 1);
  }

  printf("\n[15] 新的空框选应能覆盖旧选择（清空仍要生效）\n");
  {
    box_select_screen(&s, ecs, &cam, win,
                      lum_vec2_create(10, 660), lum_vec2_create(60, 700));   /* 空白角落 */
    check("selected_count", s.selected_count, 0);
    for (int i = 0; i < 2; ++i) rts_selection_resolve(&s, ecs, &cam, win);
    check("之后仍为 0", s.selected_count, 0);
  }

  /* ══════════════ M2：单位移动指令 ══════════════
     这些用例直接调 rts_orders_*，dt 显式传入 —— 不依赖引擎的帧循环，
     所以每一步的位移都是可精确推算的。 */

  printf("\n[16] 下发移动指令：选中单位朝各自终点推进\n");
  {
    reset_selection_and_orders(ecs);
    check("选中数", select_all(ecs), 4);
    const lum_Vec2 dst = lum_vec2_create(400.0f, 100.0f);
    check("接到指令的单位数", rts_orders_move_selected(ecs, NULL, dst), 4);
    check("active_count", rts_orders_active_count(ecs), 4);

    float before[4];
    for (uint32_t i = 0; i < 4; ++i) {
      lum_Component_transform* t; rts_Unit* u;
      unit_by_index(ecs, i, &t, &u);
      const float dx = u->move_target.x - t->position.x;
      const float dy = u->move_target.y - t->position.y;
      before[i] = sqrtf(dx * dx + dy * dy);
    }

    rts_orders_update(ecs, NULL, 1.0f / 60.0f);

    uint32_t closer = 0;
    for (uint32_t i = 0; i < 4; ++i) {
      lum_Component_transform* t; rts_Unit* u;
      unit_by_index(ecs, i, &t, &u);
      const float dx = u->move_target.x - t->position.x;
      const float dy = u->move_target.y - t->position.y;
      if (sqrtf(dx * dx + dy * dy) < before[i] - 0.5f) ++closer;
    }
    check("四个单位都更接近自己的终点", closer, 4);
    check("1/60 秒走不完，指令仍挂着", rts_orders_active_count(ecs), 4);
  }

  printf("\n[17] 到点：精确落在终点、指令清除、之后不再动\n");
  {
    uint32_t guard = 0;
    while (rts_orders_active_count(ecs) > 0 && guard < 5000u) {
      rts_orders_update(ecs, NULL, 1.0f / 30.0f);
      ++guard;
    }
    check("未超步数上限即全部到点", (uint32_t)(guard < 5000u), 1);
    check("active_count 归零", rts_orders_active_count(ecs), 0);

    uint32_t exact = 0;
    float px[4], py[4];
    for (uint32_t i = 0; i < 4; ++i) {
      lum_Component_transform* t; rts_Unit* u;
      unit_by_index(ecs, i, &t, &u);
      px[i] = t->position.x; py[i] = t->position.y;
      if (fabsf(t->position.x - u->move_target.x) < 0.001f &&
          fabsf(t->position.y - u->move_target.y) < 0.001f) ++exact;
    }
    check("四个单位都精确落在自己的 move_target 上", exact, 4);

    for (int k = 0; k < 5; ++k) rts_orders_update(ecs, NULL, 1.0f / 30.0f);
    uint32_t still = 0;
    for (uint32_t i = 0; i < 4; ++i) {
      lum_Component_transform* t; rts_Unit* u;
      unit_by_index(ecs, i, &t, &u);
      if (t->position.x == px[i] && t->position.y == py[i]) ++still;
    }
    check("到点后位置逐位不变（无滑行/无振荡）", still, 4);
  }

  printf("\n[18] 单步位移 == speed×dt；一步能到就落点、绝不越过\n");
  {
    reset_selection_and_orders(ecs);
    lum_Component_transform* t; rts_Unit* u;
    unit_by_index(ecs, 0, &t, &u);
    t->position = lum_vec2_create(0.0f, 0.0f);
    u->speed = 90.0f;
    u->selected = 1;

    /* 只选 1 个 → 编队是 1×1 → 终点就是下令点本身 */
    check("只给 1 个单位下令", rts_orders_move_selected(ecs, NULL, lum_vec2_create(1000.0f, 0.0f)), 1);
    unit_by_index(ecs, 0, &t, &u);
    checkf("1×1 编队的终点 == 下令点 x", u->move_target.x, 1000.0f, 0.001f);
    checkf("1×1 编队的终点 == 下令点 y", u->move_target.y, 0.0f, 0.001f);

    rts_orders_update(ecs, NULL, 1.0f / 60.0f);
    checkf("单步位移", t->position.x, 90.0f / 60.0f, 0.001f);
    checkf("侧向不漂", t->position.y, 0.0f, 0.0001f);

    /* 近目标：这一步（90×1.0）远超 10，必须精确停在终点而不是冲过去 */
    t->position = lum_vec2_create(0.0f, 0.0f);
    u->has_move = 0;
    rts_orders_move_selected(ecs, NULL, lum_vec2_create(10.0f, 0.0f));
    unit_by_index(ecs, 0, &t, &u);
    rts_orders_update(ecs, NULL, 1.0f);
    checkf("一步到位：精确落在终点", t->position.x, 10.0f, 0.0001f);
    check("指令已清除", (uint32_t)u->has_move, 0);
  }

  printf("\n[19] dt <= 0 一律不推进（非步进阶段 ctx->dt 就是 0）\n");
  {
    reset_selection_and_orders(ecs);
    lum_Component_transform* t; rts_Unit* u;
    unit_by_index(ecs, 0, &t, &u);
    t->position = lum_vec2_create(0.0f, 0.0f);
    u->selected = 1;
    rts_orders_move_selected(ecs, NULL, lum_vec2_create(500.0f, 0.0f));
    unit_by_index(ecs, 0, &t, &u);

    rts_orders_update(ecs, NULL, 0.0f);
    checkf("dt=0 后位置未变", t->position.x, 0.0f, 0.0001f);
    rts_orders_update(ecs, NULL, -1.0f);
    checkf("dt<0 后位置未变", t->position.x, 0.0f, 0.0001f);
    check("指令仍挂着（没被 dt=0 顺手吞掉）", rts_orders_active_count(ecs), 1);
  }

  printf("\n[20] speed=0 的单位不会动，也不该一直占着指令\n");
  {
    reset_selection_and_orders(ecs);
    lum_Component_transform* t; rts_Unit* u;
    unit_by_index(ecs, 0, &t, &u);
    t->position = lum_vec2_create(0.0f, 0.0f);
    u->selected = 1;
    u->speed = 0.0f;

    rts_orders_move_selected(ecs, NULL, lum_vec2_create(500.0f, 0.0f));
    unit_by_index(ecs, 0, &t, &u);
    rts_orders_update(ecs, NULL, 1.0f / 60.0f);
    checkf("位置未变", t->position.x, 0.0f, 0.0001f);
    check("指令已被清除", (uint32_t)u->has_move, 0);

    u->speed = 90.0f;   /* 复位，后面的用例还要用 */
  }

  printf("\n[21] 编队展开：终点两两不同，重心落在下令点上\n");
  {
    reset_selection_and_orders(ecs);
    check("选中数", select_all(ecs), 4);
    const lum_Vec2 dst = lum_vec2_create(-120.0f, 260.0f);
    check("接到指令的单位数", rts_orders_move_selected(ecs, NULL, dst), 4);

    lum_Vec2 tg[4];
    for (uint32_t i = 0; i < 4; ++i) {
      lum_Component_transform* t; rts_Unit* u;
      unit_by_index(ecs, i, &t, &u);
      tg[i] = u->move_target;
    }

    uint32_t distinct = 1;
    for (uint32_t i = 0; i < 4; ++i) {
      for (uint32_t j = i + 1; j < 4; ++j) {
        if (fabsf(tg[i].x - tg[j].x) < 0.001f && fabsf(tg[i].y - tg[j].y) < 0.001f) distinct = 0;
      }
    }
    check("四个终点两两不同（没有叠在一点）", distinct, 1);

    float sx = 0.0f, sy = 0.0f;
    for (uint32_t i = 0; i < 4; ++i) { sx += tg[i].x; sy += tg[i].y; }
    checkf("终点重心 x == 下令点", sx / 4.0f, dst.x, 0.001f);
    checkf("终点重心 y == 下令点", sy / 4.0f, dst.y, 0.001f);
    checkf("相邻终点间距 == 边长×Spread", fabsf(tg[1].x - tg[0].x),
           SIDE * Rts_Orders_Spread, 0.001f);
  }

  printf("\n[22] 未选中的单位不接指令；没有选中单位时下令返回 0\n");
  {
    reset_selection_and_orders(ecs);
    lum_Component_transform* t; rts_Unit* u;
    unit_by_index(ecs, 0, &t, &u);
    u->selected = 1;                       /* 只选 1 个 */
    check("只给 1 个单位下令", rts_orders_move_selected(ecs, NULL, lum_vec2_create(0.0f, 200.0f)), 1);
    check("active_count", rts_orders_active_count(ecs), 1);

    uint32_t idle = 0;
    for (uint32_t i = 1; i < 4; ++i) {
      lum_Component_transform* ti; rts_Unit* ui;
      unit_by_index(ecs, i, &ti, &ui);
      if (!ui->has_move) ++idle;
    }
    check("另外三个单位没有指令", idle, 3);

    reset_selection_and_orders(ecs);        /* 全部取消选中 */
    check("空选择下令返回 0", rts_orders_move_selected(ecs, NULL, lum_vec2_create(10.0f, 10.0f)), 0);
    check("没有指令挂着", rts_orders_active_count(ecs), 0);
  }

  printf("\n[23] 重复下令覆盖旧终点（M2 还没有命令队列）\n");
  {
    reset_selection_and_orders(ecs);
    lum_Component_transform* t; rts_Unit* u;
    unit_by_index(ecs, 0, &t, &u);
    u->selected = 1;
    rts_orders_move_selected(ecs, NULL, lum_vec2_create(100.0f, 0.0f));
    rts_orders_move_selected(ecs, NULL, lum_vec2_create(0.0f, 300.0f));
    unit_by_index(ecs, 0, &t, &u);
    checkf("终点 x 已改为 0", u->move_target.x, 0.0f, 0.001f);
    checkf("终点 y 已改为 300", u->move_target.y, 300.0f, 0.001f);
    check("仍然只有 1 条指令", rts_orders_active_count(ecs), 1);
  }

  /* ══════════════ M3：网格 A* 寻路 ══════════════ */

  printf("\n[24] 导航网格基本性质：膨胀、越界、格子↔世界换算\n");
  {
    rts_Navgrid nav;
    rts_navgrid_init(&nav);
    rts_navgrid_add_obstacle(&nav, 10, 10, 2, 2);      /* 原始覆盖 (10,10)..(11,11) */

    check("原始格 (10,10) 被堵", (uint32_t)rts_navgrid_blocked(&nav, 10, 10), 1);
    check("膨胀后 (9,9) 被堵", (uint32_t)rts_navgrid_blocked(&nav, 9, 9), 1);
    check("膨胀后 (12,12) 被堵", (uint32_t)rts_navgrid_blocked(&nav, 12, 12), 1);
    check("膨胀圈外 (8,8) 可通行", (uint32_t)rts_navgrid_blocked(&nav, 8, 8), 0);
    check("膨胀圈外 (13,13) 可通行", (uint32_t)rts_navgrid_blocked(&nav, 13, 13), 0);

    check("越界 (-1,0) 算被堵", (uint32_t)rts_navgrid_blocked(&nav, -1, 0), 1);
    check("越界 (64,0) 算被堵", (uint32_t)rts_navgrid_blocked(&nav, Rts_Nav_Cells_X, 0), 1);

    int32_t cx = -1, cy = -1;
    check("网格左下角可换算", (uint32_t)rts_navgrid_world_to_cell(
          lum_vec2_create(Rts_Nav_Origin_X, Rts_Nav_Origin_Y), &cx, &cy), 1);
    check("→ 格 (0,0)", (uint32_t)(cx == 0 && cy == 0), 1);
    check("网格外一点换算失败", (uint32_t)rts_navgrid_world_to_cell(
          lum_vec2_create(Rts_Nav_Origin_X - 1.0f, 0.0f), &cx, &cy), 0);

    const lum_Vec2 c = rts_navgrid_cell_center(0, 0);
    checkf("格 (0,0) 中心 x", c.x, Rts_Nav_Origin_X + Rts_Nav_Cell_Size * 0.5f, 0.001f);
    checkf("格 (0,0) 中心 y", c.y, Rts_Nav_Origin_Y + Rts_Nav_Cell_Size * 0.5f, 0.001f);

    /* 往返一致性：任意格中心换算回格子都应得到自己 */
    check("格中心往返一致 (31,31)", (uint32_t)rts_navgrid_world_to_cell(
          rts_navgrid_cell_center(31, 31), &cx, &cy) && cx == 31 && cy == 31, 1);
  }

  printf("\n[25] A* 找到的路：终点正确、逐段直线可视、且确实绕了远路\n");
  {
    rts_Navgrid nav;
    nav_wall(&nav);

    const lum_Vec2 from = lum_vec2_create(-200.0f, 0.0f);
    const lum_Vec2 to   = lum_vec2_create(400.0f, 16.0f);
    rts_Path p;
    check("有路可走", (uint32_t)rts_path_find(&nav, from, to, &p), 1);
    printf("      路点数 %u（truncated=%u）\n", (unsigned)p.count, (unsigned)p.truncated);

    /* 终点必须等于"夹到可通行格中心"的那个点 */
    const lum_Vec2 goal = rts_navgrid_clamp_to_free(&nav, to);
    checkf("最后一个路点 x == 夹后的终点", p.points[p.count - 1].x, goal.x, 0.001f);
    checkf("最后一个路点 y == 夹后的终点", p.points[p.count - 1].y, goal.y, 0.001f);

    /* 每一段都必须按【和 A* 同一条规则】直线可通 —— 否则就是"拉直切了角" */
    uint32_t clear = 1;
    lum_Vec2 prev = from;
    for (uint8_t i = 0; i < p.count; ++i) {
      if (!rts_navgrid_line_clear(&nav, prev, p.points[i])) clear = 0;
      prev = p.points[i];
    }
    check("每一段都直线可视（含禁止切角）", clear, 1);

    /* 绕路：墙把直路挡死，路径总长必须显著大于直线距离 */
    const float straight = sqrtf((to.x - from.x) * (to.x - from.x) + (to.y - from.y) * (to.y - from.y));
    const float len = path_length(&p, from);
    printf("      直线 %.1f   路径 %.1f（%.2f 倍）\n", (double)straight, (double)len,
           (double)(len / straight));
    check("路径长度 > 1.5 × 直线距离（确实绕了）", (uint32_t)(len > straight * 1.5f), 1);
  }

  printf("\n[26] 拉直的效果：路点数远少于经过的格子数\n");
  {
    rts_Navgrid nav;
    nav_wall(&nav);
    const lum_Vec2 from = lum_vec2_create(-200.0f, 0.0f);
    const lum_Vec2 to   = lum_vec2_create(400.0f, 16.0f);
    rts_Path p;
    rts_path_find(&nav, from, to, &p);

    /* 经过的格子数 ≈ 路径长度 / 格边长（A* 的原始输出就是这么多格） */
    const float len = path_length(&p, from);
    const uint32_t cells_crossed = (uint32_t)(len / Rts_Nav_Cell_Size);
    printf("      经过约 %u 格，拉直后只剩 %u 个路点\n",
           (unsigned)cells_crossed, (unsigned)p.count);
    check("路点数远少于格子数（真的拉直了）", (uint32_t)(p.count * 4u < cells_crossed), 1);
    check("没有被截断", (uint32_t)p.truncated, 0);
  }

  printf("\n[27] 终点落在墙上：夹到最近可通行格，且这条路确实走得通\n");
  {
    rts_Navgrid nav;
    rts_navgrid_init(&nav);
    rts_navgrid_add_obstacle(&nav, 10, 10, 4, 4);      /* 膨胀后 (9,9)..(14,14) 全堵 */

    /* 点在障碍正中心 (12,12) 的格中心 */
    const lum_Vec2 on_wall = rts_navgrid_cell_center(12, 12);
    const lum_Vec2 forced  = rts_navgrid_clamp_to_free(&nav, on_wall);
    int32_t fx = -1, fy = -1;
    check("夹出来的点能换算成格子", (uint32_t)rts_navgrid_world_to_cell(forced, &fx, &fy), 1);
    check("夹出来的格子是可通行的", (uint32_t)!rts_navgrid_blocked(&nav, fx, fy), 1);
    check("夹出来的点不在原点（真的挪了）",
          (uint32_t)(fabsf(forced.x - on_wall.x) > 0.001f || fabsf(forced.y - on_wall.y) > 0.001f), 1);

    rts_Path p;
    check("从远处能走到这个夹后的点",
          (uint32_t)rts_path_find(&nav, rts_navgrid_cell_center(2, 2), forced, &p), 1);
    checkf("终点就是夹后的点 x", p.points[p.count - 1].x, forced.x, 0.001f);
  }

  printf("\n[28] 没有路时明确失败（不返回半截路径）\n");
  {
    /* 把整张图铺满障碍：16 个 16×16 的块正好盖住 64×64（膨胀还会互相重叠） */
    rts_Navgrid nav;
    rts_navgrid_init(&nav);
    for (int32_t y = 0; y < Rts_Nav_Cells_Y; y += 16) {
      for (int32_t x = 0; x < Rts_Nav_Cells_X; x += 16) {
        rts_navgrid_add_obstacle(&nav, x, y, 16, 16);
      }
    }
    rts_Path p;
    check("整张图被堵 → 寻路失败", (uint32_t)rts_path_find(
          &nav, rts_navgrid_cell_center(2, 2), rts_navgrid_cell_center(50, 50), &p), 0);
    check("失败时路点数为 0", p.count, 0);

    /* 起点在网格外也是失败 */
    rts_Navgrid ok;
    rts_navgrid_init(&ok);
    check("起点在网格外 → 失败", (uint32_t)rts_path_find(
          &ok, lum_vec2_create(99999.0f, 0.0f), rts_navgrid_cell_center(10, 10), &p), 0);
  }

  printf("\n[29] 沿路径走完全程：中途逐点采样，绝不落在被堵格里\n");
  {
    rts_Navgrid nav;
    nav_wall(&nav);

    select_only(ecs, 0);
    place_unit_at(ecs, 0, lum_vec2_create(-200.0f, 0.0f));
    lum_Component_transform* t; rts_Unit* u;
    unit_by_index(ecs, 0, &t, &u);

    const lum_Vec2 to = lum_vec2_create(400.0f, 16.0f);
    check("下令成功", rts_orders_move_selected(ecs, &nav, to), 1);
    unit_by_index(ecs, 0, &t, &u);
    check("最终目标已被夹到可通行格",
          (uint32_t)!rts_navgrid_blocked(&nav,
              (int32_t)((u->move_target.x - Rts_Nav_Origin_X) / Rts_Nav_Cell_Size),
              (int32_t)((u->move_target.y - Rts_Nav_Origin_Y) / Rts_Nav_Cell_Size)), 1);

    /* 每步都采样当前位置：它必须始终落在可通行格里 */
    uint32_t steps = 0, in_wall = 0;
    float travelled = 0.0f;
    lum_Vec2 prev = t->position;
    while (rts_orders_active_count(ecs) > 0 && steps < 5000u) {
      rts_orders_update(ecs, &nav, 1.0f / 30.0f);
      unit_by_index(ecs, 0, &t, &u);

      int32_t px = -1, py = -1;
      if (!rts_navgrid_world_to_cell(t->position, &px, &py) ||
          rts_navgrid_blocked(&nav, px, py)) {
        ++in_wall;
      }
      const float dx = t->position.x - prev.x, dy = t->position.y - prev.y;
      travelled += sqrtf(dx * dx + dy * dy);
      prev = t->position;
      ++steps;
    }

    printf("      走了 %u 步，累计 %.1f 单位\n", (unsigned)steps, (double)travelled);
    check("在步数上限内走到了", (uint32_t)(steps < 5000u), 1);
    check("active_count 归零", rts_orders_active_count(ecs), 0);
    check("全程没有一步落在被堵格里", in_wall, 0);
    checkf("终点精确落在 move_target 上 x", t->position.x, u->move_target.x, 0.0001f);
    checkf("终点精确落在 move_target 上 y", t->position.y, u->move_target.y, 0.0001f);
    check("走的总长度 > 直线距离（绕了路，不是穿墙）",
          (uint32_t)(travelled > 900.0f), 1);
  }

  printf("\n[30] nav 传 NULL 退化成直线：一个路点，且不看障碍\n");
  {
    rts_Navgrid nav;
    nav_wall(&nav);

    select_only(ecs, 1);
    place_unit_at(ecs, 1, lum_vec2_create(-200.0f, 0.0f));
    lum_Component_transform* t; rts_Unit* u;
    unit_by_index(ecs, 1, &t, &u);

    /* 终点故意选在墙的另一侧：有网格时会绕，没网格时就是一条直线过去 */
    const lum_Vec2 to = lum_vec2_create(400.0f, 0.0f);
    check("下令成功", rts_orders_move_selected(ecs, NULL, to), 1);
    unit_by_index(ecs, 1, &t, &u);
    check("只有一个路点（直线）", (uint32_t)u->path.count, 1);
    checkf("路点 x == 下令点", u->path.points[0].x, to.x, 0.001f);
    checkf("路点 y == 下令点", u->path.points[0].y, to.y, 0.001f);
    check("没有网格时不夹取终点（保留原始点）",
          (uint32_t)(fabsf(u->move_target.x - to.x) < 0.001f), 1);

    /* 拉直后的路径长度就等于直线距离 */
    checkf("路长 == 直线距离", path_length(&u->path, t->position),
           sqrtf((to.x - t->position.x) * (to.x - t->position.x) +
                 (to.y - t->position.y) * (to.y - t->position.y)), 0.001f);
  }

  printf("\n[31] 禁止切角：对角邻格被堵时不许斜穿（否则会走穿角捷径）\n");
  {
    rts_Navgrid nav;
    rts_navgrid_init(&nav);
    /* 手工【只堵两格】—— 绕开膨胀那一圈，才能造出"斜线正好从两格夹角上过"的局面。
       (2,1) 与 (1,2) 斜对角相邻，而 (1,1)↔(2,2) 的斜线正好穿过它们夹的那个角。
       这两格本身都不在任何 A* 直线上，所以【只有"禁止切角"这条规则能挡住它】。 */
    nav.blocked[1 * Rts_Nav_Cells_X + 2] = 1;   /* (2,1) */
    nav.blocked[2 * Rts_Nav_Cells_X + 1] = 1;   /* (1,2) */

    const lum_Vec2 start = rts_navgrid_cell_center(0, 0);
    const lum_Vec2 goal = rts_navgrid_cell_center(3, 3);
    rts_Path p;
    check("有路可走", (uint32_t)rts_path_find(&nav, start, goal, &p), 1);

    const float len = path_length(&p, start);
    const float shortcut = sqrtf(18.0f) * Rts_Nav_Cell_Size;   /* 直穿 (0,0)→(3,3) = 3√2 格 */
    printf("      路点 %u 个，全长 %.2f（穿角捷径只要 %.2f）\n",
           (unsigned)p.count, (double)len, (double)shortcut);
    checkf("绕开夹角：全长 == 6 格", len, 6.0f * Rts_Nav_Cell_Size, 0.5f);
    check("明显长于穿角捷径", (uint32_t)(len > shortcut + 40.0f), 1);

    /* 每段按 1 世界单位密集采样，任何一点都不得落在被堵的那两格里 */
    uint32_t bad = 0;
    lum_Vec2 prev = start;
    for (uint8_t i = 0; i < p.count; ++i) {
      const float dx = p.points[i].x - prev.x;
      const float dy = p.points[i].y - prev.y;
      const int32_t steps = (int32_t)sqrtf(dx * dx + dy * dy) + 1;
      for (int32_t k = 0; k <= steps; ++k) {
        const float t = (float)k / (float)steps;
        int32_t cx = -1, cy = -1;
        if (!rts_navgrid_world_to_cell(lum_vec2_create(prev.x + dx * t, prev.y + dy * t),
                                       &cx, &cy)) { ++bad; continue; }
        if (rts_navgrid_blocked(&nav, cx, cy)) ++bad;
      }
      prev = p.points[i];
    }
    check("逐点采样：没有一步落在 (2,1)/(1,2) 上", bad, 0);

    /* 对照组：把这两格放开，就该允许直穿 —— 全长退回 3√2 格。
       有了这个对照，"192" 才能被解释成"因为禁止切角而绕路"，而不是随便一个数。 */
    rts_Navgrid open_nav;
    rts_navgrid_init(&open_nav);
    rts_Path po;
    check("放开后仍能找到路", (uint32_t)rts_path_find(&open_nav, start, goal, &po), 1);
    printf("      放开后全长 %.2f（= 3√2 格，直穿）\n", (double)path_length(&po, start));
    checkf("放开后就是直穿", path_length(&po, start), shortcut, 0.5f);
  }

  printf("\n[32] 启发式真的在省事：定案格数远小于总格数\n");
  {
    /* 这一条盯的是"路径依然正确、但启发式被写坏"的情形。
       把 h 改成恒 0 就是 Dijkstra：路径照样最优，前面所有测试照样全绿，
       只有"定案了多少格"会暴涨。没有这个读数，这种退化查不出来。 */
    rts_Navgrid nav;
    nav_wall(&nav);
    rts_Path p;

    /* 远距离 + 要绕墙：A* 应当只探索"朝终点的一条带" */
    const lum_Vec2 far_from = lum_vec2_create(-900.0f, -900.0f);
    const lum_Vec2 far_to   = lum_vec2_create( 900.0f,  900.0f);
    check("远距离仍能找到路", (uint32_t)rts_path_find(&nav, far_from, far_to, &p), 1);
    const uint32_t expanded_far = rts_path_last_expanded();
    printf("      远距离：定案 %u / %d 格（%.1f%%）\n", (unsigned)expanded_far,
           Rts_Nav_Max_Cells, (double)expanded_far * 100.0 / (double)Rts_Nav_Max_Cells);
    check("定案数远小于总格数（< 40%）",
          (uint32_t)(expanded_far * 100u < (uint32_t)Rts_Nav_Max_Cells * 40u), 1);

    /* 真正没有障碍的一小段：西侧 (-200,0) → (-120,0)，墙在东边，一路畅通。
       注意别拿"横跨墙两侧"的一对点当近距离用例 —— 那其实要北上绕一大圈，
       定案几百格才是对的。我第一版就是这么写错的，被下面这条断言抓出来。 */
    const lum_Vec2 near_from = lum_vec2_create(-200.0f, 0.0f);
    const lum_Vec2 near_to   = lum_vec2_create(-120.0f, 0.0f);
    check("近距离仍能找到路", (uint32_t)rts_path_find(&nav, near_from, near_to, &p), 1);
    const uint32_t expanded_near = rts_path_last_expanded();
    printf("      近距离：定案 %u 格\n", (unsigned)expanded_near);
    check("近距离定案数 < 200", (uint32_t)(expanded_near < 200u), 1);
    check("远距离比近距离探索得多（符合直觉）",
          (uint32_t)(expanded_far > expanded_near), 1);
  }

  printf("\n[33] 单位互斥：两个压住对方的箱子被对称推开到刚好接触\n");
  {
    rts_Navgrid nav;
    rts_navgrid_init(&nav);          /* 全空网格：只测单位之间，不带墙 */
    reset_selection_and_orders(ecs);

    place_unit_at(ecs, 0, lum_vec2_create(0.0f, 0.0f));
    place_unit_at(ecs, 1, lum_vec2_create(10.0f, 0.0f));   /* 边长 20，中心距 10 → 压了 10 */

    lum_Component_transform* t0; rts_Unit* u0;
    lum_Component_transform* t1; rts_Unit* u1;
    unit_by_index(ecs, 0, &t0, &u0);
    unit_by_index(ecs, 1, &t1, &u1);

    check("这一帧确实推了东西", (uint32_t)(rts_collision_resolve(ecs, &nav) > 0), 1);
    unit_by_index(ecs, 0, &t0, &u0);
    unit_by_index(ecs, 1, &t1, &u1);

    /* 最小重叠轴是 x（重叠 10），所以推力沿 x；两边各承担一半 = 各 5。
       这一条同时钉死了引擎的 depth 语义：depth 是【整段】平移量，各推一半才对。
       若误按"各推整个 depth"，这里会得到 -10 / 20。 */
    checkf("A 被推向 -x，各推一半", t0->position.x, -5.0f, 0.01f);
    checkf("B 被推向 +x，各推一半", t1->position.x, 15.0f, 0.01f);
    checkf("中心距正好等于边长（刚好接触，不重叠）",
           t1->position.x - t0->position.x, SIDE, 0.01f);
    checkf("y 方向没有被动过", t0->position.y, 0.0f, 0.0001f);
  }

  printf("\n[34] 本来就没碰到的两个箱子，一点都不能动\n");
  {
    rts_Navgrid nav;
    rts_navgrid_init(&nav);
    reset_selection_and_orders(ecs);

    place_unit_at(ecs, 0, lum_vec2_create(0.0f, 0.0f));
    place_unit_at(ecs, 1, lum_vec2_create(30.0f, 0.0f));   /* 30 > 边长 20，分开的 */

    lum_Component_transform* t0; rts_Unit* u0;
    lum_Component_transform* t1; rts_Unit* u1;
    unit_by_index(ecs, 0, &t0, &u0);
    unit_by_index(ecs, 1, &t1, &u1);

    check("一次都没推", rts_collision_resolve(ecs, &nav), 0);
    checkf("A 原位", t0->position.x, 0.0f, 0.0001f);
    checkf("B 原位", t1->position.x, 30.0f, 0.0001f);
  }

  printf("\n[35] 两个单位完全重合：不能算出 NaN，而且必须能分开\n");
  {
    rts_Navgrid nav;
    rts_navgrid_init(&nav);
    reset_selection_and_orders(ecs);

    /* 圆心/箱心完全一致是最容易出 NaN 的退化情形（归一化除以 0） */
    place_unit_at(ecs, 0, lum_vec2_create(40.0f, 40.0f));
    place_unit_at(ecs, 1, lum_vec2_create(40.0f, 40.0f));

    lum_Component_transform* t0; rts_Unit* u0;
    lum_Component_transform* t1; rts_Unit* u1;

    uint32_t guard = 0;
    while (rts_collision_resolve(ecs, &nav) > 0 && guard < 200u) ++guard;
    check("有限步内不再需要推", (uint32_t)(guard < 200u), 1);

    unit_by_index(ecs, 0, &t0, &u0);
    unit_by_index(ecs, 1, &t1, &u1);
    check("A 坐标不是 NaN", (uint32_t)(!(isnan(t0->position.x) || isnan(t0->position.y))), 1);
    check("B 坐标不是 NaN", (uint32_t)(!(isnan(t1->position.x) || isnan(t1->position.y))), 1);

    const float dx = fabsf(t1->position.x - t0->position.x);
    const float dy = fabsf(t1->position.y - t0->position.y);
    check("已经分开（任一轴达到边长即可）",
          (uint32_t)(dx >= SIDE - 0.06f || dy >= SIDE - 0.06f), 1);
  }

  printf("\n[36] 九个单位挤成一坨：收敛到两两不重叠，且稳定后逐位不变\n");
  {
    rts_Navgrid nav;
    rts_navgrid_init(&nav);
    reset_selection_and_orders(ecs);

    /* 场上原本只有 4 个（前面那些用例已经跑完了，这里补单位不会影响它们） */
    const lum_Vec4 spawn_col = lum_vec4_create(1, 1, 1, 1);
    for (int k = 0; k < 5; ++k) {
      if (rts_units_spawn(ecs, lum_vec2_create(0.0f, 0.0f), SIDE, spawn_col, 0, NULL) == Lum_Entity_Null) {
        printf("  补单位失败\n");
      }
    }

    /* 3×3 紧紧叠在一起：格距 6，远小于边长 20 */
    uint32_t idx = 0;
    for (int row = 0; row < 3; ++row) {
      for (int col_i = 0; col_i < 3; ++col_i) {
        place_unit_at(ecs, idx, lum_vec2_create((float)col_i * 6.0f, (float)row * 6.0f));
        ++idx;
      }
    }
    check("场上共 9 个单位", idx, 9);

    uint32_t guard = 0;
    while (rts_collision_resolve(ecs, &nav) > 0 && guard < 500u) ++guard;
    check("有限步内收敛（不再需要推）", (uint32_t)(guard < 500u), 1);
    check("再推一次已经无事可做", rts_collision_resolve(ecs, &nav), 0);

    float px[9], py[9];
    lum_Component_transform* t; rts_Unit* u;
    for (uint32_t a = 0; a < 9; ++a) {
      unit_by_index(ecs, a, &t, &u);
      px[a] = t->position.x;
      py[a] = t->position.y;
    }

    /* 箱子是轴对齐方形：把重叠判断写成"两轴都小于边长" */
    uint32_t overlapping = 0;
    for (uint32_t a = 0; a < 9; ++a) {
      for (uint32_t b = a + 1; b < 9; ++b) {
        if (fabsf(px[a] - px[b]) < SIDE - 0.06f && fabsf(py[a] - py[b]) < SIDE - 0.06f) ++overlapping;
      }
    }
    check("九个单位两两不重叠", overlapping, 0);

    /* 死区保证了真正的定点：没有可推的就一步都不动，逐位不变 */
    for (int k = 0; k < 5; ++k) rts_collision_resolve(ecs, &nav);
    uint32_t still = 0;
    for (uint32_t a = 0; a < 9; ++a) {
      unit_by_index(ecs, a, &t, &u);
      if (t->position.x == px[a] && t->position.y == py[a]) ++still;
    }
    check("稳定后位置逐位不变（没有微抖）", still, 9);
  }

  printf("\n[37] 贴着墙被挤：宁可不动，也不能被推进被堵的格子里\n");
  {
    rts_Navgrid nav;
    rts_navgrid_init(&nav);
    /* 手工只堵一格（绕过膨胀）：格 (20,0) 覆盖世界 x ∈ [-384,-352]，y ∈ [-1024,-992] */
    nav.blocked[0 * Rts_Nav_Cells_X + 20] = 1;
    reset_selection_and_orders(ecs);

    /* A 站在格 21 里、离左边界只差 1 个单位；B 在 A 右边 4 个单位处往左挤。
       推力会把 A 往左推 8，正好越过边界进入被堵的格 20。 */
    place_unit_at(ecs, 0, lum_vec2_create(-351.0f, -1008.0f));
    place_unit_at(ecs, 1, lum_vec2_create(-347.0f, -1008.0f));

    lum_Component_transform* t0; rts_Unit* u0;
    lum_Component_transform* t1; rts_Unit* u1;

    uint32_t in_wall = 0;
    for (int k = 0; k < 40; ++k) {
      rts_collision_resolve(ecs, &nav);
      unit_by_index(ecs, 0, &t0, &u0);
      int32_t cx = -1, cy = -1;
      if (!rts_navgrid_world_to_cell(t0->position, &cx, &cy) ||
          rts_navgrid_blocked(&nav, cx, cy)) ++in_wall;
    }
    check("A 全程没有一步落在被堵的格里", in_wall, 0);

    unit_by_index(ecs, 0, &t0, &u0);
    unit_by_index(ecs, 1, &t1, &u1);
    /* 墙守卫不是"推不动就冻住"，而是"能推多少推多少，推到边界为止" ——
       A 会贴着格 21 的左边界停住（-352 正好是边界，仍属格 21）。
       这比冻住好：被挤的单位会朝墙压实，而不是原地僵住。 */
    int32_t ax = -1, ay = -1;
    check("A 仍在格 21（没越进被堵的格 20）",
          (uint32_t)(rts_navgrid_world_to_cell(t0->position, &ax, &ay) && ax == 21), 1);
    check("A 没有越过边界", (uint32_t)(t0->position.x >= -352.0f - 0.001f), 1);
    checkf("A、B 仍然分开了（让步的是 B）",
           fabsf(t1->position.x - t0->position.x), SIDE, 0.06f);
  }

  printf("\n[38] 单位已经陷在被堵格里时，必须还能被推出去（不能永久冻住）\n");
  {
    /* 这是集成探针抓出来的：把单位堆到墙里之后，重叠对数一动不动 ——
       因为"不许推进被堵格"这条守卫把它自己的每一次推力都否决了。
       修法和 A* 的"起点格豁免"同源：当前就在堵格里时，目的地一律放行。 */
    rts_Navgrid nav;
    rts_navgrid_init(&nav);
    nav.blocked[0 * Rts_Nav_Cells_X + 20] = 1;   /* 只堵 (20,0)：世界 x ∈ [-384,-352] */

    reset_selection_and_orders(ecs);
    place_unit_at(ecs, 0, lum_vec2_create(-360.0f, -1000.0f));   /* 就在被堵的 (20,0) 里 */
    place_unit_at(ecs, 1, lum_vec2_create(-350.0f, -1000.0f));   /* 相邻的空格 (21,0) */

    lum_Component_transform* t0; rts_Unit* u0;
    lum_Component_transform* t1; rts_Unit* u1;
    unit_by_index(ecs, 0, &t0, &u0);
    unit_by_index(ecs, 1, &t1, &u1);

    int32_t cx = -1, cy = -1;
    check("A 一开始确实在堵格里（前提成立）",
          (uint32_t)(rts_navgrid_world_to_cell(t0->position, &cx, &cy) &&
                     rts_navgrid_blocked(&nav, cx, cy)), 1);

    const float before_x = t0->position.x;
    uint32_t guard = 0;
    while (rts_collision_resolve(ecs, &nav) > 0 && guard < 200u) ++guard;
    unit_by_index(ecs, 0, &t0, &u0);
    unit_by_index(ecs, 1, &t1, &u1);

    check("A 没有被冻住（确实被推走了）",
          (uint32_t)(t0->position.x != before_x), 1);
    checkf("最终两者不再重叠", fabsf(t1->position.x - t0->position.x), SIDE, 0.06f);
  }

  printf("\n[39] 增量推进（begin/step/take）必须与一次跑完【逐位一致】\n");
  {
    rts_Navgrid nav;
    nav_wall(&nav);
    const lum_Vec2 from = lum_vec2_create(-200.0f, 0.0f);
    const lum_Vec2 to   = lum_vec2_create(400.0f, 16.0f);

    rts_Path once;
    const bool ok_once = rts_path_find(&nav, from, to, &once);

    rts_Path_search sh;
    const bool ok_begin = rts_path_search_begin(&nav, from, to, &sh);
    uint32_t guard = 0;
    while (!sh.finished && guard < 100000u) { rts_path_search_step(&nav, &sh); ++guard; }
    rts_Path inc;
    const bool ok_take = rts_path_search_take(&nav, &sh, &inc);

    check("begin 成功", (uint32_t)ok_begin, 1);
    check("有限步内结束", (uint32_t)(guard < 100000u), 1);
    check("一次跑完也成功", (uint32_t)ok_once, 1);
    check("take 成功", (uint32_t)ok_take, 1);
    check("路点数一致", inc.count, once.count);

    uint32_t same = (inc.count == once.count) ? 1u : 0u;
    for (uint8_t i = 0; i < inc.count; ++i) {
      if (once.points[i].x != inc.points[i].x || once.points[i].y != inc.points[i].y) same = 0;
    }
    check("每个路点逐位相同", same, 1);
    check("truncated 一致", inc.truncated, once.truncated);

    /* 无路的情形也要一致：都要明确报失败，不许给半截路径 */
    rts_Navgrid sealed;
    rts_navgrid_init(&sealed);
    for (int32_t y = 0; y < Rts_Nav_Cells_Y; ++y) sealed.blocked[y * Rts_Nav_Cells_X + 30] = 1;

    rts_Path p1;
    check("封死时一次跑完失败", (uint32_t)rts_path_find(&sealed, from, to, &p1), 0);

    rts_Path_search s2;
    if (rts_path_search_begin(&sealed, from, to, &s2)) {
      uint32_t g2 = 0;
      while (!s2.finished && g2 < 100000u) { rts_path_search_step(&sealed, &s2); ++g2; }
      check("封死时增量也判定无路", (uint32_t)(s2.finished && !s2.found), 1);
      rts_Path p2;
      check("封死时 take 拒绝给路径", (uint32_t)rts_path_search_take(&sealed, &s2, &p2), 0);
    } else {
      check("封死时 begin 就失败（同样算正确）", 1, 1);
      check("封死时 take 拒绝给路径", 1, 1);
    }
  }

  printf("\n[40] “一步 = 定案一个格子”，且中途两张表都非空\n");
  {
    rts_Navgrid nav;
    nav_wall(&nav);
    rts_Path_search sh;
    check("begin 成功", (uint32_t)rts_path_search_begin(&nav, lum_vec2_create(-200.0f, 0.0f),
                                                       lum_vec2_create(400.0f, 16.0f), &sh), 1);
    check("一开始 steps 为 0", sh.steps, 0);
    check("一开始 expanded 为 0", sh.expanded, 0);
    check("一开始起点在 open 表里", (uint32_t)rts_path_last_open(sh.scx, sh.scy), 1);
    check("一开始起点不在 closed 表里", (uint32_t)rts_path_last_visited(sh.scx, sh.scy), 0);

    uint32_t mismatch = 0;
    for (uint32_t k = 0; k < 40; ++k) {
      rts_path_search_step(&nav, &sh);
      if (sh.steps != sh.expanded) ++mismatch;
    }
    check("40 步里 steps 始终 == expanded", mismatch, 0);
    check("确实推进了 40 步", sh.steps, 40);

    /* 中途两张表都得有东西，否则 demo 画不出"前沿" */
    uint32_t nopen = 0, nclosed = 0;
    for (int32_t cy = 0; cy < Rts_Nav_Cells_Y; ++cy) {
      for (int32_t cx = 0; cx < Rts_Nav_Cells_X; ++cx) {
        if (rts_path_last_open(cx, cy)) ++nopen;
        if (rts_path_last_visited(cx, cy)) ++nclosed;
      }
    }
    check("closed 的格子数 == expanded", nclosed, sh.expanded);
    check("closed 非空", (uint32_t)(nclosed > 0u), 1);
    check("open 非空", (uint32_t)(nopen > 0u), 1);

    uint32_t guard = 0;
    while (!sh.finished && guard < 100000u) { rts_path_search_step(&nav, &sh); ++guard; }
    check("已结束", (uint32_t)sh.finished, 1);
    check("找到路", (uint32_t)sh.found, 1);
    check("终点在 closed 表里", (uint32_t)rts_path_last_visited(sh.gcx, sh.gcy), 1);
    check("终点不在 open 表里", (uint32_t)rts_path_last_open(sh.gcx, sh.gcy), 0);
    /* 注意不断言"open 表已空"：弹出终点就提前收工，堆里可能还剩没处理的格子 —— 这正是提前终止的体现 */
  }

  printf("\n[41] g / h 代价读数（可视化取的就是这两个数）\n");
  {
    rts_Navgrid nav;
    rts_navgrid_init(&nav);                      /* 空地：代价应当正好是 octile */

    rts_Path_search sh;
    const lum_Vec2 from = rts_navgrid_cell_center(6, 6);
    const lum_Vec2 to   = rts_navgrid_cell_center(12, 12);
    check("begin 成功", (uint32_t)rts_path_search_begin(&nav, from, to, &sh), 1);

    float g = -1.0f, h = -1.0f;
    check("起点有代价读数", (uint32_t)rts_path_last_costs(6, 6, &g, &h), 1);
    checkf("起点的 g 是 0", g, 0.0f, 0.0f);
    checkf("起点的 h = 6√2", h, 6.0f * 1.41421356f, 1e-3f);

    /* begin 只把起点入队：起点有读数，其它格子还没被碰过 */
    check("起点在 open 表里", (uint32_t)rts_path_last_open(6, 6), 1);
    check("begin 后邻格还没被碰过（无读数）", (uint32_t)rts_path_last_costs(7, 6, &g, &h), 0);

    check("网格外的格子没有读数", (uint32_t)rts_path_last_costs(-1, 0, &g, &h), 0);
    check("网格外的格子没有读数（上界）", (uint32_t)rts_path_last_costs(0, Rts_Nav_Cells_Y, &g, &h), 0);

    rts_path_search_step(&nav, &sh);
    check("第一步定案的是起点", (uint32_t)(sh.current == sh.scy * Rts_Nav_Cells_X + sh.scx), 1);
    check("起点现在在 closed 表里", (uint32_t)rts_path_last_visited(6, 6), 1);
    check("起点仍有代价读数", (uint32_t)rts_path_last_costs(6, 6, &g, &h), 1);
    checkf("定案后 g 仍是 0", g, 0.0f, 0.0f);

    /* 展开之后邻格进了 open：既没定案、又有 g/h —— 这正是 demo 画点的那些格子 */
    check("展开后邻格在 open 表里", (uint32_t)rts_path_last_open(7, 6), 1);
    check("展开后邻格有 g/h 读数", (uint32_t)rts_path_last_costs(7, 6, &g, &h), 1);
    checkf("邻格的 g = 1", g, 1.0f, 1e-4f);
    checkf("邻格的 h = 1 + 5√2", h, 1.0f + 5.0f * 1.41421356f, 1e-3f);

    uint32_t guard = 0;
    while (!sh.finished && guard < 100000u) { rts_path_search_step(&nav, &sh); ++guard; }
    check("找到路", (uint32_t)sh.found, 1);

    /* ① g 是【真实累计代价】而不是步数：纯对角的一步必须是 √2，不是 1 */
    check("对角邻格已在 closed 表里", (uint32_t)rts_path_last_visited(7, 7), 1);
    check("(7,7) 有代价读数", (uint32_t)rts_path_last_costs(7, 7, &g, &h), 1);
    checkf("(7,7) 的 g = √2（不是步数 1）", g, 1.41421356f, 1e-4f);
    checkf("(7,7) 的 h = 5√2", h, 5.0f * 1.41421356f, 1e-3f);
    check("(7,7) 的 g < h（还在起点附近）", (uint32_t)(g < h), 1);

    /* ② 终点：g = 6√2（空地直线），h = 0 */
    check("终点有代价读数", (uint32_t)rts_path_last_costs(sh.gcx, sh.gcy, &g, &h), 1);
    checkf("终点的 g = 6√2", g, 6.0f * 1.41421356f, 1e-3f);
    checkf("终点的 h = 0", h, 0.0f, 1e-3f);

    /* ③ 不变量：所有被碰过的格子，h 都必须等于到终点的 octile 距离。
       这一条把"h 是启发式"钉死 —— 谁把 h 改成常数或算错，它立刻报。 */
    uint32_t stamped = 0, h_bad = 0, g_neg = 0;
    for (int32_t cy = 0; cy < Rts_Nav_Cells_Y; ++cy) {
      for (int32_t cx = 0; cx < Rts_Nav_Cells_X; ++cx) {
        if (!rts_path_last_open(cx, cy) && !rts_path_last_visited(cx, cy)) continue;
        ++stamped;
        if (!rts_path_last_costs(cx, cy, &g, &h)) { ++h_bad; continue; }
        if (fabsf(h - octile_cells(cx, cy, sh.gcx, sh.gcy)) > 1e-3f) ++h_bad;
        if (g < -1e-6f) ++g_neg;
      }
    }
    check("被碰过的格子数 > 0", (uint32_t)(stamped > 0u), 1);
    check("每一格的 h 都等于 octile 距离", h_bad, 0);
    check("没有负的 g", g_neg, 0);

    /* ④ 绕路时 g 必须大于直线 octile：证明它在数真实代价 */
    rts_Navgrid walled;
    nav_wall(&walled);
    rts_Path_search sw;
    check("有障碍时 begin 成功", (uint32_t)rts_path_search_begin(&walled, lum_vec2_create(-200.0f, 0.0f),
                                                                 lum_vec2_create(400.0f, 16.0f), &sw), 1);
    guard = 0;
    while (!sw.finished && guard < 100000u) { rts_path_search_step(&walled, &sw); ++guard; }
    check("有障碍时找到路", (uint32_t)sw.found, 1);
    check("终点有代价读数", (uint32_t)rts_path_last_costs(sw.gcx, sw.gcy, &g, &h), 1);
    const float straight_octile = octile_cells(sw.scx, sw.scy, sw.gcx, sw.gcy);
    check("绕路后 g 明显大于直线 octile", (uint32_t)(g > straight_octile + 0.5f), 1);
    checkf("绕路后终点的 h 仍为 0", h, 0.0f, 1e-3f);

    /* ⑤ g 是"真实累计代价"的又一证据：空地直连时 g 必须正好等于 octile，
       绕路时必须更大 —— 两件事同时成立，才说明它既不是步数也不是常数 */
    rts_Path_search so;
    check("空地 begin 成功", (uint32_t)rts_path_search_begin(&nav, rts_navgrid_cell_center(6, 6),
                                                             rts_navgrid_cell_center(18, 6), &so), 1);
    guard = 0;
    while (!so.finished && guard < 100000u) { rts_path_search_step(&nav, &so); ++guard; }
    check("空地找到路", (uint32_t)so.found, 1);
    check("空地终点有读数", (uint32_t)rts_path_last_costs(so.gcx, so.gcy, &g, &h), 1);
    checkf("空地横穿 12 格的 g = 12", g, 12.0f, 1e-4f);
  }

  lum_ecs_destroy(ecs);
  printf("\n==== %s（失败 %d 项）====\n", g_fail == 0 ? "全部通过" : "有失败", g_fail);
  return g_fail == 0 ? 0 : 1;
}

/* M1 + M2 无窗口验证：框选/单击命中的数学、阶段契约，以及单位移动指令。
   不需要 GL、不需要窗口 —— 直接建 ECS、跑一步步进（含 Transform）再结算选择。
   移动指令直接调 rts_orders_* 并把 dt 显式传进去，所以每一步的位移都是确定的。 */
#include "luminous.h"
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

  lum_ecs_destroy(ecs);
  printf("\n==== %s（失败 %d 项）====\n", g_fail == 0 ? "全部通过" : "有失败", g_fail);
  return g_fail == 0 ? 0 : 1;
}

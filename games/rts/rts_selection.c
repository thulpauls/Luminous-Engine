#include "rts_selection.h"
#include "rts_units.h"
#include "rts_layers.h"

#include <math.h>

void rts_selection_init(rts_Selection* s) {
  if (!s) return;
  s->dragging = false;
  s->start_px = lum_vec2_0();
  s->current_px = lum_vec2_0();
  s->pending = false;
  s->pending_is_click = false;
  s->pending_point_px = lum_vec2_0();
  s->selected_count = 0;
  s->total_count = 0;
}

static float rts__clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

void rts_selection_input(rts_Selection* s, rts_Mouse_state mouse,
                         lum_Vec2 window_size, bool ui_captured) {
  if (!s) return;
  if (ui_captured) { s->dragging = false; return; }   /* UI 占用输入时不下令 */

  /* 指针可能在窗口外：GLFW 的客户区坐标此时会越界成很大的值或负值 */
  const bool in_window = (mouse.position.x >= 0.0f && mouse.position.x <= window_size.x &&
                          mouse.position.y >= 0.0f && mouse.position.y <= window_size.y);

  if (!s->dragging) {
    /* 起手必须在窗口内，否则框会从画面外开始 */
    if (in_window && mouse.left_pressed) {
      s->dragging = true;
      s->start_px = mouse.position;
      s->current_px = mouse.position;
    }
    return;
  }

  /* 拖动中：把当前点夹进窗口 —— 指针拖出窗口时框贴边停住，而不是飞走 */
  s->current_px = lum_vec2_create(rts__clampf(mouse.position.x, 0.0f, window_size.x),
                                  rts__clampf(mouse.position.y, 0.0f, window_size.y));

  if (!mouse.left_down) {
    s->dragging = false;
    float dx = s->current_px.x - s->start_px.x;
    float dy = s->current_px.y - s->start_px.y;
    s->pending = true;
    s->pending_is_click = (dx * dx + dy * dy) < (Rts_Selection_Click_Slop * Rts_Selection_Click_Slop);
    s->pending_point_px = s->current_px;
  }
}

/* 与 lum_scene_pick 完全一致的 world → AABB：
   列向量长度 = 层级缩放（列主序：列0 = m[0..1]，列1 = m[4..5]），平移 = m[12..13]，
   旋转不进 AABB。保持一致才不会出现"点得到但框不到"。 */
static void rts__unit_aabb(const lum_Component_transform* t, const lum_Component_sprite* sp,
                           float* out_cx, float* out_cy, float* out_hw, float* out_hh) {
  float sx = sqrtf(t->world.m[0] * t->world.m[0] + t->world.m[1] * t->world.m[1]);
  float sy = sqrtf(t->world.m[4] * t->world.m[4] + t->world.m[5] * t->world.m[5]);
  *out_hw = sp->size.x * sx * 0.5f;
  *out_hh = sp->size.y * sy * 0.5f;
  *out_cx = t->world.m[12];
  *out_cy = t->world.m[13];
}

void rts_selection_resolve(rts_Selection* s, lum_Ecs* ecs,
                           const lum_Camera2d* camera, lum_Vec2 window_size) {
  if (!s || !ecs || !camera) return;
  lum_Component_type unit_t = rts_units_type();
  if (unit_t == Lum_Component_Invalid) return;

  /* 先全清 + 数总数：一格视图，代价 O(n) */
  uint32_t total = 0;
  {
    lum_Ecs_view v = lum_ecs_view_create(ecs, unit_t);
    rts_Unit* u;
    while (lum_ecs_view_each(&v, (void**)&u) != Lum_Entity_Null) {
      u->selected = 0;
      ++total;
    }
  }

  if (s->pending) {
    s->pending = false;

    if (s->pending_is_click) {
      /* 单击：取最上层（layer 最大、平局后建者胜）—— 交给引擎的 pick，
         Post 阶段 world 矩阵已是最新 */
      lum_Vec2 wp = lum_camera2d_screen_to_world(camera, s->pending_point_px, window_size);
      lum_Entity hit = lum_scene_pick(ecs, wp);
      if (hit != Lum_Entity_Null) {
        rts_Unit* u = (rts_Unit*)lum_ecs_get(ecs, hit, unit_t);
        if (u) u->selected = 1;
      }
    } else {
      /* 框选：屏幕矩形 → 世界 AABB（注意屏幕 Y 向下、世界 Y 向上，所以两个角
         都要换算后再取 min/max，不能直接对屏幕坐标取） */
      lum_Vec2 a = lum_camera2d_screen_to_world(camera, s->start_px, window_size);
      lum_Vec2 b = lum_camera2d_screen_to_world(camera, s->current_px, window_size);
      lum_Vec2 mn = lum_vec2_create(lum_minf(a.x, b.x), lum_minf(a.y, b.y));
      lum_Vec2 mx = lum_vec2_create(lum_maxf(a.x, b.x), lum_maxf(a.y, b.y));

      lum_Ecs_view v = lum_ecs_view_create3(ecs, lum_Component_transform_id,
                                            lum_Component_sprite_id, unit_t);
      lum_Component_transform* t;
      lum_Component_sprite* sp;
      rts_Unit* u;
      while (lum_ecs_view_each3(&v, (void**)&t, (void**)&sp, (void**)&u) != Lum_Entity_Null) {
        float cx, cy, hw, hh;
        rts__unit_aabb(t, sp, &cx, &cy, &hw, &hh);
        if (cx + hw < mn.x || cx - hw > mx.x) continue;   /* AABB 分离测试 */
        if (cy + hh < mn.y || cy - hh > mx.y) continue;
        u->selected = 1;
      }
    }
  }

  uint32_t sel = 0;
  {
    lum_Ecs_view v = lum_ecs_view_create(ecs, unit_t);
    rts_Unit* u;
    while (lum_ecs_view_each(&v, (void**)&u) != Lum_Entity_Null) {
      if (u->selected) ++sel;
    }
  }
  s->total_count = total;
  s->selected_count = sel;
}

void rts_selection_draw_marks(const rts_Selection* s, lum_Ecs* ecs) {
  if (!s || !ecs || s->selected_count == 0) return;
  lum_Component_type unit_t = rts_units_type();
  if (unit_t == Lum_Component_Invalid) return;

  const lum_Vec4 c = lum_vec4_create(0.45f, 1.00f, 0.55f, 0.95f);
  const float    th = 2.0f;

  lum_Ecs_view v = lum_ecs_view_create3(ecs, lum_Component_transform_id,
                                        lum_Component_sprite_id, unit_t);
  lum_Component_transform* t;
  lum_Component_sprite* sp;
  rts_Unit* u;
  while (lum_ecs_view_each3(&v, (void**)&t, (void**)&sp, (void**)&u) != Lum_Entity_Null) {
    if (!u->selected) continue;
    float cx, cy, hw, hh;
    rts__unit_aabb(t, sp, &cx, &cy, &hw, &hh);
    hw += 2.0f;
    hh += 2.0f;
    float bx = hw * 0.45f;    /* 角标单臂长度 */
    float by = hh * 0.45f;

    const float xs[4] = { cx - hw, cx + hw, cx - hw, cx + hw };
    const float ys[4] = { cy - hh, cy - hh, cy + hh, cy + hh };
    const float dxs[4] = { +1.0f, -1.0f, +1.0f, -1.0f };
    const float dys[4] = { +1.0f, +1.0f, -1.0f, -1.0f };
    for (int i = 0; i < 4; ++i) {
      lum_Vec2 corner = lum_vec2_create(xs[i], ys[i]);
      lum_renderer2d_draw_line(corner,
                               lum_vec2_create(xs[i] + dxs[i] * bx, ys[i]),
                               th, Rts_Layer_Selection, c);
      lum_renderer2d_draw_line(corner,
                               lum_vec2_create(xs[i], ys[i] + dys[i] * by),
                               th, Rts_Layer_Selection, c);
    }
  }
}

void rts_selection_draw_box(const rts_Selection* s) {
  if (!s || !s->dragging) return;

  float x0 = lum_minf(s->start_px.x, s->current_px.x);
  float y0 = lum_minf(s->start_px.y, s->current_px.y);
  float w  = fabsf(s->current_px.x - s->start_px.x);
  float h  = fabsf(s->current_px.y - s->start_px.y);
  if (w < 1.0f || h < 1.0f) return;

  lum_Vec4 fill = lum_vec4_create(0.35f, 0.85f, 0.45f, 0.14f);
  lum_Vec4 edge = lum_vec4_create(0.45f, 1.00f, 0.55f, 0.90f);

  /* Screen 阶段的 VP 已是像素、左上原点，这里直接用像素坐标 */
  lum_renderer2d_draw_rect(lum_vec2_create(x0, y0), lum_vec2_create(w, h),
                           0.0f, lum_vec2_0(), Rts_Layer_Box_Fill, fill);

  lum_Vec2 tl = lum_vec2_create(x0, y0);
  lum_Vec2 tr = lum_vec2_create(x0 + w, y0);
  lum_Vec2 br = lum_vec2_create(x0 + w, y0 + h);
  lum_Vec2 bl = lum_vec2_create(x0, y0 + h);
  lum_renderer2d_draw_line(tl, tr, 1.5f, Rts_Layer_Box_Edge, edge);
  lum_renderer2d_draw_line(tr, br, 1.5f, Rts_Layer_Box_Edge, edge);
  lum_renderer2d_draw_line(br, bl, 1.5f, Rts_Layer_Box_Edge, edge);
  lum_renderer2d_draw_line(bl, tl, 1.5f, Rts_Layer_Box_Edge, edge);
}

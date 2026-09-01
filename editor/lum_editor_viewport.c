/* 编辑器视口：把世界渲染进屏幕上的一块区域（路线 B：区域 VP，命令级裁剪零设施），
   相机控制、拾取、选中、高亮框全部区域感知。 */
#include "editor/lum_editor_internal.h"
#include "luminous.h"
#include <math.h>

/* 区域内相机：复制应用相机、视口换成区域尺寸——渲染 VP 与拾取换算共用同一变换 */
static lum_Camera2d region_camera(const lum_Rect* region) {
  lum_Camera2d cam = *lum_app_get_camera();
  lum_camera2d_set_viewport(&cam, region->size.x, region->size.y);
  return cam;
}

/* 区域 VP：区域局部像素 → NDC（区域即窗口），再重映射到全窗口裁剪空间里区域所在的矩形。
   世界命令在 VP 层面就落进区域，超出部分被硬件 NDC 裁剪——无需 scissor */
lum_Mat4 lum_editor_region_view_projection(const lum_Rect* region) {
  float window_w = (float)lum_window_get_width();
  float window_h = (float)lum_window_get_height();

  lum_Camera2d cam = region_camera(region);
  lum_Mat4 vp = lum_camera2d_get_view_projection_matrix(&cam);

  float sx = region->size.x / window_w;
  float sy = region->size.y / window_h;
  float tx = 2.0f * region->position.x / window_w + sx - 1.0f;
  float ty = 1.0f - 2.0f * region->position.y / window_h - sy;
  return lum_mat4_mul(lum_mat4_mul(lum_mat4_translate(tx, ty, 0.0f),
                                   lum_mat4_scale(sx, sy, 1.0f)), vp);
}

/* UI 像素点（区域所在的全窗口坐标）→ 世界坐标（与 region_view_projection 互逆） */
static lum_Vec2 region_world_of(const lum_Rect* region, lum_Vec2 ui_point) {
  lum_Vec2 local = lum_vec2_sub(ui_point, region->position);
  lum_Camera2d cam = region_camera(region);
  return lum_camera2d_screen_to_world(&cam, local, region->size);
}

static lum_Vec2 mouse_world(const lum_Rect* region) {
  return region_world_of(region, lum_input_get_mouse_position());
}

/* 世界矩阵变换本地偏移：父级的旋转/缩放会正确带动子实体光束端点 */
static lum_Vec2 world_offset(const lum_Component_transform* t, lum_Vec2 local_offset) {
  lum_Vec4 p = lum_mat4_mul_vec4(t->world, lum_vec4_create(local_offset.x, local_offset.y, 0.0f, 1.0f));
  return lum_vec2_create(p.x, p.y);
}

/* ---------- 选中与包围盒 ---------- */

/* 选中包围盒（实体本地空间）：glow 以原点为心半径 r；beam 取两端点偏移的 AABB；
   sprite 以原点为心尺寸×层级缩放。旋转实体的框不随之旋转（AABB 近似） */
static void editor_bounds(lum_Ecs* ecs, lum_Entity e, lum_Vec2* out_center, lum_Vec2* out_half) {
  *out_center = lum_vec2_0();
  *out_half = lum_vec2_create(1.0f, 1.0f);

  lum_Component_glow* glow = (lum_Component_glow*)lum_ecs_get(ecs, e, lum_component_glow_type());
  if (glow) {
    *out_half = lum_vec2_create(glow->radius, glow->radius);
    return;
  }

  lum_Component_beam* beam = (lum_Component_beam*)lum_ecs_get(ecs, e, lum_component_beam_type());
  if (beam) {
    float half_t = beam->thickness * 0.5f;
    float mn_x = fminf(beam->start_offset.x, beam->end_offset.x) - half_t;
    float mn_y = fminf(beam->start_offset.y, beam->end_offset.y) - half_t;
    float mx_x = fmaxf(beam->start_offset.x, beam->end_offset.x) + half_t;
    float mx_y = fmaxf(beam->start_offset.y, beam->end_offset.y) + half_t;
    *out_center = lum_vec2_create((mn_x + mx_x) * 0.5f, (mn_y + mx_y) * 0.5f);
    *out_half = lum_vec2_create((mx_x - mn_x) * 0.5f, (mx_y - mn_y) * 0.5f);
    return;
  }

  lum_Component_sprite* sp = (lum_Component_sprite*)lum_ecs_get(ecs, e, lum_Component_sprite_id);
  if (sp) {
    lum_Component_transform* t = (lum_Component_transform*)lum_ecs_get(ecs, e, lum_Component_transform_id);
    if (t) {
      float sx = sqrtf(t->world.m[0] * t->world.m[0] + t->world.m[1] * t->world.m[1]);
      float sy = sqrtf(t->world.m[4] * t->world.m[4] + t->world.m[5] * t->world.m[5]);
      *out_half = lum_vec2_create(sp->size.x * sx * 0.5f, sp->size.y * sy * 0.5f);
    }
  }
}

void lum_editor_select(lum_Editor* ed, lum_Ecs* ecs, lum_Entity e) {
  ed->selected = e;
  if (e == Lum_Entity_Null) {
    ed->selected_center = lum_vec2_0();
    ed->selected_half = lum_vec2_0();
    return;
  }
  editor_bounds(ecs, e, &ed->selected_center, &ed->selected_half);
}

/* ---------- 相机：中键平移 + 滚轮缩放（仅视口区域内生效） ---------- */

void lum_editor_camera_update(lum_Editor* ed) {
  /* 门控只看鼠标悬停：键盘焦点（如点过面板按钮后残留）不应冻结相机。
     F5/F9 等键盘热键仍用 input_captured 全量门控 */
  if (lum_ui_hovered() != NULL) return;

  lum_Rect region = ed->viewport_rect;
  lum_Vec2 mouse = lum_input_get_mouse_position();
  if (!lum_ui_point_in_rect(region, mouse)) return;

  lum_Camera2d* cam = lum_app_get_camera();

  /* F：视野居中——有选中实体对准它，否则回原点（画面被拖丢后的自救键） */
  if (lum_input_is_key_pressed(Lum_Key_F)) {
    lum_Ecs* ecs = lum_app_get_ecs();
    lum_Component_transform* t = (ed->selected != Lum_Entity_Null)
        ? (lum_Component_transform*)lum_ecs_get(ecs, ed->selected, lum_Component_transform_id) : NULL;
    cam->position = (t) ? lum_vec2_create(t->world.m[12], t->world.m[13]) : lum_vec2_0();
  }

  lum_Vec2 scroll = lum_input_get_scroll();
  if (scroll.y != 0.0f) {
    lum_Vec2 before = mouse_world(&region);
    float zoom = cam->zoom * powf(1.15f, scroll.y);
    if (zoom < 0.25f) zoom = 0.25f;
    if (zoom > 4.0f) zoom = 4.0f;
    cam->zoom = zoom;
    lum_Vec2 after = mouse_world(&region);
    cam->position = lum_vec2_add(cam->position, lum_vec2_sub(before, after));
  }

  if (lum_input_is_mouse_down(Lum_Mouse_Button_Middle)) {
    lum_Vec2 delta = lum_input_get_mouse_position_delta();
    if (delta.x != 0.0f || delta.y != 0.0f) {
      /* 前后两点必须走同一套区域变换——混用应用相机会把平移量放大数倍直接甩飞相机 */
      lum_Vec2 mouse_prev = lum_vec2_sub(mouse, delta);
      lum_Vec2 now_w  = region_world_of(&region, mouse);
      lum_Vec2 prev_w = region_world_of(&region, mouse_prev);
      cam->position = lum_vec2_add(cam->position, lum_vec2_sub(prev_w, now_w));
    }
  }
}

/* ---------- 拾取：点击选中 ---------- */

/* 点击拾取：引擎 sprite AABB（lum_scene_pick）∪ glow 圆 ∪ beam 端点框，层最大者胜。
   选中框几何由 lum_editor_select → editor_bounds 统一计算 */
static lum_Entity demo_pick(lum_Ecs* ecs, lum_Vec2 world) {
  lum_Entity best = lum_scene_pick(ecs, world);
  int32_t best_layer = 0;
  bool have_best = (best != Lum_Entity_Null);
  if (have_best) {
    lum_Component_sprite* sp = (lum_Component_sprite*)lum_ecs_get(ecs, best, lum_Component_sprite_id);
    best_layer = sp->layer;
  }

  lum_Ecs_view v;
  lum_Component_transform* t; lum_Entity e;

  /* glow：渲染中心 = world 平移，命中体积 = radius 圆 */
  v = lum_ecs_view_create2(ecs, lum_Component_transform_id, lum_component_glow_type());
  lum_Component_glow* glow;
  while ((e = lum_ecs_view_each2(&v, (void**)&t, (void**)&glow)) != Lum_Entity_Null) {
    float dx = world.x - t->world.m[12], dy = world.y - t->world.m[13];
    if (dx * dx + dy * dy > glow->radius * glow->radius) continue;
    if (!have_best || glow->layer > best_layer) {
      best = e; best_layer = glow->layer;
      have_best = true;
    }
  }

  /* beam：两端点世界坐标的 AABB ± 半厚度 */
  v = lum_ecs_view_create2(ecs, lum_Component_transform_id, lum_component_beam_type());
  lum_Component_beam* beam;
  while ((e = lum_ecs_view_each2(&v, (void**)&t, (void**)&beam)) != Lum_Entity_Null) {
    lum_Vec2 p1 = world_offset(t, beam->start_offset);
    lum_Vec2 p2 = world_offset(t, beam->end_offset);
    float half_t = beam->thickness * 0.5f;
    float mn_x = fminf(p1.x, p2.x) - half_t, mn_y = fminf(p1.y, p2.y) - half_t;
    float mx_x = fmaxf(p1.x, p2.x) + half_t, mx_y = fmaxf(p1.y, p2.y) + half_t;
    if (world.x < mn_x || world.x > mx_x || world.y < mn_y || world.y > mx_y) continue;
    if (!have_best || beam->layer > best_layer) {
      best = e; best_layer = beam->layer;
      have_best = true;
    }
  }

  return best;
}

void lum_editor_viewport_pick(lum_Editor* ed, lum_Ecs* ecs) {
  lum_Rect region = ed->viewport_rect;
  lum_Vec2 mouse = lum_input_get_mouse_position();
  if (!lum_ui_point_in_rect(region, mouse)) return;   /* 区域外点击：不选中也不取消 */

  if (lum_input_is_mouse_pressed(Lum_Mouse_Button_Left) && lum_ui_hovered() == NULL) {
    lum_editor_select(ed, ecs, demo_pick(ecs, mouse_world(&region)));
  }
}

/* ---------- 绘制：区域边框 + 选中高亮框 ---------- */

/* 区域边框：UI 像素空间 2px 描边。正交取 (0,W,H,0)——Y 向下与 UI 坐标同向，坐标原样可用 */
void lum_editor_render_region_border(const lum_Rect* region) {
  float window_w = (float)lum_window_get_width();
  float window_h = (float)lum_window_get_height();
  lum_Mat4 saved = lum_renderer2d_view_projection();
  lum_renderer2d_set_view_projection(lum_mat4_ortho(0.0f, window_w, window_h, 0.0f, -1.0f, 1.0f));

  lum_Vec4 border = lum_default_theme.panel_border;
  float t = 2.0f;
  float x = region->position.x, y = region->position.y;
  float w = region->size.x, h = region->size.y;
  lum_renderer2d_draw_rect(lum_vec2_create(x, y), lum_vec2_create(w, t), 0.0f, lum_vec2_0(), 1000, border);
  lum_renderer2d_draw_rect(lum_vec2_create(x, y + h - t), lum_vec2_create(w, t), 0.0f, lum_vec2_0(), 1000, border);
  lum_renderer2d_draw_rect(lum_vec2_create(x, y), lum_vec2_create(t, h), 0.0f, lum_vec2_0(), 1000, border);
  lum_renderer2d_draw_rect(lum_vec2_create(x + w - t, y), lum_vec2_create(t, h), 0.0f, lum_vec2_0(), 1000, border);

  lum_renderer2d_set_view_projection(saved);
}

/* 区域剪裁：UI 逻辑像素矩形 → 渲染目标像素（左下原点）。世界内容裁进视口区域的唯一手段——
   区域 VP 的 NDC 重映射会把视锥外坐标搬进窗口裁剪空间，GPU 视锥裁剪对它无效 */
static void push_region_scissor(const lum_Rect* region) {
  float fb_w = (float)lum_window_get_framebuffer_width();
  float fb_h = (float)lum_window_get_framebuffer_height();
  float win_w = (float)lum_window_get_width();
  float win_h = (float)lum_window_get_height();
  if (fb_w <= 0.0f || fb_h <= 0.0f || win_w <= 0.0f || win_h <= 0.0f) return;
  float sx = fb_w / win_w;
  float sy = fb_h / win_h;
  lum_renderer2d_set_scissor((int)(region->position.x * sx),
                             (int)(fb_h - (region->position.y + region->size.y) * sy),
                             (int)(region->size.x * sx),
                             (int)(region->size.y * sy));
}

void lum_editor_render_selection_box(lum_Editor* ed) {
  /* 选中高亮：青色描边框（世界空间 4 细边），层 900 = 世界之上、UI 之下。
     中心 = 本地中心偏移过 world 矩阵——与实体几何同一变换，移动/旋转/父级都跟随 */
  if (ed->selected == Lum_Entity_Null || !lum_ecs_alive(lum_app_get_ecs(), ed->selected)) return;

  lum_Component_transform* st =
      (lum_Component_transform*)lum_ecs_get(lum_app_get_ecs(), ed->selected, lum_Component_transform_id);
  if (!st) return;

  lum_Vec4 accent = lum_vec4_create(0.4f, 0.85f, 1.0f, 1.0f);
  lum_Vec2 center = world_offset(st, ed->selected_center);
  float mn_x = center.x - ed->selected_half.x, mn_y = center.y - ed->selected_half.y;
  float mx_x = center.x + ed->selected_half.x, mx_y = center.y + ed->selected_half.y;
  float w = 3.0f;
  lum_renderer2d_set_blend_mode(Lum_Blend_Alpha);
  lum_renderer2d_draw_rect(lum_vec2_create(mn_x, mn_y), lum_vec2_create(mx_x - mn_x, w), 0.0f, lum_vec2_0(), 900, accent);
  lum_renderer2d_draw_rect(lum_vec2_create(mn_x, mx_y - w), lum_vec2_create(mx_x - mn_x, w), 0.0f, lum_vec2_0(), 900, accent);
  lum_renderer2d_draw_rect(lum_vec2_create(mn_x, mn_y), lum_vec2_create(w, mx_y - mn_y), 0.0f, lum_vec2_0(), 900, accent);
  lum_renderer2d_draw_rect(lum_vec2_create(mx_x - w, mn_y), lum_vec2_create(w, mx_y - mn_y), 0.0f, lum_vec2_0(), 900, accent);
}

/* 场景 pass 绑定：shell 在 lum_ui_frame 之后、lum_scene_render 之前调用。
   绑定区域 VP + 区域剪裁——此后录制的场景命令只画进视口区域 */
void lum_editor_begin_scene_pass(lum_Editor* ed) {
  lum_renderer2d_set_view_projection(lum_editor_region_view_projection(&ed->viewport_rect));
  push_region_scissor(&ed->viewport_rect);
}

/* overlay：绑定区域 VP + 区域剪裁，画选中框与区域边框，最后解除剪裁（其后的 UI 面板不被裁）。
   shell 在 lum_ui_frame 之前调用；其后 shell 再调 lum_editor_begin_scene_pass 为场景命令重新绑定 */
void lum_editor_render_overlay(lum_Editor* ed) {
  lum_renderer2d_set_view_projection(lum_editor_region_view_projection(&ed->viewport_rect));
  push_region_scissor(&ed->viewport_rect);
  lum_editor_render_selection_box(ed);
  lum_editor_render_region_border(&ed->viewport_rect);
  lum_renderer2d_clear_scissor();
}

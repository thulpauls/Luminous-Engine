#include "rts_camera.h"

#include "lum_application.h"
#include "lum_input.h"
#include "lum_window.h"
#include <math.h>

static float rts__clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

void rts_camera_ctl_init(rts_Camera_ctl* c) {
  if (!c) return;
  c->pan_speed_px      = 620.0f;
  c->edge_speed_px     = 820.0f;
  c->edge_margin_px    = 18.0f;
  c->zoom_step         = 1.12f;
  c->zoom_min          = 0.25f;
  c->zoom_max          = 3.00f;
  c->edge_scroll_enabled = true;
  c->dragging          = false;
  c->drag_anchor_px    = lum_vec2_0();
  c->drag_cam_pos      = lum_vec2_0();
}

void rts_camera_ctl_update(rts_Camera_ctl* c, lum_Camera2d* camera, bool ui_captured) {
  if (!c || !camera) return;

  const float dt = lum_app_get_delta();
  const float zoom = camera->zoom > 0.0f ? camera->zoom : 1.0f;
  /* 屏幕像素 → 世界单位：1 像素 = 1/zoom 世界单位（投影把 viewport_w/zoom 铺满屏幕宽） */
  const float world_per_px = 1.0f / zoom;

  /* ---------- 键盘平移 ---------- */
  lum_Vec2 dir = lum_vec2_0();
  if (lum_input_is_key_down(Lum_Key_A) || lum_input_is_key_down(Lum_Key_Left))  dir.x -= 1.0f;
  if (lum_input_is_key_down(Lum_Key_D) || lum_input_is_key_down(Lum_Key_Right)) dir.x += 1.0f;
  if (lum_input_is_key_down(Lum_Key_S) || lum_input_is_key_down(Lum_Key_Down))  dir.y -= 1.0f;
  if (lum_input_is_key_down(Lum_Key_W) || lum_input_is_key_down(Lum_Key_Up))    dir.y += 1.0f;
  if (dir.x != 0.0f || dir.y != 0.0f) {
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
    dir = lum_vec2_scale(dir, 1.0f / len);            /* 斜向不加速 */
    float step = c->pan_speed_px * world_per_px * dt;
    camera->position.x += dir.x * step;
    camera->position.y += dir.y * step;
  }

  /* ---------- 鼠标贴边滚动 ---------- */
  if (c->edge_scroll_enabled && !ui_captured) {
    lum_Vec2 m = lum_input_get_mouse_position();
    float w = (float)lum_window_get_width();
    float h = (float)lum_window_get_height();
    lum_Vec2 e = lum_vec2_0();
    if (m.x <= c->edge_margin_px)     e.x -= 1.0f;
    if (m.x >= w - c->edge_margin_px) e.x += 1.0f;
    if (m.y <= c->edge_margin_px)     e.y += 1.0f;    /* 屏幕上方 = 世界 Y 增大 */
    if (m.y >= h - c->edge_margin_px) e.y -= 1.0f;
    if (e.x != 0.0f || e.y != 0.0f) {
      float step = c->edge_speed_px * world_per_px * dt;
      camera->position.x += e.x * step;
      camera->position.y += e.y * step;
    }
  }

  /* ---------- 中键拖拽：抓住世界拖 ---------- */
  if (ui_captured) {
    c->dragging = false;
  } else {
    if (lum_input_is_mouse_pressed(Lum_Mouse_Button_Middle)) {
      c->dragging = true;
      c->drag_anchor_px = lum_input_get_mouse_position();
      c->drag_cam_pos = camera->position;
    }
    if (c->dragging && !lum_input_is_mouse_down(Lum_Mouse_Button_Middle)) {
      c->dragging = false;
    }
    if (c->dragging) {
      lum_Vec2 m = lum_input_get_mouse_position();
      float dx = (m.x - c->drag_anchor_px.x) * world_per_px;
      float dy = (m.y - c->drag_anchor_px.y) * world_per_px;
      /* 鼠标右移 → 内容右移 → 相机左移；鼠标下移（屏幕 Y 向下）→ 相机在世界里上移 */
      camera->position.x = c->drag_cam_pos.x - dx;
      camera->position.y = c->drag_cam_pos.y + dy;
    }
  }

  /* ---------- 滚轮缩放：以光标下的世界点为锚 ---------- */
  if (!ui_captured) {
    float wheel = lum_input_get_scroll().y;
    if (wheel != 0.0f) {
      lum_Vec2 mouse = lum_input_get_mouse_position();
      /* lum_app_screen_to_world 读的就是我们正在改的这份相机，所以先取 before、
         改完 zoom 再取 after，差值补偿到 position 上 —— 光标下的点保持不动 */
      lum_Vec2 before = lum_app_screen_to_world(mouse);
      camera->zoom = rts__clampf(camera->zoom * powf(c->zoom_step, wheel),
                                 c->zoom_min, c->zoom_max);
      lum_Vec2 after = lum_app_screen_to_world(mouse);
      camera->position.x += before.x - after.x;
      camera->position.y += before.y - after.y;
    }
  }
}

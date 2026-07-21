#include "lum_renderer2d.h"
#include "lum_quad.h"
#include "lum_renderer.h"
#include "lum_shader.h"
#include <string.h>
#include <assert.h>

typedef struct lum_Renderer2d {
  bool initialized;
  lum_Quad default_quad;
  lum_Shader sprite_shader;
  lum_Texture2d white_texture;
} lum_Renderer2d;

static lum_Renderer2d g_renderer2d;

static void lum_renderer2d_reset_state(void) {
  memset(&g_renderer2d, 0, sizeof(g_renderer2d));
}

int lum_renderer2d_init(uint32_t viewport_w, uint32_t viewport_h, const char* vertex_shader_source, const char* fragment_shader_source) {
  assert(vertex_shader_source && fragment_shader_source);
  lum_renderer2d_reset_state();

  if (!lum_renderer_init(viewport_w, viewport_h)) return 0;
  if (!lum_quad_init(&g_renderer2d.default_quad)) {
    lum_renderer_shutdown();
    return 0;
  }
  if (!lum_shader_create_from_source(&g_renderer2d.sprite_shader, vertex_shader_source, fragment_shader_source)) {
    lum_renderer_shutdown();
    lum_quad_destroy(&g_renderer2d.default_quad);
    return 0;
  }
  if (!lum_texture2d_create_white(&g_renderer2d.white_texture)) {
    lum_renderer_shutdown();
    lum_shader_destroy(&g_renderer2d.sprite_shader);
    lum_quad_destroy(&g_renderer2d.default_quad);
    return 0;
  }

  g_renderer2d.initialized = true;
  return 1;
}

void lum_renderer2d_shutdown(void) {
  if (!g_renderer2d.initialized) return;

  lum_texture2d_destroy(&g_renderer2d.white_texture);
  lum_shader_destroy(&g_renderer2d.sprite_shader);
  lum_quad_destroy(&g_renderer2d.default_quad);

  lum_renderer_shutdown();
  lum_renderer2d_reset_state();
}

bool lum_renderer2d_is_initialized(void) {
  return g_renderer2d.initialized;
}

void lum_renderer2d_set_viewport(int x, int y, int w, int h) {
  if (!g_renderer2d.initialized) return;
  lum_renderer_set_viewport(x, y, w, h);
}

void lum_renderer2d_set_clear_color(lum_Vec4 color) {
  if (!g_renderer2d.initialized) return;
  lum_renderer_set_clear_color(color);
}

void lum_renderer2d_begin_frame(void) {
  if (!g_renderer2d.initialized) return;
  lum_renderer_begin_frame();
}

void lum_renderer2d_end_frame(void) {
  if (!g_renderer2d.initialized) return;
  lum_renderer_end_frame();
}

void lum_renderer2d_clear(void) {
  if (!g_renderer2d.initialized) return;
  lum_renderer_clear();
}

void lum_renderer2d_set_camera(const lum_Camera2d* camera) {
  assert(camera);
  if (!g_renderer2d.initialized) return;
  lum_renderer_set_view_projection(lum_camera2d_get_view_projection_matrix(camera));
}

void lum_renderer2d_set_view_projection(lum_Mat4 view_proj) {
  if (!g_renderer2d.initialized) return;
  lum_renderer_set_view_projection(view_proj);
}

void lum_renderer2d_enable_blend(void) {
  if (!g_renderer2d.initialized) return;
  lum_renderer_enable_blend();
}

void lum_renderer2d_disable_blend(void) {
  if (!g_renderer2d.initialized) return;
  lum_renderer_disable_blend();
}

void lum_renderer2d_draw_sprite(const lum_Texture2d* texture, const lum_Transform2d* transform, lum_Vec4 color) {
  assert(texture && transform);
  if (!g_renderer2d.initialized) return;
  lum_Mat4 model = lum_transform2d_get_matrix(transform);
  lum_renderer_draw_textured_quad(&g_renderer2d.default_quad, &g_renderer2d.sprite_shader, texture, model, color);
}

void lum_renderer2d_draw_rect(const lum_Transform2d* transform, lum_Vec4 color) {
  lum_renderer2d_draw_sprite(&g_renderer2d.white_texture, transform, color);
}

void lum_renderer2d_draw_sprite_ex(const lum_Texture2d* texture, lum_Vec2 position, lum_Vec2 size, float rotation_rad, lum_Vec2 origin, lum_Vec4 color) {
  if (!g_renderer2d.initialized) return;
  lum_Transform2d t = lum_transform2d_create(position, size, lum_vec2_1(), rotation_rad, origin);
  lum_renderer2d_draw_sprite(texture, &t, color);
}

void lum_renderer2d_draw_rect_ex(lum_Vec2 position, lum_Vec2 size, float rotation_rad, lum_Vec2 origin, lum_Vec4 color) {
  lum_renderer2d_draw_sprite_ex(&g_renderer2d.white_texture, position, size, rotation_rad, origin, color);
}

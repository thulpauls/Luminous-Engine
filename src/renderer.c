#include "renderer.h"
#include <string.h>
#include <glad/glad.h>
#include <assert.h>

typedef struct lum_Renderer {
  uint32_t viewport_x, viewport_y, viewport_w, viewport_h;

  lum_Vec4 clear_color;
  lum_Mat4 view_projection;
  bool has_view_projection;

  bool initialized;
} lum_Renderer;

static lum_Renderer g_renderer;

static void lum_renderer_reset(void) {
  memset(&g_renderer, 0, sizeof(g_renderer));
  g_renderer.clear_color = lum_vec4_create(0.0f, 0.0f, 0.0f, 1.0f);
  g_renderer.view_projection = lum_mat4_identity();
  g_renderer.has_view_projection = false;
}

int lum_renderer_init(uint32_t viewport_w, uint32_t viewport_h) {
  lum_renderer_reset();

  g_renderer.viewport_x = 0;
  g_renderer.viewport_y = 0;
  g_renderer.viewport_w = viewport_w;
  g_renderer.viewport_h = viewport_h;

  glViewport(0, 0, viewport_w, viewport_h);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glDisable(GL_DEPTH_TEST);

  g_renderer.initialized = true;
  
  return 1;
}

void lum_renderer_shutdown(void) {
  lum_renderer_reset();
}

bool lum_renderer_is_initialized(void) {
  return g_renderer.initialized;
}

void lum_renderer_set_viewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
  if (!lum_renderer_is_initialized()) return;
  
  g_renderer.viewport_x = x;
  g_renderer.viewport_y = y;
  g_renderer.viewport_w = w;
  g_renderer.viewport_h = h;

  glViewport(x, y, w, h);
}

void lum_renderer_set_clear_color(lum_Vec4 color) {
  if (!lum_renderer_is_initialized()) return;
  g_renderer.clear_color = color;
}

void lum_renderer_begin_frame(void) {}
void lum_renderer_end_frame(void) {}

void lum_renderer_clear(void) {
  if (!lum_renderer_is_initialized()) return;

  glClearColor(g_renderer.clear_color.x, g_renderer.clear_color.y, g_renderer.clear_color.z, g_renderer.clear_color.w);
  glClear(GL_COLOR_BUFFER_BIT);
}

void lum_renderer_set_view_projection(lum_Mat4 view_proj) {
  if (!lum_renderer_is_initialized()) return;

  g_renderer.view_projection = view_proj;
  g_renderer.has_view_projection = true;
}

lum_Mat4 lum_renderer_get_view_projection(void) {
  return g_renderer.view_projection;
}

void lum_renderer_enable_blend(void) {
  if (!lum_renderer_is_initialized()) return;

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void lum_renderer_disable_blend(void) {
  if (!lum_renderer_is_initialized()) return;

  glDisable(GL_BLEND);
}

void lum_renderer_draw_quad(const lum_Quad* quad, const lum_Shader* shader, lum_Mat4 model, lum_Vec4 color) {
  assert(quad && shader);
  if (!lum_renderer_is_initialized() || !g_renderer.has_view_projection) return;
  
  lum_shader_use(shader);
  lum_shader_uniform_set4m(shader, "u_model", model);
  lum_shader_uniform_set4m(shader, "u_view_projection", g_renderer.view_projection);
  lum_shader_uniform_set4v(shader, "u_color", lum_vec4_create(color.x, color.y, color.z, color.w));
  
  lum_quad_draw(quad);
}

void lum_renderer_draw_textured_quad(const lum_Quad* quad, const lum_Shader* shader, const lum_Texture2d* texture, lum_Mat4 model, lum_Vec4 color) {
  assert(quad && shader && texture);
  if (!lum_renderer_is_initialized() || !g_renderer.has_view_projection) return;

  lum_shader_use(shader);
  lum_shader_uniform_set4m(shader, "u_model", model);
  lum_shader_uniform_set4m(shader, "u_view_projection", g_renderer.view_projection);
  lum_shader_uniform_set4v(shader, "u_color", lum_vec4_create(color.x, color.y, color.z, color.w));
  lum_shader_uniform_set1i(shader, "u_texture", 0);

  lum_texture2d_bind(texture, 0);
  lum_quad_draw(quad);
}

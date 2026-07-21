#ifndef Lum_Renderer_H
#define Lum_Renderer_H

#include <stdbool.h>
#include <stdint.h>

#include "lum_math2d.h"
#include "lum_quad.h"
#include "lum_shader.h"
#include "lum_texture.h"

typedef struct lum_Renderer {
  uint32_t viewport_x, viewport_y, viewport_w, viewport_h;

  lum_Vec4 clear_color;
  lum_Mat4 view_projection;
  bool has_view_projection;

  bool initialized;
} lum_Renderer;

int lum_renderer_init(uint32_t viewport_w, uint32_t viewport_h);
void lum_renderer_shutdown(void);
bool lum_renderer_is_initialized(void);

void lum_renderer_set_viewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void lum_renderer_set_clear_color(lum_Vec4 color);

void lum_renderer_begin_frame(void);
void lum_renderer_clear(void);
void lum_renderer_end_frame(void);

void lum_renderer_set_view_projection(lum_Mat4 view_proj);
lum_Mat4 lum_renderer_get_view_projection(void);

void lum_renderer_enable_blend(void);
void lum_renderer_disable_blend(void);

void lum_renderer_draw_quad(const lum_Quad* quad, const lum_Shader* shader, lum_Mat4 model, lum_Vec4 color);
void lum_renderer_draw_textured_quad(const lum_Quad* quad, const lum_Shader* shader, const lum_Texture2d* texture, lum_Mat4 model, lum_Vec4 color);


#endif //Lum_Renderer_H

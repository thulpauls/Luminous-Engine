#ifndef Lum_Renderer2d_H
#define Lum_Renderer2d_H

#include <stdbool.h>
#include <stdint.h>
#include "lum_math2d.h"
#include "lum_transform.h"
#include "lum_texture.h"
#include "lum_camera.h"

int lum_renderer2d_init(uint32_t viewport_w, uint32_t viewport_h, const char* vertex_shader_source, const char* fragment_shader_source);
void lum_renderer2d_shutdown(void);

bool lum_renderer2d_is_initialized(void);

void lum_renderer2d_set_viewport(int x, int y, int w, int h);
void lum_renderer2d_set_clear_color(lum_Vec4 color);

void lum_renderer2d_begin_frame(void);
void lum_renderer2d_end_frame(void);
void lum_renderer2d_clear(void);

void lum_renderer2d_set_camera(const lum_Camera2d* camera);
void lum_renderer2d_set_view_projection(lum_Mat4 view_proj);

void lum_renderer2d_enable_blend(void);
void lum_renderer2d_disable_blend(void);

void lum_renderer2d_draw_sprite(const lum_Texture2d* texture, const lum_Transform2d* transform, lum_Vec4 color);
void lum_renderer2d_draw_rect(const lum_Transform2d* transform, lum_Vec4 color);
void lum_renderer2d_draw_sprite_layer(const lum_Texture2d* texture, const lum_Transform2d* transform, int32_t layer, lum_Vec4 color);
void lum_renderer2d_draw_rect_layer(const lum_Transform2d* transform, int32_t layer, lum_Vec4 color);

void lum_renderer2d_draw_sprite_ex(const lum_Texture2d* texture, lum_Vec2 position, lum_Vec2 size, float rotation_rad, lum_Vec2 origin, lum_Vec4 color);
void lum_renderer2d_draw_rect_ex(lum_Vec2 position, lum_Vec2 size, float rotation_rad, lum_Vec2 origin, lum_Vec4 color);
void lum_renderer2d_draw_sprite_ex_layer(const lum_Texture2d* texture, lum_Vec2 position, lum_Vec2 size, float rotation_rad, lum_Vec2 origin, int32_t layer, lum_Vec4 color);
void lum_renderer2d_draw_rect_ex_layer(lum_Vec2 position, lum_Vec2 size, float rotation_rad, lum_Vec2 origin, int32_t layer, lum_Vec4 color);

uint32_t lum_renderer2d_get_last_frame_command_count(void);
uint32_t lum_renderer2d_get_last_frame_quad_count(void);
uint32_t lum_renderer2d_get_last_frame_batch_count(void);
uint32_t lum_renderer2d_get_last_frame_draw_call_count(void);

#endif //Lum_Renderer2d_H

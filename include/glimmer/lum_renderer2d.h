#ifndef Lum_Renderer2d_H
#define Lum_Renderer2d_H

#include <stdint.h>
#include <stdbool.h>
#include "glimmer/lum_math2d.h"
#include "glimmer/lum_transform.h"
#include "glimmer/lum_texture.h"
#include "glimmer/lum_camera.h"
#include "glimmer/lum_material.h"

typedef struct lum_Vertex {
    float position[3];
    float uv[2];
    float color[4];
} lum_Vertex;

typedef enum lum_Primitive {
    Lum_Primitive_Triangles = 0,
    Lum_Primitive_Triangle_Strip,
    Lum_Primitive_Triangle_Fan,
    Lum_Primitive_Lines,
    Lum_Primitive_Line_Strip,
    Lum_Primitive_Line_Loop,
    Lum_Primitive_Points
} lum_Primitive;

int lum_renderer2d_init(uint32_t viewport_w, uint32_t viewport_h);
void lum_renderer2d_shutdown(void);

void lum_renderer2d_set_viewport(int x, int y, int w, int h);
void lum_renderer2d_get_viewport(int* x, int* y, int* w, int* h);   /* 读当前视口（视图"全屏"解析用） */
void lum_renderer2d_set_clear_color(lum_Vec4 color);

void lum_renderer2d_begin_frame(void);
void lum_renderer2d_end_frame(void);
void lum_renderer2d_clear(void);

void lum_renderer2d_set_camera(const lum_Camera2d* camera);
void lum_renderer2d_set_view_projection(lum_Mat4 view_proj);
lum_Mat4 lum_renderer2d_view_projection(void);   /* 当前帧投影读取（UI/子系统暂存还原用） */

typedef enum lum_Blend_mode {
  Lum_Blend_None     = 0,
  Lum_Blend_Alpha    = 1,   /* SRC_ALPHA, ONE_MINUS_SRC_ALPHA */
  Lum_Blend_Additive = 2,   /* ONE, ONE */
  Lum_Blend_Premul   = 3,   /* ONE, ONE_MINUS_SRC_ALPHA */
} lum_Blend_mode;

void lum_renderer2d_set_blend_mode(lum_Blend_mode mode);

/* 剪裁：后续 draw 命令按渲染目标像素矩形（左下原点）丢弃片元，clear 成对退出。
   命令级快照（同 view_projection/blend）：不同剪裁的命令不入同批，flush 时切换 GL 状态 */
void lum_renderer2d_set_scissor(int x, int y, int w, int h);
void lum_renderer2d_clear_scissor(void);

/* 区域清屏：glClear 只受 scissor 限制、不受 viewport 限制，所以要把多个视图清进各自矩形
   必须走这条。内部临时开关 GL scissor，不影响命令级剪裁状态（set_scissor 的记录值）。 */
void lum_renderer2d_clear_region(int x, int y, int w, int h, lum_Vec4 color);

void lum_renderer2d_build_quad(lum_Mat4 model, lum_Vec4 color, lum_Vertex out_verts[4]);

void lum_renderer2d_draw_mesh(const lum_Vertex* vertices, uint32_t vertex_count, const uint32_t* indices, uint32_t index_count, lum_Primitive primitive, const lum_Material* material, const lum_Texture2d* texture, int32_t layer);
void lum_renderer2d_draw_fullscreen(const lum_Material* material, const lum_Texture2d* src, int32_t layer);

void lum_renderer2d_draw_sprite(const lum_Texture2d* texture, const lum_Transform2d* transform, int32_t layer, lum_Vec4 color);
void lum_renderer2d_draw_sprite_uv(const lum_Texture2d* texture, lum_Vec2 position, lum_Vec2 size, lum_Vec4 uv_rect, int32_t layer, lum_Vec4 color);

/* 直接用世界矩阵绘制 sprite 子矩形（场景层级渲染用），model 语义同 lum_renderer2d_build_quad */
void lum_renderer2d_draw_sprite_matrix_uv(const lum_Texture2d* texture, lum_Mat4 model,
                                                 lum_Vec4 uv_rect, int32_t layer, lum_Vec4 color);

void lum_renderer2d_draw_rect(lum_Vec2 position, lum_Vec2 size, float rotation_rad, lum_Vec2 origin, int32_t layer, lum_Vec4 color);

void lum_renderer2d_draw_line(lum_Vec2 start, lum_Vec2 end, float thickness, int32_t layer, lum_Vec4 color);

void lum_renderer2d_draw_circle(lum_Vec2 position, float radius, int32_t layer, lum_Vec4 color);

void lum_renderer2d_draw_triangle(lum_Vec2 p0, lum_Vec2 p1, lum_Vec2 p2, int32_t layer, lum_Vec4 color);

void lum_renderer2d_draw_polygon_convex(const lum_Vec2* points, uint32_t count, int32_t layer, lum_Vec4 color);

void lum_renderer2d_draw_polyline(const lum_Vec2* points, uint32_t count, float thickness, int32_t layer, lum_Vec4 color);

void lum_renderer2d_draw_points(const lum_Vec2* points, uint32_t count, float size, int32_t layer, lum_Vec4 color);

uint32_t lum_renderer2d_get_draw_call_count(void);

#endif //Lum_Renderer2d_H

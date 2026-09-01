#ifndef Lum_Text_Renderer_H
#define Lum_Text_Renderer_H

#include "lum_math2d.h"
#include <stdbool.h>

typedef struct lum_Font {
  bool initialized;
  void* renderer;
} lum_Font;

int lum_tr_load_font(lum_Font* font, const char* text, float height_pixel);
void lum_tr_destroy_font(lum_Font* font);

/* 预光栅化全部 ASCII 字形进 atlas（一次性，消除渲染首帧的批量纹理上传） */
void lum_tr_preload_ascii(lum_Font* font);

/*
* @brief 在世界空间绘制文字，使用 draw_sprite 渲染管线
* @param position 世界坐标，文本以该坐标作为左下角顶点
*/
void lum_tr_draw_layer(lum_Font* font, const char* text, lum_Vec2 position, int32_t layer, lum_Vec4 color);
void lum_tr_draw(lum_Font* font, const char* text, lum_Vec2 position, lum_Vec4 color);

/*
* @brief 测量文本包围尺寸，返回 (宽, 高)；多行取最大宽 × 行数。
*        会触发未加载字形的懒加载
*/
lum_Vec2 lum_tr_measure_text(lum_Font* font, const char* text);

/* 基线到行顶的上升高度（像素）＝ ascent × scale；UI 顶边对齐换算用 */
float lum_tr_ascent(lum_Font* font);

#endif //Lum_Text_Renderer_H

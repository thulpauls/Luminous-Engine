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

/*
* @brief 在世界空间绘制文字，使用 draw_sprite 渲染管线
* @param position 世界坐标，文本以该坐标作为左下角顶点
*/
void lum_tr_draw_layer(lum_Font* font, const char* text, lum_Vec2 position, int32_t layer, lum_Vec4 color);
void lum_tr_draw(lum_Font* font, const char* text, lum_Vec2 position, lum_Vec4 color);

#endif //Lum_Text_Renderer_H
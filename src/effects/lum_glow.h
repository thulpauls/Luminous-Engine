#ifndef Lum_Glow_H
#define Lum_Glow_H

#include "lum_math2d.h"
#include <stdint.h>
#include <stdbool.h>

int  lum_glow_init(void);
void lum_glow_shutdown(void);
bool lum_glow_is_initialized(void);

#define Lum_Glow_Default_Core_Size  0.18f
#define Lum_Glow_Default_Core_Decay 1.0f
#define Lum_Glow_Default_Edge_Decay 4.0f

/*
* @brief 绘制高斯衰减光晕
* @param center 光晕中心点坐标
* @param radius 可见半径
* @param color color.rgb 为光色，color.a 在 additive blend 时不参与混合
*/
void lum_glow_draw(lum_Vec2 center, float radius, lum_Vec4 color);
void lum_glow_draw_layer(lum_Vec2 center, float radius, int32_t layer, lum_Vec4 color);

/*
* @brief 绘制光晕，可独立调节核心大小 / 核心衰减 / 边缘衰减
* @param center     光晕中心点坐标
* @param radius     可见半径
* @param color      color.rgb 为光色，color.a 在 additive blend 时不参与混合
* @param core_size  亮核半径比例 (0, 1]，越大核心越宽；默认 Lum_Glow_Default_Core_Size
* @param core_decay 核心强度乘数，越小核心越暗；默认 Lum_Glow_Default_Core_Decay
* @param edge_decay 外晕衰减系数，越大边缘收得越快（光晕越紧）；默认 Lum_Glow_Default_Edge_Decay
* @note 相同参数的光晕共享同一材质实例（可合批），不同参数自动拆批，互不影响。
*/
void lum_glow_draw_advanced(lum_Vec2 center, float radius, lum_Vec4 color, float core_size, float core_decay, float edge_decay);
void lum_glow_draw_layer_advanced(lum_Vec2 center, float radius, int32_t layer, lum_Vec4 color, float core_size, float core_decay, float edge_decay);

#endif //Lum_Glow_H

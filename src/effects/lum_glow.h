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
#define Lum_Glow_Default_Halo_Intensity 0.35f

#define Lum_Glow_Default_Ring_Radius    0.6f
#define Lum_Glow_Default_Ring_Width     0.12f
#define Lum_Glow_Default_Ring_Intensity 1.0f
#define Lum_Glow_Default_Ring_Inner_Decay 1.0f
#define Lum_Glow_Default_Ring_Outer_Decay 1.0f

/*
* @brief 绘制高斯衰减光晕
* @param center 光晕中心点坐标
* @param radius 可见半径
* @param color color.rgb 为光色，color.a 在 additive blend 时不参与混合
*/
void lum_glow_draw(lum_Vec2 center, float radius, lum_Vec4 color);
void lum_glow_draw_layer(lum_Vec2 center, float radius, int32_t layer, lum_Vec4 color);

/*
* @brief 绘制高斯衰减光晕，可独立调节核心大小 / 核心衰减 / 边缘衰减 / 外围亮度
* @param center         光晕中心点坐标
* @param radius         可见半径
* @param color          color.rgb 为光色，color.a 在 additive blend 时不参与混合
* @param core_size      亮核半径比例 (0, 1]
* @param core_decay     核心强度乘数
* @param edge_decay     外晕衰减系数
* @param halo_intensity 外围亮度乘数，控制外晕整体明暗
*/
void lum_glow_draw_advanced(lum_Vec2 center, float radius, lum_Vec4 color, float core_size, float core_decay, float edge_decay, float halo_intensity);
void lum_glow_draw_layer_advanced(lum_Vec2 center, float radius, int32_t layer, lum_Vec4 color, float core_size, float core_decay, float edge_decay, float halo_intensity);

/*
* @brief 绘制环形光环
* @param center 光环中心点坐标
* @param radius 可见半径
* @param color color.rgb 为光色，color.a 在 additive blend 时不参与混合
*/
void lum_glow_draw_ring(lum_Vec2 center, float radius, lum_Vec4 color);
void lum_glow_draw_ring_layer(lum_Vec2 center, float radius, int32_t layer, lum_Vec4 color);

/*
* @brief 绘制环形光环，可独立调节环半径 / 环厚度 / 亮度 / 内外侧衰减速率
* @param center           光圈中心点坐标
* @param radius           可见半径
* @param color            color.rgb 为光色，color.a 在 additive blend 时不参与混合
* @param ring_radius      环峰值半径比例 (0, 1]
* @param ring_width       环厚度
* @param ring_intensity   环亮度乘数
* @param ring_inner_decay 内侧（朝中心方向）衰减速率
* @param ring_outer_decay 外侧（朝边缘方向）衰减速率
*/
void lum_glow_draw_ring_advanced(lum_Vec2 center, float radius, lum_Vec4 color, float ring_radius, float ring_width, float ring_intensity, float ring_inner_decay, float ring_outer_decay);
void lum_glow_draw_ring_layer_advanced(lum_Vec2 center, float radius, int32_t layer, lum_Vec4 color, float ring_radius, float ring_width, float ring_intensity, float ring_inner_decay, float ring_outer_decay);

#endif //Lum_Glow_H

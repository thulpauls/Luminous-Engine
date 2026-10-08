#ifndef Lum_Beam_H
#define Lum_Beam_H

#include "glimmer/lum_math2d.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum lum_Beam_mode {
    Lum_Beam_Mode_Lateral = 0,     /* 横向：亮度从中心线向两侧（宽度方向）衰减 */
    Lum_Beam_Mode_Longitudinal = 1, /* 纵向：亮度从中心点向两端（长度方向）衰减 */
    Lum_Beam_Mode_Center = 2       /* 中心：亮度从中心点同时向宽度和长度方向衰减 */
} lum_Beam_mode;

int  lum_beam_init(void);
void lum_beam_shutdown(void);
bool lum_beam_is_initialized(void);

#define Lum_Beam_Default_Core_Size  0.25f
#define Lum_Beam_Default_Core_Decay 1.0f
#define Lum_Beam_Default_Edge_Decay 4.0f

/*
* @brief 绘制线状光效
* @param start     线段起点
* @param end       线段终点
* @param thickness 线宽
* @param mode      衰减模式
* @param color     color.rgb 为光色，color.a 在 additive blend 时不参与混合
*/
void lum_beam_draw(lum_Vec2 start, lum_Vec2 end, float thickness, lum_Beam_mode mode, int32_t layer, lum_Vec4 color);

/*
* @brief 绘制线状光效，可独立调节核心宽度 / 核心强度 / 边缘衰减
* @param start       线段起点
* @param end         线段终点
* @param thickness   线宽
* @param mode        衰减模式
* @param color       color.rgb 为光色，color.a 在 additive blend 时不参与混合
* @param core_size   亮核宽度比例 (0, 1]
* @param core_decay  核心强度乘数
* @param edge_decay  边缘衰减系数
*/
void lum_beam_draw_advanced(lum_Vec2 start, lum_Vec2 end, float thickness, lum_Beam_mode mode, int32_t layer, lum_Vec4 color, float core_size, float core_decay, float edge_decay);

#endif //Lum_Beam_H

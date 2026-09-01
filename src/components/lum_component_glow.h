#ifndef Lum_Component_Glow_H
#define Lum_Component_Glow_H

#include "lum_scene.h"

/*
* 内置扩展组件：光晕/光环实体。ECS 数据 + 配套渲染系统 + 字段表序列化三件套。
* 用法：lum_components_register(ecs) 一次注册后，
*   lum_component_glow_add 创建（默认值）→ 挂 transform、改字段 →
*   lum_scene_add_render_system(ecs, lum_component_glow_render_system, NULL) 参与绘制。
*/

typedef struct lum_Component_glow {
  bool     ring;              /* false = 高斯光晕，true = 中空光环 */
  float    radius;
  int32_t  layer;
  lum_Vec4 color;
  float    core_size, core_decay, edge_decay, halo_intensity;             /* 光晕参数 */
  float    ring_radius, ring_width, ring_intensity;                       /* 光环参数 */
  float    ring_inner_decay, ring_outer_decay;
} lum_Component_glow;

/* 注册组件并绑定序列化字段表（组件名 "glow"）。对同一 world 幂等，返回组件 id */
lum_Component_type lum_component_glow_register(lum_Ecs* ecs);

/* 当前 world 的组件 id（本进程内未注册过返回 Lum_Component_Invalid） */
lum_Component_type lum_component_glow_type(void);

/* 便捷添加：填入与字段表一致的默认值 */
lum_Component_glow* lum_component_glow_add(lum_Ecs* ecs, lum_Entity e);

/* 渲染系统（lum_scene_add_render_system 用）：additive 混合，渲染中心 = world 平移 */
void lum_component_glow_render_system(lum_Ecs* ecs, void* user_data);

#endif //Lum_Component_Glow_H

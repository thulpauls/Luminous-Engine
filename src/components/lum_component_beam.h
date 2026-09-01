#ifndef Lum_Component_Beam_H
#define Lum_Component_Beam_H

#include "lum_scene.h"

/*
* 内置扩展组件：线状光束实体。ECS 数据 + 配套渲染系统 + 字段表序列化三件套。
* 用法同 lum_component_glow.h。
*/

typedef struct lum_Component_beam {
  lum_Vec2 start_offset, end_offset;   /* 相对实体 transform.position 的本地偏移 */
  float    thickness;
  int32_t  mode;                        /* lum_Beam_mode */
  int32_t  layer;
  lum_Vec4 color;
  float    core_size, core_decay, edge_decay;
} lum_Component_beam;

/* 注册组件并绑定序列化字段表（组件名 "beam"）。对同一 world 幂等，返回组件 id */
lum_Component_type lum_component_beam_register(lum_Ecs* ecs);

/* 当前 world 的组件 id（本进程内未注册过返回 Lum_Component_Invalid） */
lum_Component_type lum_component_beam_type(void);

/* 便捷添加：填入与字段表一致的默认值 */
lum_Component_beam* lum_component_beam_add(lum_Ecs* ecs, lum_Entity e);

/* 渲染系统（lum_scene_add_render_system 用）：additive 混合，端点偏移经 world 矩阵变换 */
void lum_component_beam_render_system(lum_Ecs* ecs, void* user_data);

#endif //Lum_Component_Beam_H

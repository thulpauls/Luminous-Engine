#ifndef Lum_Components_H
#define Lum_Components_H

#include "lum_component_glow.h"
#include "lum_component_beam.h"

/*
* 内置扩展组件聚合入口：一次调用注册全部扩展组件（注册 + 字段表序列化绑定）。
* 渲染系统按需挂接：lum_app_add_system(Lum_Stage_Scene, 0, lum_component_glow_render_system, NULL) 等。
*/

int lum_components_register(lum_Ecs* ecs);

#endif //Lum_Components_H

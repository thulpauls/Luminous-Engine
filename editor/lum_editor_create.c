/* 编辑器实体创建：+ glow / + beam 按钮（视野中心，自动命名并选中）。 */
#include "editor/lum_editor_internal.h"
#include "luminous.h"
#include <stdio.h>

void lum_editor_on_create_glow(lum_Widget* w, void* user) {
  (void)w; (void)user;
  lum_Editor* ed = lum_editor_instance();
  lum_Ecs* ecs = lum_app_get_ecs();
  lum_Entity e = lum_ecs_create_entity(ecs);
  char name[32];
  snprintf(name, sizeof(name), "glow %u", ++ed->create_sequence);
  lum_scene_add_name(ecs, e, name);
  lum_scene_add_transform(ecs, e)->position = lum_app_get_camera()->position;   /* 视野中心 */
  lum_component_glow_add(ecs, e);
  lum_editor_select(ed, ecs, e);
}

void lum_editor_on_create_beam(lum_Widget* w, void* user) {
  (void)w; (void)user;
  lum_Editor* ed = lum_editor_instance();
  lum_Ecs* ecs = lum_app_get_ecs();
  lum_Entity e = lum_ecs_create_entity(ecs);
  char name[32];
  snprintf(name, sizeof(name), "beam %u", ++ed->create_sequence);
  lum_scene_add_name(ecs, e, name);
  lum_scene_add_transform(ecs, e)->position = lum_app_get_camera()->position;
  lum_Component_beam* beam = lum_component_beam_add(ecs, e);
  beam->end_offset = lum_vec2_create(80.0f, 0.0f);   /* 默认可见：水平光束 */
  lum_editor_select(ed, ecs, e);
}

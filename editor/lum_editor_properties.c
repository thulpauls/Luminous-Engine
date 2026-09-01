/* 编辑器属性面板：字段表 → 控件桥、编辑绑定、面板重建。 */
#include "editor/lum_editor_internal.h"
#include "luminous.h"
#include <stdio.h>
#include <stdlib.h>

/* 编辑控件 → 组件内存的绑定。回调里按实体重新解析组件指针（池扩容会搬移组件内存，
   widget 里缓存组件指针会悬空）；field 指向静态字段表条目，进程期稳定 */
typedef struct Property_binding {
  lum_Ecs* ecs;
  lum_Entity entity;
  lum_Component_type component_type;
  const lum_Component_field_desc* field;   /* NULL = 实体名行（走 lum_scene_set_name） */
  uint32_t sub_index;                      /* Vec2/Vec4 的分量下标 */
} Property_binding;

static Property_binding* property_binding_new(lum_Ecs* ecs, lum_Entity e, lum_Component_type type,
                                              const lum_Component_field_desc* field, uint32_t sub_index) {
  Property_binding* b = (Property_binding*)malloc(sizeof(*b));
  if (!b) return NULL;
  b->ecs = ecs;
  b->entity = e;
  b->component_type = type;
  b->field = field;
  b->sub_index = sub_index;
  return b;   /* 重建属性面板时遗弃（量级：每次重建几十字节，测试床可接受） */
}

static void on_property_number(lum_Widget* w, void* user) {
  Property_binding* b = (Property_binding*)user;
  char* component = (char*)lum_ecs_get(b->ecs, b->entity, b->component_type);
  if (!component || !b->field) return;
  char* member = component + b->field->offset;
  const char* text = lum_ui_get_text(w);
  switch (b->field->kind) {
    case Lum_Component_field_F32: *(float*)member = strtof(text, NULL); break;
    case Lum_Component_field_I32: *(int32_t*)member = (int32_t)strtol(text, NULL, 10); break;
    case Lum_Component_field_U32: *(uint32_t*)member = (uint32_t)strtoul(text, NULL, 10); break;
    case Lum_Component_field_Vec2:
    case Lum_Component_field_Vec4: ((float*)member)[b->sub_index] = strtof(text, NULL); break;
    default: break;
  }
}

static void on_property_bool(lum_Widget* w, void* user) {
  Property_binding* b = (Property_binding*)user;
  char* component = (char*)lum_ecs_get(b->ecs, b->entity, b->component_type);
  if (!component || !b->field) return;
  *(bool*)(component + b->field->offset) = lum_ui_get_checked(w);
}

static void on_property_text(lum_Widget* w, void* user) {
  Property_binding* b = (Property_binding*)user;
  char* component = (char*)lum_ecs_get(b->ecs, b->entity, b->component_type);
  if (!component || !b->field) return;
  /* 截断拷贝：控件文本任意长，字段缓冲定长（size 即容量） */
  snprintf((char*)(component + b->field->offset), b->field->size, "%s", lum_ui_get_text(w));
}

static void on_property_name(lum_Widget* w, void* user) {
  Property_binding* b = (Property_binding*)user;
  lum_scene_set_name(b->ecs, b->entity, lum_ui_get_text(w));
  lum_editor_instance()->outliner_dirty = true;   /* 大纲行文字随名更新 */
}

static lum_Widget* property_textbox(lum_Widget* row, const char* initial, float width) {
  lum_Widget* box = lum_ui_textbox(row, initial);
  lum_ui_set_fixed_size(box, lum_vec2_create(width, 0.0f));
  return box;
}

/* 一个数值字段的编辑行：label + textbox；Vec2/Vec4 展开为分量 textbox。
   type 必须原样进绑定：回调靠它重新解析组件指针 */
static void property_build_field(lum_Widget* parent, lum_Ecs* ecs, lum_Entity e,
                                 lum_Component_type type, void* component,
                                 const lum_Component_field_desc* f) {
  char* member = (char*)component + f->offset;
  char text[64];

  switch (f->kind) {
    case Lum_Component_field_Bool: {
      lum_Widget* check = lum_ui_checkbox(parent, f->key, *(const bool*)member);
      lum_ui_on_value_changed(check, on_property_bool, property_binding_new(ecs, e, type, f, 0));
      break;
    }
    case Lum_Component_field_Str: {
      lum_Widget* row = lum_ui_box(parent, true);
      lum_ui_label(row, f->key);
      lum_Widget* box = property_textbox(row, (const char*)member, 160.0f);
      lum_ui_on_value_changed(box, on_property_text, property_binding_new(ecs, e, type, f, 0));
      break;
    }
    case Lum_Component_field_Vec2:
    case Lum_Component_field_Vec4: {
      uint32_t n = (f->kind == Lum_Component_field_Vec2) ? 2u : 4u;
      lum_Widget* row = lum_ui_box(parent, true);
      lum_ui_label(row, f->key);
      for (uint32_t i = 0; i < n; ++i) {
        snprintf(text, sizeof(text), "%.6g", ((const float*)member)[i]);
        lum_Widget* box = property_textbox(row, text, 64.0f);
        lum_ui_on_value_changed(box, on_property_number, property_binding_new(ecs, e, type, f, i));
      }
      break;
    }
    default: {   /* F32 / I32 / U32：单数值 textbox */
      lum_Widget* row = lum_ui_box(parent, true);
      lum_ui_label(row, f->key);
      if (f->kind == Lum_Component_field_F32) snprintf(text, sizeof(text), "%.6g", *(const float*)member);
      else if (f->kind == Lum_Component_field_I32) snprintf(text, sizeof(text), "%d", *(const int32_t*)member);
      else snprintf(text, sizeof(text), "%u", *(const uint32_t*)member);
      lum_Widget* box = property_textbox(row, text, 90.0f);
      lum_ui_on_value_changed(box, on_property_number, property_binding_new(ecs, e, type, f, 0));
      break;
    }
  }
}

void lum_editor_properties_rebuild(lum_Editor* ed, lum_Ecs* ecs, lum_Entity target) {
  /* 保留模式无销毁：重建 = 隐藏旧行 + 新建（重建仅在换选中/读档/改名时发生） */
  for (uint32_t i = 0; i < ed->prop_rows->child_count; ++i) {
    lum_ui_set_visible(ed->prop_rows->children[i], false);
  }

  if (target == Lum_Entity_Null) {
    lum_ui_label(ed->prop_rows, "no selection");
    return;
  }

  const char* name = lum_scene_name(ecs, target);
  char header[Lum_Scene_Name_Max + 32];
  snprintf(header, sizeof(header), "entity: %s", name ? name : "(unnamed)");
  lum_ui_label(ed->prop_rows, header);

  /* name 编辑行（实体名不走字段表：set_name 截断 + 大纲联动） */
  {
    lum_Widget* row = lum_ui_box(ed->prop_rows, true);
    lum_ui_label(row, "name");
    lum_Widget* box = property_textbox(row, name ? name : "", 160.0f);
    lum_ui_on_value_changed(box, on_property_name, property_binding_new(ecs, target, 0, NULL, 0));
  }

  /* 组件段：跳过 hierarchy（内部链表指针不可编辑）与 name（上方已有专门编辑行）；
     有字段表的组件整表生成 */
  lum_Component_type types[16];
  uint32_t type_count = lum_ecs_entity_component_types(ecs, target, types, 16);
  for (uint32_t i = 0; i < type_count; ++i) {
    if (types[i] == lum_Component_hierarchy_id || types[i] == lum_Component_name_id) continue;
    const char* cname = lum_ecs_component_name(ecs, types[i]);
    char section[64];
    snprintf(section, sizeof(section), "-- %s --", cname ? cname : "?");
    lum_ui_label(ed->prop_rows, section);

    const void* fields = NULL;
    uint32_t field_count = 0;
    if (!lum_ecs_component_field_table(ecs, types[i], &fields, &field_count)) {
      lum_ui_label(ed->prop_rows, "(callback-serialized, not editable)");
      continue;
    }
    void* component = lum_ecs_get(ecs, target, types[i]);
    if (!component) continue;
    const lum_Component_field_desc* desc = (const lum_Component_field_desc*)fields;
    for (uint32_t j = 0; j < field_count; ++j) {
      property_build_field(ed->prop_rows, ecs, target, types[i], component, &desc[j]);
    }
  }
}

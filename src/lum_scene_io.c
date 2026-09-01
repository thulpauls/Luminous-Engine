#include "lum_scene_io.h"
#include "lum_serial.h"
#include "lum_scene.h"
#include "lum_assets.h"
#include "lum_filesystem.h"
#include "containers/lum_vector.h"
#include "lum_assert.h"
#include "lum_log.h"
#include <stdlib.h>
#include <string.h>

#define Lum_Scene_Io_Version 1u
#define Lum_Scene_Io_No_Ordinal 0xFFFFFFFFu

/* ---------- 内置组件的序列化途径 ---------- */

/* transform：字段表。world 矩阵不在表内 —— 派生数据每帧重算，天然不落盘 */
static const lum_Component_field_desc g_transform_fields[] = {
  Lum_Component_Make_Field_Vec2(lum_Component_transform, position, "position", 0, 0),
  Lum_Component_Make_Field_F32 (lum_Component_transform, rotation, "rotation", 0.0f),
  Lum_Component_Make_Field_Vec2(lum_Component_transform, scale,    "scale",    1, 1),
  Lum_Component_Make_Field_Vec2(lum_Component_transform, origin,   "origin",   0, 0),
};

/* name：纯值组件，字段表一条即完事（Str 读方向以缓冲容量截断） */
static const lum_Component_field_desc g_name_fields[] = {
  Lum_Component_Make_Field_Str(lum_Component_name, text, "text", ""),
};

/* sprite：texture 指针 ⇄ 路径超出字段表表达力，走回调；其余字段为值 */
static int sprite_write(const void* component, lum_Archive* archive) {
  const lum_Component_sprite* sprite = (const lum_Component_sprite*)component;

  const char* path = lum_assets_texture_path(sprite->texture);
  if (!path && sprite->texture) {
    Lum_Log_Warn("scene_io: sprite texture is not from assets, saved as empty path.");
  }
  lum_serial_w_str(archive, "texture", path ? path : "");
  lum_serial_w_vec2(archive, "size", sprite->size);
  lum_serial_w_vec4(archive, "uv", sprite->uv);
  lum_serial_w_vec4(archive, "color", sprite->color);
  lum_serial_w_i32(archive, "layer", sprite->layer);
  lum_serial_w_bool(archive, "flip_x", sprite->flip_x);
  lum_serial_w_bool(archive, "flip_y", sprite->flip_y);
  return 1;
}

static int sprite_read(void* component, lum_Archive* archive) {
  lum_Component_sprite* sprite = (lum_Component_sprite*)component;

  char path[Lum_Serial_Max_Line];
  lum_serial_r_str(archive, "texture", "", path, sizeof(path));
  sprite->texture = path[0] ? lum_assets_texture(path, Lum_Texture_Flag_None) : NULL;
  if (path[0] && !sprite->texture) {
    Lum_Log_Warn("scene_io: sprite texture load failed: %s", path);
  }

  lum_serial_r_vec2(archive, "size",   lum_vec2_create(1.0f, 1.0f), &sprite->size);
  lum_serial_r_vec4(archive, "uv",     lum_vec4_create(0.0f, 0.0f, 1.0f, 1.0f), &sprite->uv);
  lum_serial_r_vec4(archive, "color",  lum_vec4_create(1.0f, 1.0f, 1.0f, 1.0f), &sprite->color);
  lum_serial_r_i32 (archive, "layer",  0, &sprite->layer);
  lum_serial_r_bool(archive, "flip_x", false, &sprite->flip_x);
  lum_serial_r_bool(archive, "flip_y", false, &sprite->flip_y);
  return 1;
}

/* script：函数指针不可落盘，按「脚本名注册表」序列化名字；user_data 不序列化 */
static int script_write(const void* component, lum_Archive* archive) {
  const lum_Component_script* script = (const lum_Component_script*)component;
  const char* name = lum_scene_script_name(script->start, script->update);
  if (!name) {
    Lum_Log_Warn("scene_io: unregistered script (fn pair) skipped in save.");
    return 1;
  }
  if (script->user_data) {
    Lum_Log_Warn("scene_io: script '%s' has user_data, it will be NULL after load.", name);
  }
  lum_serial_w_str(archive, "name", name);
  return 1;
}

static int script_read(void* component, lum_Archive* archive) {
  lum_Component_script* script = (lum_Component_script*)component;
  char name[Lum_Serial_Max_Line];
  lum_serial_r_str(archive, "name", "", name, sizeof(name));
  if (!name[0]) return 1;
  if (!lum_scene_script_lookup(name, &script->start, &script->update)) {
    Lum_Log_Warn("scene_io: unknown script '%s', callbacks left NULL.", name);
  }
  return 1;
}

int lum_scene_io_init(lum_Ecs* ecs) {
  if (!ecs) return 0;
  /* 组件本体已由 lum_scene_init 注册；这里只附加序列化途径（不得重复注册） */
  lum_ecs_set_component_field_table(ecs, lum_Component_transform_id, g_transform_fields,
                                    sizeof(g_transform_fields) / sizeof(g_transform_fields[0]));
  lum_ecs_set_component_field_table(ecs, lum_Component_name_id, g_name_fields,
                                    sizeof(g_name_fields) / sizeof(g_name_fields[0]));
  lum_ecs_set_component_serializer(ecs, lum_Component_sprite_id, sprite_write, sprite_read);
  lum_ecs_set_component_serializer(ecs, lum_Component_script_id, script_write, script_read);
  return 1;
}

/* ---------- 保存 ---------- */

int lum_scene_save(const lum_Ecs* ecs, const char* path) {
  if (!ecs || !path) return 0;

  /* 槽位序 → 文件序号映射：alive 实体按槽位序依次编 0..N-1 */
  lum_Vector ordinals;
  if (!lum_vector_init(&ordinals, sizeof(uint32_t))) return 0;

  uint32_t slot_count = lum_ecs_entity_slot_count(ecs);
  uint32_t next_ordinal = 0;
  for (uint32_t i = 0; i < slot_count; ++i) {
    uint32_t ordinal = lum_ecs_alive(ecs, lum_ecs_entity_at_slot(ecs, i)) ? next_ordinal++ : Lum_Scene_Io_No_Ordinal;
    if (!lum_vector_push(&ordinals, &ordinal)) {
      lum_vector_destroy(&ordinals);
      return 0;
    }
  }

  lum_Archive writer;
  if (!lum_serial_init_writer(&writer)) {
    lum_vector_destroy(&ordinals);
    return 0;
  }
  lum_serial_w_linef(&writer, "version %u", Lum_Scene_Io_Version);

  for (uint32_t i = 0; i < slot_count; ++i) {
    uint32_t ordinal = *(const uint32_t*)lum_vector_get_const(&ordinals, i);
    if (ordinal == Lum_Scene_Io_No_Ordinal) continue;

    lum_Entity entity = lum_ecs_entity_at_slot(ecs, i);
    lum_serial_w_linef(&writer, "entity %u", ordinal);

    /* 组件按类型 id 序枚举 → 同一世界两次保存逐字节相同 */
    lum_Component_type component_types[Lum_Serial_Max_Segment_Fields];
    uint32_t type_count = lum_ecs_entity_component_types(ecs, entity, component_types, Lum_Serial_Max_Segment_Fields);
    for (uint32_t t = 0; t < type_count; ++t) {
      const void* component = lum_ecs_get(ecs, entity, component_types[t]);
      if (component) {
        lum_serial_component_write(ecs, component_types[t], component, &writer);   /* 未注册静默跳过 */
      }
    }

    const lum_Component_hierarchy* hierarchy =
        (const lum_Component_hierarchy*)lum_ecs_get(ecs, entity, lum_Component_hierarchy_id);
    if (hierarchy && hierarchy->parent != Lum_Entity_Null) {
      uint32_t parent_slot = lum_ecs_entity_slot_index(hierarchy->parent);
      uint32_t parent_ordinal = (parent_slot < slot_count)
          ? *(const uint32_t*)lum_vector_get_const(&ordinals, parent_slot)
          : Lum_Scene_Io_No_Ordinal;
      if (parent_ordinal != Lum_Scene_Io_No_Ordinal) {
        lum_serial_w_linef(&writer, "parent %u", parent_ordinal);
      } else {
        Lum_Log_Warn("scene_io: entity %u has unserializable parent, link dropped.", ordinal);
      }
    }
  }

  size_t text_size = 0;
  const char* text = lum_serial_writer_finish(&writer, &text_size);
  int ok = text && lum_fs_write(path, text) > 0;
  if (!ok) Lum_Log_Error("scene_io: save failed: %s", path);

  lum_serial_destroy(&writer);
  lum_vector_destroy(&ordinals);
  return ok;
}

/* ---------- 加载 ---------- */

typedef struct lum_Link_request {
  uint32_t child_ordinal;
  uint32_t parent_ordinal;
} lum_Link_request;

static void lum__scene_io_clear_world(lum_Ecs* ecs) {
  uint32_t slot_count = lum_ecs_entity_slot_count(ecs);
  for (uint32_t i = 0; i < slot_count; ++i) {
    lum_Entity entity = lum_ecs_entity_at_slot(ecs, i);
    if (entity != Lum_Entity_Null) lum_ecs_destroy_entity(ecs, entity);
  }
  lum_ecs_flush(ecs);
}

int lum_scene_load(lum_Ecs* ecs, const char* path) {
  if (!ecs || !path) return 0;

  lum_Binary_file file = lum_fs_read_binary(path);
  if (!file.data) {
    Lum_Log_Error("scene_io: load failed, cannot read: %s", path);
    return 0;
  }
  /* lum_fs_read_binary 产出的缓冲不保证 NUL 结尾，补一个再交给文本读取器 */
  char* text = (char*)malloc(file.size + 1);
  if (!text) {
    free(file.data);
    return 0;
  }
  memcpy(text, file.data, file.size);
  text[file.size] = '\0';
  free(file.data);

  lum_Archive reader;
  if (!lum_serial_init_reader(&reader, text)) {
    free(text);
    return 0;
  }

  /* 版本检查 */
  if (!lum_serial_peek(&reader) || strcmp(lum_serial_peek_key(&reader), "version") != 0) {
    Lum_Log_Error("scene_io: %s lacks version header.", path);
    lum_serial_destroy(&reader);
    free(text);
    return 0;
  }
  uint32_t version = (uint32_t)strtoul(lum_serial_peek_argv(&reader, 1), NULL, 10);
  if (version / 100u != Lum_Scene_Io_Version / 100u) {
    Lum_Log_Error("scene_io: %s has version %u, expected %u.", path, version, Lum_Scene_Io_Version);
    lum_serial_destroy(&reader);
    free(text);
    return 0;
  }
  lum_serial_consume(&reader);

  lum__scene_io_clear_world(ecs);

  /* 单遍流式：实体创建 + 组件填充；parent 请求延迟到 EOF 后统一接链 */
  lum_Vector handle_by_ordinal;
  lum_Vector link_requests;
  if (!lum_vector_init(&handle_by_ordinal, sizeof(lum_Entity)) ||
      !lum_vector_init(&link_requests, sizeof(lum_Link_request))) {
    lum_vector_destroy(&handle_by_ordinal);
    free(text);
    return 0;
  }

  uint32_t current_ordinal = Lum_Scene_Io_No_Ordinal;
  lum_Entity current_entity = Lum_Entity_Null;
  int ok = 1;
  uint32_t loadstat_entities = 0, loadstat_components = 0, loadstat_links = 0;

  while (lum_serial_peek(&reader)) {
    const char* key = lum_serial_peek_key(&reader);

    if (strcmp(key, "entity") == 0) {
      current_ordinal = (uint32_t)strtoul(lum_serial_peek_argv(&reader, 1), NULL, 10);
      current_entity = lum_ecs_create_entity(ecs);
      if (current_entity == Lum_Entity_Null) {
        Lum_Log_Error("scene_io: entity creation failed at line %u.", lum_serial_peek_line(&reader));
        ok = 0;
        break;
      }
      /* 序号可能稀疏/乱序：数组按需增长对齐 */
      while (lum_vector_size(&handle_by_ordinal) <= current_ordinal) {
        lum_Entity null_entity = Lum_Entity_Null;
        if (!lum_vector_push(&handle_by_ordinal, &null_entity)) { ok = 0; break; }
      }
      if (ok) {
        *(lum_Entity*)lum_vector_get(&handle_by_ordinal, current_ordinal) = current_entity;
      }
      loadstat_entities++;
      lum_serial_consume(&reader);
    }
    else if (strcmp(key, "component") == 0) {
      const char* component_name = lum_serial_peek_argv(&reader, 1);
      lum_Component_type type = component_name
          ? lum_ecs_component_type_from_name(ecs, component_name)
          : Lum_Component_Invalid;
      lum_serial_consume(&reader);

      if (type == Lum_Component_Invalid) {
        Lum_Log_Warn("scene_io: unknown component '%s' at line %u, skipped.",
                     component_name ? component_name : "?", lum_serial_peek_line(&reader));
        lum_serial_begin_component(&reader);   /* 吞掉字段段 */
        continue;
      }
      if (current_entity == Lum_Entity_Null) {
        Lum_Log_Warn("scene_io: component before entity at line %u, skipped.",
                     lum_serial_peek_line(&reader));
        lum_serial_begin_component(&reader);
        continue;
      }
      void* component = lum_ecs_add(ecs, current_entity, type);
      if (component) {
        lum_serial_component_read(ecs, type, component, &reader);
        loadstat_components++;
      } else {
        lum_serial_begin_component(&reader);
        Lum_Log_Warn("scene_io: component add failed ('%s'), fields skipped.", component_name);
      }
    }
    else if (strcmp(key, "parent") == 0) {
      lum_Link_request request;
      request.child_ordinal = current_ordinal;
      request.parent_ordinal = (uint32_t)strtoul(lum_serial_peek_argv(&reader, 1), NULL, 10);
      if (current_ordinal != Lum_Scene_Io_No_Ordinal) {
        lum_vector_push(&link_requests, &request);
      }
      lum_serial_consume(&reader);
    }
    else {
      lum_serial_consume(&reader);   /* 未知结构行：跳过（前向兼容） */
    }
  }

  /* 接链：全部实体就位后统一执行 */
  for (uint32_t i = 0; i < lum_vector_size(&link_requests); ++i) {
    const lum_Link_request* request = (const lum_Link_request*)lum_vector_get_const(&link_requests, i);
    uint32_t child_count = lum_vector_size(&handle_by_ordinal);
    if (request->child_ordinal >= child_count || request->parent_ordinal >= child_count) {
      Lum_Log_Warn("scene_io: parent link %u->%u out of range, skipped.",
                   request->child_ordinal, request->parent_ordinal);
      continue;
    }
    lum_Entity child = *(const lum_Entity*)lum_vector_get_const(&handle_by_ordinal, request->child_ordinal);
    lum_Entity parent = *(const lum_Entity*)lum_vector_get_const(&handle_by_ordinal, request->parent_ordinal);
    if (child != Lum_Entity_Null && parent != Lum_Entity_Null) {
      lum_scene_set_parent(ecs, child, parent);
      loadstat_links++;
    }
  }

  fprintf(stderr, "scene_io: loaded %u entities, %u components, %u links from %s\n",
          loadstat_entities, loadstat_components, loadstat_links, "scene file");
  fflush(stderr);

  lum_vector_destroy(&handle_by_ordinal);
  lum_vector_destroy(&link_requests);
  lum_serial_destroy(&reader);
  free(text);
  return ok;
}

#include "lum_serial.h"
#include "lum_assert.h"
#include "lum_log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* 字段读取永不消费的 结构行关键字 */
static const char* lum__serial_structural_keys[] = { "version", "entity", "parent", "component" };

static bool lum__serial_is_structural(const char* key) {
  for (size_t i = 0; i < sizeof(lum__serial_structural_keys) / sizeof(lum__serial_structural_keys[0]); ++i) {
    if (strcmp(lum__serial_structural_keys[i], key) == 0) return true;
  }
  return false;
}

static int lum__serial_append(lum_Archive* archive, const char* data, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    if (!lum_vector_push(&archive->buffer, &data[i])) return 0;
  }
  return 1;
}

int lum_serial_init_writer(lum_Archive* archive) {
  Lum_Assert(archive);
  memset(archive, 0, sizeof(*archive));
  archive->writing = true;
  if (!lum_vector_init(&archive->buffer, sizeof(char))) return 0;
  return 1;
}

int lum_serial_init_reader(lum_Archive* archive, const char* text) {
  Lum_Assert(archive && text);
  memset(archive, 0, sizeof(*archive));
  archive->writing = false;
  archive->text = text;
  archive->size = strlen(text);
  if (!lum_vector_init(&archive->segment, sizeof(lum_Archive_line))) return 0;
  return 1;
}

void lum_serial_destroy(lum_Archive* archive) {
  Lum_Assert(archive);
  if (archive->writing) {
    lum_vector_destroy(&archive->buffer);
  } else {
    lum_vector_destroy(&archive->segment);
  }
  memset(archive, 0, sizeof(*archive));
}

/* ---------- 写方向 ---------- */

static int lum__serial_w_line_raw(lum_Archive* archive, const char* line) {
  if (!lum__serial_append(archive, line, strlen(line))) return 0;
  if (!lum__serial_append(archive, "\n", 1)) return 0;
  return 1;
}

int lum_serial_w_linef(lum_Archive* archive, const char* fmt, ...) {
  char buf[Lum_Serial_Max_Line];
  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  if (n < 0) return 0;
  return lum__serial_w_line_raw(archive, buf);
}

int lum_serial_w_f32(lum_Archive* archive, const char* key, float value) {
  return lum_serial_w_linef(archive, "%s %.9g", key, value);
}

int lum_serial_w_i32(lum_Archive* archive, const char* key, int32_t value) {
  return lum_serial_w_linef(archive, "%s %d", key, value);
}

int lum_serial_w_u32(lum_Archive* archive, const char* key, uint32_t value) {
  return lum_serial_w_linef(archive, "%s %u", key, value);
}

int lum_serial_w_bool(lum_Archive* archive, const char* key, bool value) {
  return lum_serial_w_linef(archive, "%s %s", key, value ? "true" : "false");
}

int lum_serial_w_str(lum_Archive* archive, const char* key, const char* value) {
  bool needs_quote = !value || !*value || strpbrk(value, " \t");
  if (needs_quote) {
    return lum_serial_w_linef(archive, "%s \"%s\"", key, value ? value : "");
  }
  return lum_serial_w_linef(archive, "%s %s", key, value);
}

int lum_serial_w_vec2(lum_Archive* archive, const char* key, lum_Vec2 value) {
  return lum_serial_w_linef(archive, "%s %.9g %.9g", key, value.x, value.y);
}

int lum_serial_w_vec4(lum_Archive* archive, const char* key, lum_Vec4 value) {
  return lum_serial_w_linef(archive, "%s %.9g %.9g %.9g %.9g", key, value.x, value.y, value.z, value.w);
}

const char* lum_serial_writer_finish(lum_Archive* archive, size_t* out_size) {
  Lum_Assert(archive && archive->writing);
  char terminator = '\0';
  lum_vector_push(&archive->buffer, &terminator);   /* strlen 安全的终止符，不计入 size */
  if (out_size) *out_size = lum_vector_size(&archive->buffer) - 1u;
  return (const char*)lum_vector_data(&archive->buffer);
}

/* ---------- 读方向：行填充与切词 ---------- */

/* 从文本读下一条有效行（跳过空行/注释）切词进 out；EOF 返回 false */
static bool lum__serial_fill_line(lum_Archive* archive, lum_Archive_line* out) {
  while (archive->pos < archive->size) {
    size_t begin = archive->pos;
    while (archive->pos < archive->size && archive->text[archive->pos] != '\n') archive->pos++;
    size_t len = archive->pos - begin;
    if (archive->pos < archive->size) archive->pos++;   /* 越过 \n */
    archive->line_no++;

    if (len >= Lum_Serial_Max_Line) len = Lum_Serial_Max_Line - 1;
    memcpy(out->raw, archive->text + begin, len);
    out->raw[len] = '\0';

    out->argc = 0;
    out->consumed = false;
    out->line_no = archive->line_no;
    char* p = out->raw;
    while (*p) {
      while (*p == ' ' || *p == '\t' || *p == '\r') *p++ = '\0';
      if (!*p) break;
      char* token = p;
      if (*p == '"') {
        p++;
        token = p;
        while (*p && *p != '"') p++;
        if (*p == '"') *p++ = '\0';
      } else {
        while (*p && *p != ' ' && *p != '\t' && *p != '\r') p++;
      }
      if (*p) *p++ = '\0';
      if (out->argc < Lum_Serial_Max_Args) {
        out->argv[out->argc++] = token;
      }
    }

    if (out->argc == 0) continue;          /* 空行 */
    if (out->argv[0][0] == '#') continue;  /* 注释行 */
    return true;
  }
  return false;
}

/* 行结构拷贝后 argv 仍指向源 raw —— 重定基址到目标自身的 raw */
static void lum__serial_rebase_argv(lum_Archive_line* dest, const lum_Archive_line* src) {
  for (int i = 0; i < src->argc; ++i) {
    dest->argv[i] = dest->raw + (src->argv[i] - src->raw);
  }
}

static void lum__serial_copy_line(lum_Archive_line* dest, const lum_Archive_line* src) {
  *dest = *src;
  lum__serial_rebase_argv(dest, src);
}

/* 组件段装载：把到下一个结构行（或 EOF）之前的全部字段行收进段缓冲；
   停止处的结构行保留在 pending 里供主循环 peek */
static void lum__serial_ensure_segment(lum_Archive* archive) {
  if (archive->segment_loaded) return;
  archive->segment_loaded = true;

  for (;;) {
    lum_Archive_line line;
    if (!lum__serial_fill_line(archive, &line)) return;
    if (lum__serial_is_structural(line.argv[0])) {
      lum__serial_copy_line(&archive->pending, &line);
      archive->has_pending = true;
      return;
    }
    if (lum_vector_size(&archive->segment) < Lum_Serial_Max_Segment_Fields) {
      lum_Archive_line* slot = (lum_Archive_line*)lum_vector_emplace(&archive->segment);
      if (slot) lum__serial_copy_line(slot, &line);
    }
    /* 超出字段数上限的行丢弃 */
  }
}

/* 段内按 key 搜索未消费的字段行；命中则标记消费并返回 */
static const lum_Archive_line* lum__serial_find_field(lum_Archive* archive, const char* key) {
  lum__serial_ensure_segment(archive);
  for (uint32_t i = 0; i < lum_vector_size(&archive->segment); ++i) {
    lum_Archive_line* line = (lum_Archive_line*)lum_vector_get(&archive->segment, i);
    if (!line->consumed && line->argc >= 2 && strcmp(line->argv[0], key) == 0) {
      line->consumed = true;
      return line;
    }
  }
  return NULL;
}

static const char* lum__serial_line_arg(const lum_Archive_line* line, int index) {
  if (index < 0 || index >= line->argc) return NULL;
  return line->argv[index];
}

/* ---------- 读方向：字段读取（段内任意顺序；缺失用默认值） ---------- */

int lum_serial_r_f32(lum_Archive* archive, const char* key, float def, float* out) {
  *out = def;
  const lum_Archive_line* line = lum__serial_find_field(archive, key);
  if (!line) return 0;
  *out = strtof(lum__serial_line_arg(line, 1), NULL);
  return 1;
}

int lum_serial_r_i32(lum_Archive* archive, const char* key, int32_t def, int32_t* out) {
  *out = def;
  const lum_Archive_line* line = lum__serial_find_field(archive, key);
  if (!line) return 0;
  *out = (int32_t)strtol(lum__serial_line_arg(line, 1), NULL, 10);
  return 1;
}

int lum_serial_r_u32(lum_Archive* archive, const char* key, uint32_t def, uint32_t* out) {
  *out = def;
  const lum_Archive_line* line = lum__serial_find_field(archive, key);
  if (!line) return 0;
  *out = (uint32_t)strtoul(lum__serial_line_arg(line, 1), NULL, 10);
  return 1;
}

int lum_serial_r_bool(lum_Archive* archive, const char* key, bool def, bool* out) {
  *out = def;
  const lum_Archive_line* line = lum__serial_find_field(archive, key);
  if (!line) return 0;
  const char* arg = lum__serial_line_arg(line, 1);
  *out = (arg[0] == 't' || arg[0] == '1');
  return 1;
}

int lum_serial_r_str(lum_Archive* archive, const char* key, const char* def, char* out, size_t cap) {
  if (def) {
    snprintf(out, cap, "%s", def);
  } else if (cap > 0) {
    out[0] = '\0';
  }
  const lum_Archive_line* line = lum__serial_find_field(archive, key);
  if (!line) return 0;
  snprintf(out, cap, "%s", lum__serial_line_arg(line, 1));
  return 1;
}

int lum_serial_r_vec2(lum_Archive* archive, const char* key, lum_Vec2 def, lum_Vec2* out) {
  *out = def;
  const lum_Archive_line* line = lum__serial_find_field(archive, key);
  if (!line || line->argc < 3) return 0;
  out->x = strtof(lum__serial_line_arg(line, 1), NULL);
  out->y = strtof(lum__serial_line_arg(line, 2), NULL);
  return 1;
}

int lum_serial_r_vec4(lum_Archive* archive, const char* key, lum_Vec4 def, lum_Vec4* out) {
  *out = def;
  const lum_Archive_line* line = lum__serial_find_field(archive, key);
  if (!line || line->argc < 5) return 0;
  out->x = strtof(lum__serial_line_arg(line, 1), NULL);
  out->y = strtof(lum__serial_line_arg(line, 2), NULL);
  out->z = strtof(lum__serial_line_arg(line, 3), NULL);
  out->w = strtof(lum__serial_line_arg(line, 4), NULL);
  return 1;
}

/* ---------- 读方向：结构行导航 ---------- */

bool lum_serial_peek(lum_Archive* archive) {
  if (archive->has_pending) return true;
  lum_Archive_line line;
  if (lum__serial_fill_line(archive, &line)) {
    lum__serial_copy_line(&archive->pending, &line);
    archive->has_pending = true;
  }
  return archive->has_pending;
}

const char* lum_serial_peek_key(lum_Archive* archive) {
  return lum_serial_peek(archive) ? archive->pending.argv[0] : NULL;
}

int lum_serial_peek_argc(lum_Archive* archive) {
  return lum_serial_peek(archive) ? archive->pending.argc : 0;
}

const char* lum_serial_peek_argv(lum_Archive* archive, int index) {
  return lum_serial_peek(archive) ? lum__serial_line_arg(&archive->pending, index) : NULL;
}

uint32_t lum_serial_peek_line(lum_Archive* archive) {
  return archive->pending.line_no;
}

void lum_serial_consume(lum_Archive* archive) {
  archive->has_pending = false;
}

void lum_serial_begin_component(lum_Archive* archive) {
  lum_vector_clear(&archive->segment);
  archive->segment_loaded = false;
}

/* ========== 声明式封装层：字段表驱动 ==========
   字段表本体存在 world 里（lum_ecs_set_component_field_table，不透明指针），
   随世界销毁自动消失——没有全局注册表，多世界天然安全。 */

typedef struct lum_Component_field_table {
  const lum_Component_field_desc* fields;
  uint32_t count;
} lum_Component_field_table;

static bool lum__serial_field_table(const lum_Ecs* ecs, lum_Component_type type, lum_Component_field_table* out) {
  const void* fields = NULL;
  uint32_t count = 0;
  if (!lum_ecs_component_field_table(ecs, type, &fields, &count)) return false;
  out->fields = (const lum_Component_field_desc*)fields;
  out->count = count;
  return true;
}

lum_Component_type lum_serial_register_component(lum_Ecs* ecs, const char* name, size_t size,
                                                 const lum_Component_field_desc* fields, uint32_t field_count) {
  if (field_count > Lum_Serial_Max_Segment_Fields) {
    Lum_Log_Warn("serial: component '%s' has %u fields, segment cap is %d; extra fields dropped.",
                 name, field_count, Lum_Serial_Max_Segment_Fields);
  }
  lum_Component_type type = lum_ecs_register_component(ecs, name, size);
  if (type == Lum_Component_Invalid) return type;
  lum_ecs_set_component_field_table(ecs, type, fields, field_count);
  return type;
}

static void lum__serial_table_write(const lum_Component_field_table* table, const void* component, lum_Archive* archive) {
  for (uint32_t i = 0; i < table->count; ++i) {
    const lum_Component_field_desc* field = &table->fields[i];
    const char* member = (const char*)component + field->offset;
    switch (field->kind) {
      case Lum_Component_field_F32:  lum_serial_w_f32 (archive, field->key, *(const float*)member);    break;
      case Lum_Component_field_I32:  lum_serial_w_i32 (archive, field->key, *(const int32_t*)member);  break;
      case Lum_Component_field_U32:  lum_serial_w_u32 (archive, field->key, *(const uint32_t*)member); break;
      case Lum_Component_field_Bool: lum_serial_w_bool(archive, field->key, *(const bool*)member);     break;
      case Lum_Component_field_Vec2: lum_serial_w_vec2(archive, field->key, *(const lum_Vec2*)member); break;
      case Lum_Component_field_Vec4: lum_serial_w_vec4(archive, field->key, *(const lum_Vec4*)member); break;
      case Lum_Component_field_Str:  lum_serial_w_str (archive, field->key, member);                   break;
    }
  }
}

static void lum__serial_table_read(const lum_Component_field_table* table, void* component, lum_Archive* archive) {
  for (uint32_t i = 0; i < table->count; ++i) {
    const lum_Component_field_desc* field = &table->fields[i];
    char* member = (char*)component + field->offset;
    switch (field->kind) {
      case Lum_Component_field_F32:  lum_serial_r_f32 (archive, field->key, field->def.f,  (float*)member);                         break;
      case Lum_Component_field_I32:  lum_serial_r_i32 (archive, field->key, field->def.i,  (int32_t*)member);                       break;
      case Lum_Component_field_U32:  lum_serial_r_u32 (archive, field->key, field->def.u,  (uint32_t*)member);                      break;
      case Lum_Component_field_Bool: lum_serial_r_bool(archive, field->key, field->def.b,  (bool*)member);                          break;
      case Lum_Component_field_Vec2: lum_serial_r_vec2(archive, field->key, field->def.v2, (lum_Vec2*)member);                      break;
      case Lum_Component_field_Vec4: lum_serial_r_vec4(archive, field->key, field->def.v4, (lum_Vec4*)member);                      break;
      case Lum_Component_field_Str:  lum_serial_r_str (archive, field->key, field->def.s ? field->def.s : "", member, field->size); break;
    }
  }
}

int lum_serial_component_write(const lum_Ecs* ecs, lum_Component_type type, const void* component, lum_Archive* archive) {
  if (!ecs || !component || !archive || !archive->writing) return 0;

  const char* name = lum_ecs_component_name(ecs, type);
  lum_Component_field_table table;
  bool has_table = lum__serial_field_table(ecs, type, &table);
  lum_Component_write_fn write = NULL;
  lum_Component_read_fn read_fn = NULL;
  bool has_callback = lum_ecs_component_serializer(ecs, type, &write, &read_fn);
  if (!name || (!has_table && !has_callback)) return 0;   /* 无名或无序列化途径：不产生任何输出 */

  if (!lum_serial_w_linef(archive, "component %s", name)) return 0;
  if (has_table) {
    lum__serial_table_write(&table, component, archive);
  } else {
    write(component, archive);
  }
  return 1;
}

int lum_serial_component_read(lum_Ecs* ecs, lum_Component_type type, void* component, lum_Archive* archive) {
  if (!ecs || !component || !archive || archive->writing) return 0;

  lum_Component_field_table table;
  bool has_table = lum__serial_field_table(ecs, type, &table);
  lum_Component_write_fn write = NULL;
  lum_Component_read_fn read = NULL;
  bool has_callback = lum_ecs_component_serializer(ecs, type, &write, &read);
  if (!has_table && !has_callback) return 0;

  lum_serial_begin_component(archive);
  if (has_table) {
    lum__serial_table_read(&table, component, archive);
  } else {
    read(component, archive);
  }
  return 1;
}

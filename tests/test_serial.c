/* 序列化自测：纯逻辑，无 GL。
   编译：gcc -Isrc tests/test_serial.c src/lum_serial.c src/lum_ecs.c src/containers/lum_vector.c src/lum_log.c -o test_serial */
#include "lum_serial.h"
#include "lum_ecs.h"
#include <stdio.h>
#include <string.h>

static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { \
  if (cond) { ++g_pass; } \
  else { ++g_fail; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

typedef struct {
  lum_Vec2 pos;
  float    rot;
  bool     flip;
  char     name[32];
  lum_Vec4 tint;
  int32_t  hp;
  uint32_t flags;
} T_fake;

static const T_fake g_sample = {
  { 100.5f, -3.25f }, 0.125f, true, "player one",
  { 1.0f, 0.5f, 0.25f, 1.0f }, -42, 0xDEADBEEFu
};

static int fake_write(const void* comp, lum_Archive* a) {
  const T_fake* f = (const T_fake*)comp;
  if (!lum_serial_w_vec2(a, "pos", f->pos)) return 0;
  if (!lum_serial_w_f32 (a, "rot", f->rot)) return 0;
  if (!lum_serial_w_bool(a, "flip", f->flip)) return 0;
  if (!lum_serial_w_str (a, "name", f->name)) return 0;
  if (!lum_serial_w_vec4(a, "tint", f->tint)) return 0;
  if (!lum_serial_w_i32 (a, "hp", f->hp)) return 0;
  if (!lum_serial_w_u32 (a, "flags", f->flags)) return 0;
  return 1;
}

static int fake_read(void* comp, lum_Archive* a) {
  T_fake* f = (T_fake*)comp;
  memset(f, 0, sizeof(*f));
  /* 故意乱序读取：字段顺序与写入不同 */
  lum_serial_r_i32 (a, "hp",    -1, &f->hp);
  lum_serial_r_vec2(a, "pos",   lum_vec2_0(), &f->pos);
  lum_serial_r_u32 (a, "flags", 0u, &f->flags);
  lum_serial_r_f32 (a, "rot",   0.0f, &f->rot);
  lum_serial_r_vec4(a, "tint",  lum_vec4_create(1, 1, 1, 1), &f->tint);
  lum_serial_r_str (a, "name",  "?", f->name, sizeof(f->name));
  lum_serial_r_bool(a, "flip",  false, &f->flip);
  return 1;
}

static int check_fake_equal(const T_fake* a, const T_fake* b) {
  return a->pos.x == b->pos.x && a->pos.y == b->pos.y &&
         a->rot == b->rot && a->flip == b->flip &&
         strcmp(a->name, b->name) == 0 &&
         a->tint.x == b->tint.x && a->tint.y == b->tint.y &&
         a->tint.z == b->tint.z && a->tint.w == b->tint.w &&
         a->hp == b->hp && a->flags == b->flags;
}

static void test_roundtrip(void) {
  /* 写：结构行 + 组件字段 */
  lum_Archive writer;
  CHECK(lum_serial_init_writer(&writer));
  CHECK(lum_serial_w_linef(&writer, "version %u", 1));
  CHECK(lum_serial_w_linef(&writer, "entity %u %s", 0, "ship"));
  CHECK(lum_serial_w_linef(&writer, "component fake"));
  CHECK(fake_write(&g_sample, &writer));
  CHECK(lum_serial_w_linef(&writer, "entity %u", 1));   /* r_* 不应吃掉这行 */

  size_t size = 0;
  const char* text = lum_serial_writer_finish(&writer, &size);
  CHECK(text && size > 0);

  /* 读：结构行导航 + 乱序字段 + 引号串 */
  lum_Archive reader;
  CHECK(lum_serial_init_reader(&reader, text));

  CHECK(lum_serial_peek(&reader));
  CHECK(strcmp(lum_serial_peek_key(&reader), "version") == 0);
  CHECK(lum_serial_peek_argc(&reader) == 2);
  CHECK(strcmp(lum_serial_peek_argv(&reader, 1), "1") == 0);
  lum_serial_consume(&reader);

  CHECK(strcmp(lum_serial_peek_key(&reader), "entity") == 0);
  CHECK(strcmp(lum_serial_peek_argv(&reader, 2), "ship") == 0);
  lum_serial_consume(&reader);

  CHECK(strcmp(lum_serial_peek_key(&reader), "component") == 0);
  lum_serial_consume(&reader);
  lum_serial_begin_component(&reader);

  T_fake loaded;
  CHECK(fake_read(&loaded, &reader));
  CHECK(check_fake_equal(&loaded, &g_sample));   /* 乱序 + 含空格名字（引号）往返一致 */

  /* 组件字段读取没有越界吃掉下一个结构行 */
  CHECK(strcmp(lum_serial_peek_key(&reader), "entity") == 0);
  CHECK(lum_serial_peek_argc(&reader) == 2);
  lum_serial_consume(&reader);
  CHECK(!lum_serial_peek(&reader));              /* 文件结束 */

  lum_serial_destroy(&writer);
  lum_serial_destroy(&reader);
}

static void test_defaults_and_comments(void) {
  const char* text =
    "# 注释行\n"
    "\n"
    "component fake\n"
    "hp 7\n"
    "component fake\n"
    "rot 2.5\n";

  lum_Archive reader;
  CHECK(lum_serial_init_reader(&reader, text));
  CHECK(strcmp(lum_serial_peek_key(&reader), "component") == 0);
  lum_serial_consume(&reader);
  lum_serial_begin_component(&reader);

  T_fake f;
  lum_serial_r_i32(&reader, "hp", -1, &f.hp);
  CHECK(f.hp == 7);

  /* 缺失字段用默认值，且不消费下一行 */
  lum_serial_r_f32(&reader, "rot", 0.0f, &f.rot);
  CHECK(f.rot == 0.0f);
  lum_serial_r_str(&reader, "name", "anon", f.name, sizeof(f.name));
  CHECK(strcmp(f.name, "anon") == 0);
  lum_serial_r_bool(&reader, "flip", true, &f.flip);
  CHECK(f.flip == true);

  /* 第二段组件 */
  CHECK(strcmp(lum_serial_peek_key(&reader), "component") == 0);
  lum_serial_consume(&reader);
  lum_serial_begin_component(&reader);
  lum_serial_r_f32(&reader, "rot", 0.0f, &f.rot);
  CHECK(f.rot == 2.5f);
  lum_serial_r_i32(&reader, "hp", -1, &f.hp);
  CHECK(f.hp == -1);

  lum_serial_destroy(&reader);
}

static lum_Component_type T_fake_id;

static void test_ecs_serializer_registry(void) {
  lum_Ecs* ecs = lum_ecs_create();
  T_fake_id = lum_ecs_register_component(ecs, "fake", sizeof(T_fake));
  lum_Component_type other_id = lum_ecs_register_component(ecs, "other", sizeof(float));

  /* 未注册序列化器 → false */
  lum_Component_write_fn w; lum_Component_read_fn r;
  CHECK(!lum_ecs_component_serializer(ecs, T_fake_id, &w, &r));

  lum_ecs_set_component_serializer(ecs, T_fake_id, fake_write, fake_read);
  CHECK(lum_ecs_component_serializer(ecs, T_fake_id, &w, &r));
  CHECK(w == fake_write && r == fake_read);
  CHECK(!lum_ecs_component_serializer(ecs, other_id, &w, &r));   /* 惰性对齐后其他类型仍为空 */

  /* 实体组件枚举 */
  lum_Entity e = lum_ecs_create_entity(ecs);
  CHECK(lum_ecs_entity_component_types(ecs, e, NULL, 0) == 0);
  lum_ecs_add(ecs, e, T_fake_id);
  lum_ecs_add(ecs, e, other_id);

  lum_Component_type types[4];
  uint32_t n = lum_ecs_entity_component_types(ecs, e, types, 4);
  CHECK(n == 2 && types[0] == T_fake_id && types[1] == other_id);

  /* cap 截断语义：返回总数 */
  CHECK(lum_ecs_entity_component_types(ecs, e, types, 1) == 2);

  /* 注销 */
  lum_ecs_remove(ecs, e, T_fake_id);
  CHECK(lum_ecs_entity_component_types(ecs, e, types, 4) == 1 && types[0] == other_id);

  lum_ecs_destroy(ecs);
}

/* ========== 声明式字段表封装层 ========== */

typedef struct {
  lum_Vec2 pos;
  float    speed;
  bool     flip;
  int32_t  hp;
  char     tag[24];
} T_table;

static const lum_Component_field_desc g_table_fields[] = {
  Lum_Component_Make_Field_Vec2(T_table, pos,   "pos",   0, 0),
  Lum_Component_Make_Field_F32 (T_table, speed, "speed", 5.0f),
  Lum_Component_Make_Field_Bool(T_table, flip,  "flip",  false),
  Lum_Component_Make_Field_I32 (T_table, hp,    "hp",    100),
  Lum_Component_Make_Field_Str (T_table, tag,   "tag",   "none"),
};

static void test_field_table_roundtrip(void) {
  lum_Ecs* ecs = lum_ecs_create();
  lum_Component_type t = lum_serial_register_component(ecs, "table_comp", sizeof(T_table),
                                                       g_table_fields, 5);
  CHECK(t != Lum_Component_Invalid);

  T_table src = { { 3.5f, -7.25f }, 12.5f, true, -8, "hello world" };

  lum_Archive writer;
  CHECK(lum_serial_init_writer(&writer));
  CHECK(lum_serial_component_write(ecs, t, &src, &writer));
  size_t size = 0;
  const char* text = lum_serial_writer_finish(&writer, &size);

  /* 写出的是完整块：component 行 + 字段行 */
  lum_Archive reader;
  CHECK(lum_serial_init_reader(&reader, text));
  CHECK(strcmp(lum_serial_peek_key(&reader), "component") == 0);
  CHECK(strcmp(lum_serial_peek_argv(&reader, 1), "table_comp") == 0);
  lum_serial_consume(&reader);

  T_table dst;
  CHECK(lum_serial_component_read(ecs, t, &dst, &reader));   /* 内部自调 begin_component */
  CHECK(dst.pos.x == src.pos.x && dst.pos.y == src.pos.y);
  CHECK(dst.speed == src.speed);
  CHECK(dst.flip == src.flip);
  CHECK(dst.hp == src.hp);
  CHECK(strcmp(dst.tag, src.tag) == 0);                       /* 含空格字符串经引号往返 */
  CHECK(!lum_serial_peek(&reader));                           /* 块已读完 */

  lum_serial_destroy(&writer);
  lum_serial_destroy(&reader);
  lum_ecs_destroy(ecs);
}

static void test_field_table_defaults(void) {
  lum_Ecs* ecs = lum_ecs_create();
  lum_Component_type t = lum_serial_register_component(ecs, "table_comp", sizeof(T_table),
                                                       g_table_fields, 5);

  /* 手写缺字段的文件：speed/hp/tag 缺失 → 默认值 */
  const char* text =
    "component table_comp\n"
    "pos 1 2\n"
    "flip true\n";

  lum_Archive reader;
  CHECK(lum_serial_init_reader(&reader, text));
  CHECK(strcmp(lum_serial_peek_key(&reader), "component") == 0);
  lum_serial_consume(&reader);

  T_table dst;
  CHECK(lum_serial_component_read(ecs, t, &dst, &reader));
  CHECK(dst.pos.x == 1.0f && dst.pos.y == 2.0f);
  CHECK(dst.flip == true);
  CHECK(dst.speed == 5.0f);                 /* 表默认 */
  CHECK(dst.hp == 100);                     /* 表默认 */
  CHECK(strcmp(dst.tag, "none") == 0);      /* 表默认 */

  lum_serial_destroy(&reader);
  lum_ecs_destroy(ecs);
}

static void test_dispatch_priority_and_missing(void) {
  lum_Ecs* ecs = lum_ecs_create();

  /* 自定义回调组件：走 component_write/read 的回调路径 */
  lum_Component_type fake = lum_ecs_register_component(ecs, "fake", sizeof(T_fake));
  lum_ecs_set_component_serializer(ecs, fake, fake_write, fake_read);

  lum_Archive writer;
  CHECK(lum_serial_init_writer(&writer));
  CHECK(lum_serial_component_write(ecs, fake, &g_sample, &writer));
  const char* text = lum_serial_writer_finish(&writer, NULL);

  lum_Archive reader;
  CHECK(lum_serial_init_reader(&reader, text));
  CHECK(strcmp(lum_serial_peek_key(&reader), "component") == 0);
  lum_serial_consume(&reader);   /* peek 之后 consume 才有效 */
  T_fake dst;
  CHECK(lum_serial_component_read(ecs, fake, &dst, &reader));
  CHECK(check_fake_equal(&dst, &g_sample));

  /* 既无表也无回调：返回 0 且不写任何行 */
  lum_Component_type plain = lum_ecs_register_component(ecs, "plain", sizeof(float));
  lum_Archive writer2;
  lum_serial_init_writer(&writer2);
  float v = 1.0f;
  CHECK(lum_serial_component_write(ecs, plain, &v, &writer2) == 0);
  size_t sz2 = 99;
  const char* text2 = lum_serial_writer_finish(&writer2, &sz2);
  CHECK(sz2 == 0 && text2 && text2[0] == '\0');

  lum_serial_destroy(&writer);
  lum_serial_destroy(&writer2);
  lum_serial_destroy(&reader);
  lum_ecs_destroy(ecs);
}

int main(void) {
  test_roundtrip();
  test_defaults_and_comments();
  test_ecs_serializer_registry();
  test_field_table_roundtrip();
  test_field_table_defaults();
  test_dispatch_priority_and_missing();
  printf("serial tests: %d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}

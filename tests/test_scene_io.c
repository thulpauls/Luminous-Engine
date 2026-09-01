/* 场景序列化自测：纯逻辑（不触 GL 调用；链接含 renderer2d/glad 但不初始化）。
   编译：gcc -Isrc -Iinclude tests/test_scene_io.c src/lum_scene_io.c src/lum_scene.c
   src/lum_serial.c src/lum_ecs.c src/lum_assets.c src/lum_text_renderer.c src/lum_renderer2d.c
   src/lum_material.c src/lum_shader.c src/lum_transform.c src/lum_math2d.c src/lum_texture.c
   src/lum_render_target.c src/lum_filesystem.c src/lum_atlas.c src/lum_camera.c
   src/containers/lum_vector.c src/effects/lum_effects.c src/effects/lum_glow.c
   src/effects/lum_beam.c glad.c src/lum_log.c -o test_scene_io */
#include "lum_scene_io.h"
#include "lum_scene.h"
#include "components/lum_components.h"
#include "effects/lum_beam.h"
#include "lum_filesystem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { \
  if (cond) { ++g_pass; } \
  else { ++g_fail; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

#define TEST_PATH "test_scene_io_roundtrip.lum"

/* 构建测试世界：ship(位置+旋转) ← moon(子实体, 偏移) ← moon_moon(孙实体)，
   各带 transform + sprite（sprite.texture 置 NULL 模拟无资源环境）+ 实体名 */
static void build_world(lum_Ecs* ecs, lum_Entity* out_ship, lum_Entity* out_moon, lum_Entity* out_grandchild) {
  lum_Entity ship = lum_ecs_create_entity(ecs);
  lum_scene_add_name(ecs, ship, "ship");
  lum_Component_transform* ship_transform = lum_scene_add_transform(ecs, ship);
  ship_transform->position = lum_vec2_create(480.0f, 300.0f);
  ship_transform->rotation = 0.5f;
  lum_Component_sprite* ship_sprite = lum_scene_add_sprite(ecs, ship, NULL);
  ship_sprite->size = lum_vec2_create(48, 48);
  ship_sprite->layer = 1;
  *out_ship = ship;

  lum_Entity moon = lum_ecs_create_entity(ecs);
  lum_scene_add_name(ecs, moon, "moon");
  lum_Component_transform* moon_transform = lum_scene_add_transform(ecs, moon);
  moon_transform->position = lum_vec2_create(70, 0);
  moon_transform->scale = lum_vec2_create(1.5f, 1.5f);
  lum_scene_add_sprite(ecs, moon, NULL);
  lum_scene_set_parent(ecs, moon, ship);
  *out_moon = moon;

  lum_Entity grandchild = lum_ecs_create_entity(ecs);
  lum_scene_add_name(ecs, grandchild, "moon_moon");
  lum_scene_add_transform(ecs, grandchild)->position = lum_vec2_create(10, 5);
  lum_scene_set_parent(ecs, grandchild, moon);
  *out_grandchild = grandchild;

  /* 引擎扩展组件：非默认值 glow / beam 实体（字段表序列化往返；位置设为独占值便于查找） */
  lum_Entity glow_entity = lum_ecs_create_entity(ecs);
  lum_scene_add_name(ecs, glow_entity, "glow_test");
  lum_scene_add_transform(ecs, glow_entity)->position = lum_vec2_create(999.0f, 111.0f);
  lum_Component_glow* glow = lum_component_glow_add(ecs, glow_entity);
  glow->ring = true;
  glow->radius = 33.5f;
  glow->layer = 2;
  glow->color = lum_vec4_create(0.5f, 0.25f, 0.75f, 1.0f);

  lum_Entity beam_entity = lum_ecs_create_entity(ecs);
  lum_scene_add_name(ecs, beam_entity, "beam_test");
  lum_scene_add_transform(ecs, beam_entity)->position = lum_vec2_create(50.0f, 60.0f);
  lum_Component_beam* beam = lum_component_beam_add(ecs, beam_entity);
  beam->start_offset = lum_vec2_create(1.0f, 2.0f);
  beam->end_offset = lum_vec2_create(3.0f, 4.0f);
  beam->thickness = 12.5f;
  beam->mode = Lum_Beam_Mode_Center;
  beam->layer = 3;
}

static char* read_file_text(const char* path) {
  lum_Binary_file file = lum_fs_read_binary(path);
  if (!file.data) return NULL;
  char* text = (char*)malloc(file.size + 1);
  memcpy(text, file.data, file.size);
  text[file.size] = '\0';
  free(file.data);
  return text;
}

static void test_roundtrip_and_determinism(void) {
  lum_Ecs* world = lum_ecs_create();
  lum_scene_init(world);
  lum_scene_io_init(world);
  CHECK(lum_components_register(world));

  lum_Entity ship, moon, grandchild;
  build_world(world, &ship, &moon, &grandchild);

  /* 保存两次：确定性（逐字节相同） */
  CHECK(lum_scene_save(world, TEST_PATH));
  char* first = read_file_text(TEST_PATH);
  CHECK(first != NULL);
  CHECK(lum_scene_save(world, TEST_PATH));
  char* second = read_file_text(TEST_PATH);
  CHECK(second != NULL);
  CHECK(strcmp(first, second) == 0);

  /* 加载进新世界 */
  lum_Ecs* restored = lum_ecs_create();
  lum_scene_init(restored);
  lum_scene_io_init(restored);
  CHECK(lum_components_register(restored));
  CHECK(lum_scene_load(restored, TEST_PATH));

  /* 实体数一致 */
  uint32_t original_alive = 0, restored_alive = 0;
  for (uint32_t i = 0; i < lum_ecs_entity_slot_count(world); ++i)
    if (lum_ecs_entity_at_slot(world, i) != Lum_Entity_Null) original_alive++;
  for (uint32_t i = 0; i < lum_ecs_entity_slot_count(restored); ++i)
    if (lum_ecs_entity_at_slot(restored, i) != Lum_Entity_Null) restored_alive++;
  CHECK(original_alive == 5 && restored_alive == 5);

  /* 找到恢复后的 ship（根实体，transform.position 匹配） */
  lum_Entity restored_ship = Lum_Entity_Null, restored_moon = Lum_Entity_Null,
             restored_grandchild = Lum_Entity_Null;
  for (uint32_t i = 0; i < lum_ecs_entity_slot_count(restored); ++i) {
    lum_Entity e = lum_ecs_entity_at_slot(restored, i);
    if (e == Lum_Entity_Null) continue;
    if (lum_scene_parent(restored, e) == Lum_Entity_Null) {
      lum_Component_transform* t = (lum_Component_transform*)lum_ecs_get(restored, e, lum_Component_transform_id);
      if (t && t->position.x == 480.0f && t->rotation == 0.5f) restored_ship = e;
    }
  }
  CHECK(restored_ship != Lum_Entity_Null);

  /* 层级结构等价 + 局部值等价 */
  restored_moon = lum_scene_first_child(restored, restored_ship);
  CHECK(restored_moon != Lum_Entity_Null);
  lum_Component_transform* moon_transform =
      (lum_Component_transform*)lum_ecs_get(restored, restored_moon, lum_Component_transform_id);
  CHECK(moon_transform && moon_transform->position.x == 70.0f && moon_transform->position.y == 0.0f);
  CHECK(moon_transform->scale.x == 1.5f && moon_transform->scale.y == 1.5f);
  CHECK(lum_scene_parent(restored, restored_moon) == restored_ship);

  restored_grandchild = lum_scene_first_child(restored, restored_moon);
  CHECK(restored_grandchild != Lum_Entity_Null);
  CHECK(lum_scene_parent(restored, restored_grandchild) == restored_moon);
  CHECK(lum_scene_child_count(restored, restored_grandchild) == 0);

  /* 名字往返：文件里有 component name 段，恢复后按实体对得上 */
  CHECK(strstr(first, "component name") != NULL);
  {
    const char* ship_name   = lum_scene_name(restored, restored_ship);
    const char* moon_name   = lum_scene_name(restored, restored_moon);
    const char* grand_name  = lum_scene_name(restored, restored_grandchild);
    CHECK(ship_name  && strcmp(ship_name,  "ship")      == 0);
    CHECK(moon_name  && strcmp(moon_name,  "moon")      == 0);
    CHECK(grand_name && strcmp(grand_name, "moon_moon") == 0);
  }

  /* 扩展组件字段往返：按独占的 transform 位置找到恢复的 glow/beam 实体 */
  {
    lum_Entity restored_glow = Lum_Entity_Null, restored_beam = Lum_Entity_Null;
    for (uint32_t i = 0; i < lum_ecs_entity_slot_count(restored); ++i) {
      lum_Entity e = lum_ecs_entity_at_slot(restored, i);
      if (e == Lum_Entity_Null) continue;
      lum_Component_transform* t = (lum_Component_transform*)lum_ecs_get(restored, e, lum_Component_transform_id);
      if (!t) continue;
      if (t->position.x == 999.0f && t->position.y == 111.0f) restored_glow = e;
      if (t->position.x == 50.0f && t->position.y == 60.0f) restored_beam = e;
    }
    CHECK(restored_glow != Lum_Entity_Null && restored_beam != Lum_Entity_Null);

    const lum_Component_glow* glow =
        (const lum_Component_glow*)lum_ecs_get(restored, restored_glow, lum_component_glow_type());
    CHECK(glow && glow->ring == true && glow->radius == 33.5f && glow->layer == 2);
    CHECK(glow->color.x == 0.5f && glow->color.y == 0.25f && glow->color.z == 0.75f);

    const lum_Component_beam* beam =
        (const lum_Component_beam*)lum_ecs_get(restored, restored_beam, lum_component_beam_type());
    CHECK(beam && beam->thickness == 12.5f && beam->mode == Lum_Beam_Mode_Center && beam->layer == 3);
    CHECK(beam->start_offset.x == 1.0f && beam->start_offset.y == 2.0f);
    CHECK(beam->end_offset.x == 3.0f && beam->end_offset.y == 4.0f);
  }

  /* sprite 字段往返（texture 为 NULL → 空 path → NULL） */
  lum_Component_sprite* ship_sprite =
      (lum_Component_sprite*)lum_ecs_get(restored, restored_ship, lum_Component_sprite_id);
  CHECK(ship_sprite && ship_sprite->texture == NULL);
  CHECK(ship_sprite->size.x == 48.0f && ship_sprite->layer == 1);

  /* world 矩阵派生数据：跑一次管线后 moon.world ≠ identity（由 ship × local 合成） */
  lum_scene_update(restored, 0.0f);
  lum_Component_transform* restored_ship_transform =
      (lum_Component_transform*)lum_ecs_get(restored, restored_ship, lum_Component_transform_id);
  CHECK(restored_ship_transform->world.m[12] != 0.0f);   /* 平移分量非零 = 管线重建了矩阵 */

  free(first);
  free(second);
  lum_ecs_destroy(world);
  lum_ecs_destroy(restored);
  remove(TEST_PATH);
}

static void test_error_paths(void) {
  lum_Ecs* ecs = lum_ecs_create();
  lum_scene_init(ecs);
  lum_scene_io_init(ecs);

  /* 不存在的文件 */
  CHECK(!lum_scene_load(ecs, "build/no_such_scene.lum"));

  /* 坏版本号拒载 */
  lum_fs_write("bad_version.lum", "version 999\n");
  CHECK(!lum_scene_load(ecs, "bad_version.lum"));

  /* 无版本头拒载 */
  lum_fs_write("no_version.lum", "entity 0\n");
  CHECK(!lum_scene_load(ecs, "no_version.lum"));

  /* 未知组件名：跳过不崩，已知组件照常 */
  const char* messy =
    "version 1\n"
    "entity 0\n"
    "component future_component\n"
    "some_field 1 2 3\n"
    "component transform\n"
    "position 5 6\n";
  lum_fs_write("messy.lum", messy);
  CHECK(lum_scene_load(ecs, "messy.lum"));
  CHECK(lum_ecs_entity_slot_count(ecs) >= 1);
  lum_Entity e = lum_ecs_entity_at_slot(ecs, 0);
  CHECK(e != Lum_Entity_Null);
  lum_Component_transform* t = (lum_Component_transform*)lum_ecs_get(ecs, e, lum_Component_transform_id);
  CHECK(t && t->position.x == 5.0f && t->position.y == 6.0f);

  lum_ecs_destroy(ecs);
}

static void test_name_component(void) {
  lum_Ecs* ecs = lum_ecs_create();
  lum_scene_init(ecs);
  lum_scene_io_init(ecs);

  lum_Entity e = lum_ecs_create_entity(ecs);

  /* 无 name 组件：get 返回 NULL */
  CHECK(lum_scene_name(ecs, e) == NULL);

  /* add 拷贝文本；NULL 视为空名 */
  lum_Component_name* component = lum_scene_add_name(ecs, e, "player");
  CHECK(component && strcmp(component->text, "player") == 0);
  CHECK(lum_scene_name(ecs, e) && strcmp(lum_scene_name(ecs, e), "player") == 0);

  /* set 对已有组件就地更新（不重复添加） */
  lum_scene_set_name(ecs, e, "renamed");
  CHECK(lum_scene_name(ecs, e) && strcmp(lum_scene_name(ecs, e), "renamed") == 0);
  {
    lum_Component_type types[8];
    CHECK(lum_ecs_entity_component_types(ecs, e, types, 8) == 1u);   /* 仍只有 name 一个组件 */
    CHECK(types[0] == lum_Component_name_id);
  }

  /* set 对无组件实体自动添加 */
  lum_Entity other = lum_ecs_create_entity(ecs);
  lum_scene_set_name(ecs, other, "auto_added");
  CHECK(lum_scene_name(ecs, other) && strcmp(lum_scene_name(ecs, other), "auto_added") == 0);

  /* 超长截断到 Lum_Scene_Name_Max-1 + NUL */
  char long_name[Lum_Scene_Name_Max + 16];
  memset(long_name, 'x', sizeof(long_name));
  long_name[sizeof(long_name) - 1] = '\0';
  lum_scene_set_name(ecs, other, long_name);
  const char* stored = lum_scene_name(ecs, other);
  CHECK(stored && strlen(stored) == Lum_Scene_Name_Max - 1);
  for (uint32_t i = 0; i < Lum_Scene_Name_Max - 1; ++i) CHECK(stored[i] == 'x');

  /* NULL 名 → 空串（组件存在） */
  lum_scene_set_name(ecs, other, NULL);
  stored = lum_scene_name(ecs, other);
  CHECK(stored && stored[0] == '\0');

  lum_ecs_destroy(ecs);
}

int main(void) {
  test_roundtrip_and_determinism();
  test_name_component();
  test_error_paths();
  printf("scene_io tests: %d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}

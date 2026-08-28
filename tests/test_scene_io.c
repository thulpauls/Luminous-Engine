/* 场景序列化自测：纯逻辑（不触 GL 调用；链接含 renderer2d/glad 但不初始化）。
   编译：gcc -Isrc -Iinclude tests/test_scene_io.c src/lum_scene_io.c src/lum_scene.c
   src/lum_serial.c src/lum_ecs.c src/lum_assets.c src/lum_text_renderer.c src/lum_renderer2d.c
   src/lum_material.c src/lum_shader.c src/lum_transform.c src/lum_math2d.c src/lum_texture.c
   src/lum_render_target.c src/lum_filesystem.c src/lum_atlas.c src/lum_camera.c
   src/containers/lum_vector.c src/effects/lum_effects.c src/effects/lum_glow.c
   src/effects/lum_beam.c glad.c src/lum_log.c -o test_scene_io */
#include "lum_scene_io.h"
#include "lum_scene.h"
#include "lum_filesystem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { \
  if (cond) { ++g_pass; } \
  else { ++g_fail; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

#define TEST_PATH "build/test_scene_io_roundtrip.lum"

/* 构建测试世界：ship(位置+旋转) ← moon(子实体, 偏移) ← moon_moon(孙实体)，
   各带 transform + sprite（sprite.texture 置 NULL 模拟无资源环境） */
static void build_world(lum_Ecs* ecs, lum_Entity* out_ship, lum_Entity* out_moon, lum_Entity* out_grandchild) {
  lum_Entity ship = lum_ecs_create_entity(ecs);
  lum_Component_transform* ship_transform = lum_scene_add_transform(ecs, ship);
  ship_transform->position = lum_vec2_create(480.0f, 300.0f);
  ship_transform->rotation = 0.5f;
  lum_Component_sprite* ship_sprite = lum_scene_add_sprite(ecs, ship, NULL);
  ship_sprite->size = lum_vec2_create(48, 48);
  ship_sprite->layer = 1;
  *out_ship = ship;

  lum_Entity moon = lum_ecs_create_entity(ecs);
  lum_Component_transform* moon_transform = lum_scene_add_transform(ecs, moon);
  moon_transform->position = lum_vec2_create(70, 0);
  moon_transform->scale = lum_vec2_create(1.5f, 1.5f);
  lum_scene_add_sprite(ecs, moon, NULL);
  lum_scene_set_parent(ecs, moon, ship);
  *out_moon = moon;

  lum_Entity grandchild = lum_ecs_create_entity(ecs);
  lum_scene_add_transform(ecs, grandchild)->position = lum_vec2_create(10, 5);
  lum_scene_set_parent(ecs, grandchild, moon);
  *out_grandchild = grandchild;
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
  CHECK(lum_scene_load(restored, TEST_PATH));

  /* 实体数一致 */
  uint32_t original_alive = 0, restored_alive = 0;
  for (uint32_t i = 0; i < lum_ecs_entity_slot_count(world); ++i)
    if (lum_ecs_entity_at_slot(world, i) != Lum_Entity_Null) original_alive++;
  for (uint32_t i = 0; i < lum_ecs_entity_slot_count(restored); ++i)
    if (lum_ecs_entity_at_slot(restored, i) != Lum_Entity_Null) restored_alive++;
  CHECK(original_alive == 3 && restored_alive == 3);

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
  lum_fs_write("build/bad_version.lum", "version 999\n");
  CHECK(!lum_scene_load(ecs, "build/bad_version.lum"));

  /* 无版本头拒载 */
  lum_fs_write("build/no_version.lum", "entity 0\n");
  CHECK(!lum_scene_load(ecs, "build/no_version.lum"));

  /* 未知组件名：跳过不崩，已知组件照常 */
  const char* messy =
    "version 1\n"
    "entity 0\n"
    "component future_component\n"
    "some_field 1 2 3\n"
    "component transform\n"
    "position 5 6\n";
  lum_fs_write("build/messy.lum", messy);
  CHECK(lum_scene_load(ecs, "build/messy.lum"));
  CHECK(lum_ecs_entity_slot_count(ecs) >= 1);
  lum_Entity e = lum_ecs_entity_at_slot(ecs, 0);
  CHECK(e != Lum_Entity_Null);
  lum_Component_transform* t = (lum_Component_transform*)lum_ecs_get(ecs, e, lum_Component_transform_id);
  CHECK(t && t->position.x == 5.0f && t->position.y == 6.0f);

  lum_ecs_destroy(ecs);
}

int main(void) {
  test_roundtrip_and_determinism();
  test_error_paths();
  printf("scene_io tests: %d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}

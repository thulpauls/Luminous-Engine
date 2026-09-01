/* 场景层自测：拾取查询（AABB 命中 / 层序 / 层级缩放 / 无 sprite 不可拾取）。
   纯逻辑，不触 GL。由 CMake 目标 test_scene 运行。 */
#include "lum_scene.h"
#include <stdio.h>

static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { \
  if (cond) { ++g_pass; } \
  else { ++g_fail; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

static void test_pick(void) {
  lum_Ecs* ecs = lum_ecs_create();
  lum_scene_init(ecs);

  /* bottom：大精灵铺底（层 0）；top：重叠处的小精灵（层 5）；far：远处独立精灵 */
  lum_Entity bottom = lum_ecs_create_entity(ecs);
  lum_scene_add_transform(ecs, bottom);
  lum_Component_sprite* bottom_sprite = lum_scene_add_sprite(ecs, bottom, NULL);
  bottom_sprite->size = lum_vec2_create(100, 100);
  bottom_sprite->layer = 0;

  lum_Entity top = lum_ecs_create_entity(ecs);
  lum_Component_transform* top_transform = lum_scene_add_transform(ecs, top);
  top_transform->position = lum_vec2_create(10, 10);
  lum_Component_sprite* top_sprite = lum_scene_add_sprite(ecs, top, NULL);
  top_sprite->size = lum_vec2_create(40, 40);
  top_sprite->layer = 5;

  lum_Entity far = lum_ecs_create_entity(ecs);
  lum_Component_transform* far_transform = lum_scene_add_transform(ecs, far);
  far_transform->position = lum_vec2_create(500, 0);
  lum_Component_sprite* far_sprite = lum_scene_add_sprite(ecs, far, NULL);
  far_sprite->size = lum_vec2_create(10, 10);
  far_sprite->layer = 1;

  /* 层级缩放：parent scale=2 无 sprite，child 尺寸 (10,10) → world AABB 20×20 @ (60,0) */
  lum_Entity parent = lum_ecs_create_entity(ecs);
  lum_Component_transform* parent_transform = lum_scene_add_transform(ecs, parent);
  parent_transform->scale = lum_vec2_create(2.0f, 2.0f);
  lum_Entity child = lum_ecs_create_entity(ecs);
  lum_Component_transform* child_transform = lum_scene_add_transform(ecs, child);
  child_transform->position = lum_vec2_create(30, 0);
  lum_Component_sprite* child_sprite = lum_scene_add_sprite(ecs, child, NULL);
  child_sprite->size = lum_vec2_create(10, 10);
  child_sprite->layer = 2;
  lum_scene_set_parent(ecs, child, parent);

  /* 无 sprite 实体：transform 就位但不可拾取 */
  lum_Entity bare = lum_ecs_create_entity(ecs);
  lum_Component_transform* bare_transform = lum_scene_add_transform(ecs, bare);
  bare_transform->position = lum_vec2_create(10, 10);

  lum_scene_update(ecs, 0.0f);   /* 拾取读 world 派生数据，先跑管线 */

  /* 重叠区 → 层最大者 */
  CHECK(lum_scene_pick(ecs, lum_vec2_create(5, 5)) == top);
  /* bottom 独占区（top 的 AABB 之外） */
  CHECK(lum_scene_pick(ecs, lum_vec2_create(-40, 0)) == bottom);
  /* far 独立命中 */
  CHECK(lum_scene_pick(ecs, lum_vec2_create(500, 0)) == far);
  /* 空白 → 无命中 */
  CHECK(lum_scene_pick(ecs, lum_vec2_create(200, 200)) == Lum_Entity_Null);
  /* 层级缩放生效：child world AABB = (60,0) ± 10 */
  CHECK(lum_scene_pick(ecs, lum_vec2_create(68, 0)) == child);
  /* 缩放没有虚判：child AABB 外、bottom 独占区（避开 top 的 AABB）→ bottom */
  CHECK(lum_scene_pick(ecs, lum_vec2_create(40, -40)) == bottom);
  /* bare 与 top 同点：bare 无 sprite 不参与 → top */
  CHECK(lum_scene_pick(ecs, lum_vec2_create(10, 10)) == top);

  /* 销毁后再拾取：top 消失 → 该点被 bare 所在位置的 bottom 覆盖（(10,10) 在 bottom 内） */
  lum_scene_destroy_entity(ecs, top);
  lum_scene_update(ecs, 0.0f);
  CHECK(lum_scene_pick(ecs, lum_vec2_create(5, 5)) == bottom);

  lum_ecs_destroy(ecs);
}

int main(void) {
  test_pick();
  printf("scene tests: %d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}

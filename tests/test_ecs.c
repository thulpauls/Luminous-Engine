/* lum_ecs 核心自测：纯逻辑，无 GL 依赖。
   编译：gcc -Isrc tests/test_ecs.c src/lum_ecs.c src/lum_log.c -o test_ecs */
#include "lum_ecs.h"
#include <stdio.h>
#include <string.h>

static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { \
  if (cond) { ++g_pass; } \
  else { ++g_fail; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

typedef struct { float x, y; } T_pos;
typedef struct { int hp; }       T_health;
static lum_Component_type T_pos_id, T_health_id;

static void test_basic(void) {
  lum_Ecs* ecs = lum_ecs_create();
  CHECK(ecs != NULL);

  T_pos_id     = lum_ecs_register_component(ecs, "pos", sizeof(T_pos));
  T_health_id  = lum_ecs_register_component(ecs, "health", sizeof(T_health));
  CHECK(T_pos_id != T_health_id);
  CHECK(lum_ecs_component_type_count(ecs) == 2);

  lum_Entity e = lum_ecs_create_entity(ecs);
  CHECK(e != Lum_Entity_Null);
  CHECK(lum_ecs_alive(ecs, e));

  /* add：返回清零内存；重复 add 返回同一份 */
  T_pos* p = (T_pos*)lum_ecs_add(ecs, e, T_pos_id);
  CHECK(p && p->x == 0.0f && p->y == 0.0f);
  p->x = 3.0f; p->y = 4.0f;
  CHECK(lum_ecs_add(ecs, e, T_pos_id) == p);          /* 同一份，未被清零 */
  CHECK(((T_pos*)lum_ecs_get(ecs, e, T_pos_id))->x == 3.0f);
  CHECK(lum_ecs_has(ecs, e, T_pos_id));
  CHECK(!lum_ecs_has(ecs, e, T_health_id));
  CHECK(lum_ecs_get(ecs, e, T_health_id) == NULL);

  /* remove：自己没了，别家没事 */
  lum_ecs_add(ecs, e, T_health_id);
  lum_ecs_remove(ecs, e, T_health_id);
  CHECK(!lum_ecs_has(ecs, e, T_health_id));
  CHECK(lum_ecs_has(ecs, e, T_pos_id));

  lum_ecs_destroy(ecs);
}

static void test_generation(void) {
  lum_Ecs* ecs = lum_ecs_create();
  T_pos_id = lum_ecs_register_component(ecs, "pos", sizeof(T_pos));

  lum_Entity a = lum_ecs_create_entity(ecs);
  lum_ecs_add(ecs, a, T_pos_id);
  lum_ecs_destroy_entity(ecs, a);
  CHECK(!lum_ecs_alive(ecs, a));                      /* 墓碑即刻判死 */

  lum_Entity b = lum_ecs_create_entity(ecs);          /* 复用 a 的槽 */
  CHECK(lum_ecs_alive(ecs, b));
  CHECK(!lum_ecs_alive(ecs, a));                      /* 旧句柄仍死（代数不同） */
  CHECK(lum_ecs_get(ecs, a, T_pos_id) == NULL);       /* 旧句柄取不到 b 的组件 */

  /* flush 前：a 的组件还在池里但被视图跳过；flush 后彻底移除 */
  lum_ecs_flush(ecs);
  lum_Entity c = lum_ecs_create_entity(ecs);
  CHECK(lum_ecs_alive(ecs, c) && !lum_ecs_alive(ecs, a));

  lum_ecs_destroy(ecs);
}

static int count_view2(lum_Ecs* ecs) {
  lum_Ecs_view v = lum_ecs_view_create2(ecs, T_pos_id, T_health_id);
  T_pos* p; T_health* h; lum_Entity e; int n = 0;
  while ((e = lum_ecs_view_each2(&v, (void**)&p, (void**)&h)) != Lum_Entity_Null) {
    (void)e; (void)p; (void)h;
    ++n;
  }
  return n;
}

static void test_views(void) {
  lum_Ecs* ecs = lum_ecs_create();
  T_pos_id    = lum_ecs_register_component(ecs, "pos", sizeof(T_pos));
  T_health_id = lum_ecs_register_component(ecs, "health", sizeof(T_health));

  lum_Entity both[3], pos_only[2];
  for (int i = 0; i < 3; ++i) {
    both[i] = lum_ecs_create_entity(ecs);
    lum_ecs_add(ecs, both[i], T_pos_id);
    lum_ecs_add(ecs, both[i], T_health_id);
  }
  for (int i = 0; i < 2; ++i) {
    pos_only[i] = lum_ecs_create_entity(ecs);
    lum_ecs_add(ecs, pos_only[i], T_pos_id);
  }
  CHECK(count_view2(ecs) == 3);

  /* 迭代中销毁：安全；被销毁者若还没轮到，当轮即跳过（墓碑即隐身） */
  lum_Ecs_view v = lum_ecs_view_create2(ecs, T_pos_id, T_health_id);
  T_pos* p; T_health* h; lum_Entity e; int visited = 0;
  while ((e = lum_ecs_view_each2(&v, (void**)&p, (void**)&h)) != Lum_Entity_Null) {
    ++visited;
    if (visited == 1) lum_ecs_destroy_entity(ecs, both[1]);   /* 迭代中销毁别人 */
  }
  CHECK(visited == 2);   /* both[1] 在轮到之前被销毁 → 跳过 */
  CHECK(count_view2(ecs) == 2);

  /* 迭代中创建：安全，本视图不访问新实体 */
  lum_Entity fresh = lum_ecs_create_entity(ecs);
  lum_ecs_add(ecs, fresh, T_pos_id);
  lum_ecs_add(ecs, fresh, T_health_id);
  CHECK(count_view2(ecs) == 3);

  /* flush 压缩后视图仍正确 */
  lum_ecs_flush(ecs);
  CHECK(count_view2(ecs) == 3);
  CHECK(lum_ecs_has(ecs, both[0], T_pos_id));
  CHECK(!lum_ecs_has(ecs, both[1], T_pos_id));        /* 死者组件已拆 */

  lum_ecs_destroy(ecs);
}

static void test_stress(void) {
  lum_Ecs* ecs = lum_ecs_create();
  T_pos_id = lum_ecs_register_component(ecs, "pos", sizeof(T_pos));

  enum { N = 10000 };
  static lum_Entity ents[N];
  for (int i = 0; i < N; ++i) {
    ents[i] = lum_ecs_create_entity(ecs);
    T_pos* p = (T_pos*)lum_ecs_add(ecs, ents[i], T_pos_id);
    p->x = (float)i;
  }

  lum_Ecs_view v = lum_ecs_view_create(ecs, T_pos_id);
  T_pos* p; lum_Entity e; int n = 0; double sum = 0.0;
  while ((e = lum_ecs_view_each(&v, (void**)&p)) != Lum_Entity_Null) {
    (void)e; ++n; sum += (double)p->x;
  }
  CHECK(n == N);
  CHECK(sum == (double)N * (double)(N - 1) / 2.0);

  /* 删一半、flush、再验证 */
  for (int i = 0; i < N; i += 2) lum_ecs_destroy_entity(ecs, ents[i]);
  lum_ecs_flush(ecs);
  v = lum_ecs_view_create(ecs, T_pos_id);
  n = 0;
  while ((e = lum_ecs_view_each(&v, (void**)&p)) != Lum_Entity_Null) { (void)p; ++n; }
  CHECK(n == N / 2);

  lum_ecs_destroy(ecs);
}

static void test_name_type(void) {
  lum_Ecs* ecs = lum_ecs_create();
  T_pos_id    = lum_ecs_register_component(ecs, "pos", sizeof(T_pos));
  T_health_id = lum_ecs_register_component(ecs, "health", sizeof(T_health));

  /* name → type：注册序即 id 序 */
  CHECK(lum_ecs_component_type_from_name(ecs, "pos") == T_pos_id);
  CHECK(lum_ecs_component_type_from_name(ecs, "health") == T_health_id);
  CHECK(lum_ecs_component_type_from_name(ecs, "no_such") == Lum_Component_Invalid);
  CHECK(lum_ecs_component_type_from_name(ecs, NULL) == Lum_Component_Invalid);
  CHECK(lum_ecs_component_type_from_name(NULL, "pos") == Lum_Component_Invalid);

  /* type → name：与注册名一致 */
  CHECK(lum_ecs_component_name(ecs, T_pos_id) && strcmp(lum_ecs_component_name(ecs, T_pos_id), "pos") == 0);
  CHECK(lum_ecs_component_name(ecs, T_health_id) && strcmp(lum_ecs_component_name(ecs, T_health_id), "health") == 0);
  CHECK(lum_ecs_component_name(ecs, Lum_Component_Invalid) == NULL);
  CHECK(lum_ecs_component_name(NULL, T_pos_id) == NULL);

  /* round-trip：name → type → name */
  lum_Component_type t = lum_ecs_component_type_from_name(ecs, "health");
  CHECK(t == lum_ecs_component_type_from_name(ecs, lum_ecs_component_name(ecs, t)));

  lum_ecs_destroy(ecs);
}

int main(void) {
  test_basic();
  test_generation();
  test_views();
  test_stress();
  test_name_type();
  printf("lum_ecs tests: %d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}

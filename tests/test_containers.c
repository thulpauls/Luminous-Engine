/* 容器自测：纯逻辑，无 GL 依赖。
   编译：gcc -Isrc tests/test_containers.c src/containers/lum_vector.c src/lum_log.c -o test_containers */
#include "containers/lum_vector.h"
#include "containers/lum_list.h"
#include <stdio.h>

static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { \
  if (cond) { ++g_pass; } \
  else { ++g_fail; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

typedef struct { int id; float v; } Item;

static void test_vector_basic(void) {
  lum_Vector v;
  CHECK(lum_vector_init(&v, sizeof(Item)));

  CHECK(lum_vector_empty(&v) && lum_vector_size(&v) == 0);

  for (int i = 0; i < 100; ++i) {
    Item it = { i, (float)i * 0.5f };
    CHECK(lum_vector_push(&v, &it) != NULL);
  }
  CHECK(lum_vector_size(&v) == 100);
  CHECK(lum_vector_at_t(&v, 42, Item)->id == 42);
  CHECK(lum_vector_at_t(&v, 42, Item)->v == 21.0f);

  /* emplace：清零槽原地填充 */
  Item* e = (Item*)lum_vector_emplace(&v);
  CHECK(e && e->id == 0 && e->v == 0.0f);
  e->id = 999;
  CHECK(lum_vector_size(&v) == 101);

  /* pop */
  lum_vector_pop(&v);
  CHECK(lum_vector_size(&v) == 100);
  CHECK(lum_vector_at_t(&v, 99, Item)->id == 99);

  /* 保序删中间 */
  lum_vector_remove(&v, 0);
  CHECK(lum_vector_at_t(&v, 0, Item)->id == 1);
  CHECK(lum_vector_size(&v) == 99);

  /* swap 删中间：尾元素补位（顺序变化，值集合不变） */
  lum_vector_remove_swap(&v, 0);
  CHECK(lum_vector_size(&v) == 98);
  CHECK(lum_vector_at_t(&v, 0, Item)->id == 99);   /* 原尾元素补到 0 位 */

  /* clear 保留容量 */
  uint32_t cap_before = v.capacity;
  lum_vector_clear(&v);
  CHECK(lum_vector_empty(&v) && v.capacity == cap_before);
  CHECK(lum_vector_push(&v, &(Item){ 7, 0 }) != NULL);
  CHECK(lum_vector_at_t(&v, 0, Item)->id == 7);

  /* reserve 之后 push 不再扩容 */
  CHECK(lum_vector_reserve(&v, 1000));
  uint32_t cap_after = v.capacity;
  for (int i = 0; i < 900; ++i) lum_vector_push(&v, &(Item){ i, 0 });
  CHECK(v.capacity == cap_after && lum_vector_size(&v) == 901);

  lum_vector_destroy(&v);
  CHECK(v.data == NULL && v.size == 0);
}

static void test_vector_shrink(void) {
  lum_Vector v;
  CHECK(lum_vector_init(&v, sizeof(Item)));

  /* 空 vector shrink：释放整个缓冲 */
  CHECK(lum_vector_shrink(&v));
  CHECK(v.data == NULL && v.capacity == 0);

  /* push 100（容量倍增 >100）→ shrink 到 100，内容不变 */
  for (int i = 0; i < 100; ++i) lum_vector_push(&v, &(Item){ i, 0 });
  CHECK(v.capacity > 100);
  CHECK(lum_vector_shrink(&v));
  CHECK(v.capacity == 100 && lum_vector_size(&v) == 100);
  CHECK(lum_vector_at_t(&v, 0, Item)->id == 0);
  CHECK(lum_vector_at_t(&v, 99, Item)->id == 99);

  /* shrink 后继续可用（重新倍增） */
  CHECK(lum_vector_push(&v, &(Item){ 100, 0 }) != NULL);
  CHECK(lum_vector_at_t(&v, 100, Item)->id == 100);

  /* clear → shrink：缓冲彻底释放，复用如初 */
  lum_vector_clear(&v);
  CHECK(lum_vector_shrink(&v));
  CHECK(v.data == NULL && v.capacity == 0 && lum_vector_size(&v) == 0);
  CHECK(lum_vector_push(&v, &(Item){ 7, 0 }) != NULL);
  CHECK(lum_vector_at_t(&v, 0, Item)->id == 7);

  lum_vector_destroy(&v);
}

static void test_vector_stress(void) {
  lum_Vector v;
  CHECK(lum_vector_init(&v, sizeof(double)));
  enum { N = 20000 };
  for (int i = 0; i < N; ++i) {
    double x = (double)i;
    lum_vector_push(&v, &x);
  }
  double sum = 0;
  for (uint32_t i = 0; i < lum_vector_size(&v); ++i) {
    sum += *(const double*)lum_vector_get_const(&v, i);
  }
  CHECK(sum == (double)N * (double)(N - 1) / 2.0);
  lum_vector_destroy(&v);
}

typedef struct {
  int id;
  lum_List_node node;
} Obj;

static void test_list_basic(void) {
  lum_List list;
  lum_list_init(&list);
  CHECK(lum_list_empty(&list) && lum_list_size(&list) == 0);
  CHECK(lum_list_front(&list) == NULL && lum_list_pop_front(&list) == NULL);

  Obj a = { 1, {0,0} }, b = { 2, {0,0} }, c = { 3, {0,0} };
  lum_list_push_back(&list, &a.node);
  lum_list_push_back(&list, &b.node);
  lum_list_push_back(&list, &c.node);
  CHECK(lum_list_size(&list) == 3);
  CHECK(lum_list_entry(lum_list_front(&list), Obj, node)->id == 1);
  CHECK(lum_list_entry(lum_list_back(&list), Obj, node)->id == 3);

  /* 头插 */
  Obj d = { 0, {0,0} };
  lum_list_push_front(&list, &d.node);
  CHECK(lum_list_entry(lum_list_front(&list), Obj, node)->id == 0);

  /* 顺序迭代 */
  lum_List_node* n;
  int ids[4]; int i = 0;
  lum_list_each(&list, n) { ids[i++] = lum_list_entry(n, Obj, node)->id; }
  CHECK(i == 4);
  CHECK(ids[0] == 0 && ids[1] == 1 && ids[2] == 2 && ids[3] == 3);

  /* O(1) 删中间，链自动接上 */
  lum_list_remove(&list, &b.node);
  CHECK(lum_list_size(&list) == 3);
  i = 0;
  lum_list_each(&list, n) { ids[i++] = lum_list_entry(n, Obj, node)->id; }
  CHECK(i == 3 && ids[0] == 0 && ids[1] == 1 && ids[2] == 3);

  /* pop 两端 */
  CHECK(lum_list_entry(lum_list_pop_front(&list), Obj, node)->id == 0);
  CHECK(lum_list_entry(lum_list_pop_back(&list), Obj, node)->id == 3);
  CHECK(lum_list_size(&list) == 1);
}

static void test_list_safe_remove(void) {
  lum_List list;
  lum_list_init(&list);

  enum { N = 8 };
  Obj objs[N];
  for (int i = 0; i < N; ++i) {
    objs[i].id = i;
    lum_list_push_back(&list, &objs[i].node);
  }

  /* safe 迭代中删偶数项 */
  lum_List_node *n, *tmp;
  int visited = 0;
  lum_list_each_safe(&list, n, tmp) {
    ++visited;
    Obj* o = lum_list_entry(n, Obj, node);
    if (o->id % 2 == 0) lum_list_remove(&list, n);
  }
  CHECK(visited == N);
  CHECK(lum_list_size(&list) == N / 2);

  int first_id = lum_list_entry(lum_list_front(&list), Obj, node)->id;
  CHECK(first_id == 1);

  /* 删到空 */
  lum_list_each_safe(&list, n, tmp) { lum_list_remove(&list, n); }
  CHECK(lum_list_empty(&list) && lum_list_size(&list) == 0);
}

int main(void) {
  test_vector_basic();
  test_vector_shrink();
  test_vector_stress();
  test_list_basic();
  test_list_safe_remove();
  printf("containers tests: %d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}

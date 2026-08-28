#include "lum_ecs.h"
#include "containers/lum_vector.h"
#include "lum_assert.h"
#include "lum_log.h"
#include <stdlib.h>
#include <string.h>

#define Lum_E_Index_Bits 20
#define Lum_E_Index_Mask ((1u << Lum_E_Index_Bits) - 1u)
#define Lum_E_Gen_Max    ((1u << 12) - 1u)
#define Lum_Pool_Sparse_None UINT32_MAX

typedef struct lum_Entity_slot {
  uint32_t generation;   /* 回收一次 +1；句柄 gen 与之相等且 alive 才有效 */
  bool     occupied;
  bool     alive;
} lum_Entity_slot;

struct lum_Pool {
  size_t     size;        /* 单组件字节数 */
  lum_Vector components;  /* dense 组件数组 */
  lum_Vector entities;    /* dense 实体 id 数组（含代数） */
  lum_Vector sparse;      /* 实体下标 → dense 位置；长度 = 实体槽总数 */
};
typedef struct lum_Pool lum_Pool;

struct lum_Ecs {
  lum_Vector slots;
  lum_Vector free_stack;
  lum_Vector pools;
  lum_Vector pool_names;
  lum_Vector serializers;   /* lum_Component_serializer，按类型 id 惰性对齐 */
  lum_Vector field_tables;  /* lum_Field_table_slot，按类型 id 惰性对齐 */
  uint32_t   alive_count;
};

typedef struct lum_Component_serializer {
  lum_Component_write_fn write;
  lum_Component_read_fn read;
} lum_Component_serializer;

typedef struct lum_Field_table_slot {
  const void* fields;       /* 不透明：内容与解释权在 serial 层 */
  uint32_t count;
} lum_Field_table_slot;

static uint32_t lum__entity_index(lum_Entity e) { return e & Lum_E_Index_Mask; }
static uint32_t lum__entity_gen(lum_Entity e)   { return e >> Lum_E_Index_Bits; }
static lum_Entity lum__entity_make(uint32_t index, uint32_t gen) { return index | (gen << Lum_E_Index_Bits); }

static uint32_t* lum__pool_entities(const lum_Pool* p) { return (uint32_t*)p->entities.data; }
static uint32_t* lum__pool_sparse(const lum_Pool* p)   { return (uint32_t*)p->sparse.data; }
static unsigned char* lum__pool_components(const lum_Pool* p) { return (unsigned char*)p->components.data; }

lum_Ecs* lum_ecs_create(void) {
  lum_Ecs* ecs = (lum_Ecs*)calloc(1, sizeof(*ecs));
  if (!ecs) return NULL;

  if (!lum_vector_init(&ecs->slots, sizeof(lum_Entity_slot)) ||
      !lum_vector_init(&ecs->free_stack, sizeof(uint32_t)) ||
      !lum_vector_init(&ecs->pools, sizeof(lum_Pool)) ||
      !lum_vector_init(&ecs->pool_names, sizeof(const char*)) ||
      !lum_vector_init(&ecs->serializers, sizeof(lum_Component_serializer)) ||
      !lum_vector_init(&ecs->field_tables, sizeof(lum_Field_table_slot))) {
    lum_ecs_destroy(ecs);
    return NULL;
  }
  return ecs;
}

void lum_ecs_destroy(lum_Ecs* ecs) {
  if (!ecs) return;
  for (uint32_t p = 0; p < lum_vector_size(&ecs->pools); ++p) {
    lum_Pool* pool = (lum_Pool*)lum_vector_get(&ecs->pools, p);
    lum_vector_destroy(&pool->components);
    lum_vector_destroy(&pool->entities);
    lum_vector_destroy(&pool->sparse);
  }
  lum_vector_destroy(&ecs->pools);
  lum_vector_destroy(&ecs->pool_names);
  lum_vector_destroy(&ecs->serializers);
  lum_vector_destroy(&ecs->field_tables);
  lum_vector_destroy(&ecs->slots);
  lum_vector_destroy(&ecs->free_stack);
  free(ecs);
}

/* 追加一个实体槽；所有池的 sparse 同步补一位 NONE */
static bool lum__ecs_append_slot(lum_Ecs* ecs) {
  if (lum_vector_size(&ecs->slots) >= Lum_Ecs_Max_Entities) {
    Lum_Log_Error("ecs: entity limit reached (%u).", Lum_Ecs_Max_Entities);
    return false;
  }
  lum_Entity_slot* entity_slot = (lum_Entity_slot*)lum_vector_emplace(&ecs->slots);
  if (!entity_slot) { Lum_Log_Error("ecs: entity slot alloc failed."); return false; }
  entity_slot->generation = 1;

  for (uint32_t p = 0; p < lum_vector_size(&ecs->pools); ++p) {
    lum_Pool* pool = (lum_Pool*)lum_vector_get(&ecs->pools, p);
    uint32_t none = Lum_Pool_Sparse_None;
    if (!lum_vector_push(&pool->sparse, &none)) {
      Lum_Log_Error("ecs: sparse alloc failed.");
      return false;
    }
  }
  return true;
}

lum_Entity lum_ecs_create_entity(lum_Ecs* ecs) {
  Lum_Assert(ecs);

  uint32_t slot;
  if (!lum_vector_empty(&ecs->free_stack)) {
    slot = *(uint32_t*)lum_vector_get(&ecs->free_stack, lum_vector_size(&ecs->free_stack) - 1u);
    lum_vector_pop(&ecs->free_stack);
  } else {
    if (!lum__ecs_append_slot(ecs)) return Lum_Entity_Null;
    slot = lum_vector_size(&ecs->slots) - 1u;
  }

  lum_Entity_slot* entity_slot = (lum_Entity_slot*)lum_vector_get(&ecs->slots, slot);
  entity_slot->occupied = true;
  entity_slot->alive = true;
  ecs->alive_count++;
  return lum__entity_make(slot, entity_slot->generation);
}

bool lum_ecs_alive(const lum_Ecs* ecs, lum_Entity e) {
  if (!ecs || e == Lum_Entity_Null) return false;
  uint32_t index = lum__entity_index(e);
  if (index >= lum_vector_size(&ecs->slots)) return false;
  const lum_Entity_slot* entity_slot = (const lum_Entity_slot*)lum_vector_get_const(&ecs->slots, index);
  return entity_slot->occupied && entity_slot->alive && entity_slot->generation == lum__entity_gen(e);
}

void lum_ecs_destroy_entity(lum_Ecs* ecs, lum_Entity e) {
  if (!lum_ecs_alive(ecs, e)) return;
  lum_Entity_slot* entity_slot = (lum_Entity_slot*)lum_vector_get(&ecs->slots, lum__entity_index(e));
  entity_slot->alive = false;
  ecs->alive_count--;
}

uint32_t lum_ecs_entity_slot_count(const lum_Ecs* ecs) {
  return ecs ? lum_vector_size((lum_Vector*)&ecs->slots) : 0u;
}

lum_Entity lum_ecs_entity_at_slot(const lum_Ecs* ecs, uint32_t slot_index) {
  if (!ecs || slot_index >= lum_vector_size((lum_Vector*)&ecs->slots)) return Lum_Entity_Null;
  const lum_Entity_slot* entity_slot =
      (const lum_Entity_slot*)lum_vector_get_const((lum_Vector*)&ecs->slots, slot_index);
  if (!entity_slot->occupied || !entity_slot->alive) return Lum_Entity_Null;
  return lum__entity_make(slot_index, entity_slot->generation);
}

uint32_t lum_ecs_entity_slot_index(lum_Entity entity) {
  return lum__entity_index(entity);
}

void lum_ecs_flush(lum_Ecs* ecs) {
  if (!ecs) return;

  /* 1) 各池剔除死亡实体的组件（原地压缩） */
  for (uint32_t p = 0; p < lum_vector_size(&ecs->pools); ++p) {
    lum_Pool* pool = (lum_Pool*)lum_vector_get(&ecs->pools, p);
    uint32_t* entities = lum__pool_entities(pool);
    uint32_t* sparse = lum__pool_sparse(pool);
    unsigned char* components = lum__pool_components(pool);

    uint32_t w = 0;
    for (uint32_t i = 0; i < lum_vector_size(&pool->entities); ++i) {
      lum_Entity e = entities[i];
      if (!lum_ecs_alive(ecs, e)) continue;
      if (w != i) {
        entities[w] = e;
        memmove(components + (size_t)w * pool->size,
                components + (size_t)i * pool->size,
                pool->size);
        sparse[lum__entity_index(e)] = w;
      }
      ++w;
    }
    pool->entities.size = w;      /* 容器字段公开，压缩后直接截断 */
    pool->components.size = w;
  }

  /* 2) 回收实体槽：代数 +1 使残留旧句柄彻底失效，下标入空闲栈 */
  for (uint32_t i = 0; i < lum_vector_size(&ecs->slots); ++i) {
    lum_Entity_slot* entity_slot = (lum_Entity_slot*)lum_vector_get(&ecs->slots, i);
    if (entity_slot->occupied && !entity_slot->alive) {
      entity_slot->occupied = false;
      entity_slot->generation = (entity_slot->generation >= Lum_E_Gen_Max) ? 1u : entity_slot->generation + 1u;
      if (!lum_vector_push(&ecs->free_stack, &i)) break;
    }
  }
}

lum_Component_type lum_ecs_register_component(lum_Ecs* ecs, const char* name, size_t size) {
  if (!ecs || !name || size == 0) {
    Lum_Log_Error("ecs: invalid component registration (%s).", name ? name : "?");
    return Lum_Component_Invalid;
  }

  lum_Pool* pool = (lum_Pool*)lum_vector_emplace(&ecs->pools);
  const char** name_slot = (const char**)lum_vector_emplace(&ecs->pool_names);
  if (!pool || !name_slot) {
    Lum_Log_Error("ecs: component registration alloc failed (%s).", name);
    return Lum_Component_Invalid;
  }
  pool->size = size;   /* 单组件字节数：get/remove/flush 的偏移计算都依赖它 */
  if (!lum_vector_init(&pool->components, size) ||
      !lum_vector_init(&pool->entities, sizeof(uint32_t)) ||
      !lum_vector_init(&pool->sparse, sizeof(uint32_t))) {
    Lum_Log_Error("ecs: component registration alloc failed (%s).", name);
    return Lum_Component_Invalid;
  }

  /* sparse 补到与当前实体槽数一致 */
  while (lum_vector_size(&pool->sparse) < lum_vector_size(&ecs->slots)) {
    uint32_t none = Lum_Pool_Sparse_None;
    if (!lum_vector_push(&pool->sparse, &none)) {
      Lum_Log_Error("ecs: sparse alloc failed (%s).", name);
      return Lum_Component_Invalid;
    }
  }

  *name_slot = name;
  return lum_vector_size(&ecs->pools) - 1u;
}

uint32_t lum_ecs_component_type_count(const lum_Ecs* ecs) {
  return ecs ? lum_vector_size((lum_Vector*)&ecs->pools) : 0u;
}

lum_Component_type lum_ecs_component_type_from_name(const lum_Ecs* ecs, const char* name) {
  if (!ecs || !name) return Lum_Component_Invalid;
  for (uint32_t t = 0; t < lum_vector_size((lum_Vector*)&ecs->pool_names); ++t) {
    const char* candidate = *(const char**)lum_vector_get_const((lum_Vector*)&ecs->pool_names, t);
    if (candidate && strcmp(candidate, name) == 0) return t;
  }
  return Lum_Component_Invalid;
}

const char* lum_ecs_component_name(const lum_Ecs* ecs, lum_Component_type type) {
  if (!ecs || type >= lum_vector_size((lum_Vector*)&ecs->pool_names)) return NULL;
  return *(const char**)lum_vector_get_const((lum_Vector*)&ecs->pool_names, type);
}

void lum_ecs_set_component_serializer(lum_Ecs* ecs, lum_Component_type type,
                                      lum_Component_write_fn write, lum_Component_read_fn read) {
  if (!ecs || type >= lum_vector_size(&ecs->pools)) return;
  while (lum_vector_size(&ecs->serializers) <= type) {
    lum_Component_serializer* empty = (lum_Component_serializer*)lum_vector_emplace(&ecs->serializers);
    if (!empty) return;
    empty->write = NULL;
    empty->read = NULL;
  }
  lum_Component_serializer* serializer = (lum_Component_serializer*)lum_vector_get(&ecs->serializers, type);
  serializer->write = write;
  serializer->read = read;
}

bool lum_ecs_component_serializer(const lum_Ecs* ecs, lum_Component_type type,
                                  lum_Component_write_fn* out_write, lum_Component_read_fn* out_read) {
  if (out_write) *out_write = NULL;
  if (out_read) *out_read = NULL;
  if (!ecs || type >= lum_vector_size((lum_Vector*)&ecs->serializers)) return false;
  const lum_Component_serializer* serializer =
      (const lum_Component_serializer*)lum_vector_get_const((lum_Vector*)&ecs->serializers, type);
  if (!serializer->write || !serializer->read) return false;
  if (out_write) *out_write = serializer->write;
  if (out_read) *out_read = serializer->read;
  return true;
}

uint32_t lum_ecs_entity_component_types(const lum_Ecs* ecs, lum_Entity e,
                                        lum_Component_type* out_types, uint32_t cap) {
  if (!lum_ecs_alive(ecs, e)) return 0;
  uint32_t count = 0;
  uint32_t index = lum__entity_index(e);
  for (uint32_t t = 0; t < lum_vector_size((lum_Vector*)&ecs->pools); ++t) {
    const lum_Pool* pool = (const lum_Pool*)lum_vector_get_const((lum_Vector*)&ecs->pools, t);
    if (lum__pool_sparse(pool)[index] != Lum_Pool_Sparse_None) {
      if (out_types && count < cap) out_types[count] = t;
      count++;
    }
  }
  return count;
}

void lum_ecs_set_component_field_table(lum_Ecs* ecs, lum_Component_type type,
                                       const void* fields, uint32_t field_count) {
  if (!ecs || type >= lum_vector_size(&ecs->pools)) return;
  while (lum_vector_size(&ecs->field_tables) <= type) {
    lum_Field_table_slot* empty = (lum_Field_table_slot*)lum_vector_emplace(&ecs->field_tables);
    if (!empty) return;
    empty->fields = NULL;
    empty->count = 0;
  }
  lum_Field_table_slot* slot = (lum_Field_table_slot*)lum_vector_get(&ecs->field_tables, type);
  slot->fields = fields;
  slot->count = field_count;
}

bool lum_ecs_component_field_table(const lum_Ecs* ecs, lum_Component_type type,
                                   const void** out_fields, uint32_t* out_field_count) {
  if (out_fields) *out_fields = NULL;
  if (out_field_count) *out_field_count = 0;
  if (!ecs || type >= lum_vector_size((lum_Vector*)&ecs->field_tables)) return false;
  const lum_Field_table_slot* slot =
      (const lum_Field_table_slot*)lum_vector_get_const((lum_Vector*)&ecs->field_tables, type);
  if (!slot->fields) return false;
  if (out_fields) *out_fields = slot->fields;
  if (out_field_count) *out_field_count = slot->count;
  return true;
}

void* lum_ecs_add(lum_Ecs* ecs, lum_Entity e, lum_Component_type t) {
  if (!lum_ecs_alive(ecs, e)) return NULL;
  if (t >= lum_vector_size(&ecs->pools)) {
    Lum_Log_Error("ecs: add with unregistered component type %u.", t);
    return NULL;
  }

  lum_Pool* pool = (lum_Pool*)lum_vector_get(&ecs->pools, t);
  uint32_t index = lum__entity_index(e);
  if (lum__pool_sparse(pool)[index] != Lum_Pool_Sparse_None) { /* 已经存在对应 entity */
    return lum__pool_components(pool) + (size_t)lum__pool_sparse(pool)[index] * pool->size;
  }
  /* 不存在对应 entity */
  if (!lum_vector_reserve(&pool->components, lum_vector_size(&pool->components) + 1u) ||
      !lum_vector_reserve(&pool->entities, lum_vector_size(&pool->entities) + 1u)) {
    Lum_Log_Error("ecs: dense pool alloc failed (type %u).", t);
    return NULL;
  }

  void* slot = lum_vector_emplace(&pool->components);
  uint32_t id = e;
  lum_vector_push(&pool->entities, &id);
  lum__pool_sparse(pool)[index] = lum_vector_size(&pool->components) - 1u;
  return slot;
}

void* lum_ecs_get(const lum_Ecs* ecs, lum_Entity e, lum_Component_type t) {
  if (!lum_ecs_alive(ecs, e) || t >= lum_vector_size((lum_Vector*)&ecs->pools)) return NULL;
  const lum_Pool* pool = (const lum_Pool*)lum_vector_get_const((lum_Vector*)&ecs->pools, t);
  uint32_t pos = lum__pool_sparse(pool)[lum__entity_index(e)];
  if (pos == Lum_Pool_Sparse_None) return NULL;
  return lum__pool_components(pool) + (size_t)pos * pool->size;
}

bool lum_ecs_has(const lum_Ecs* ecs, lum_Entity e, lum_Component_type t) {
  return lum_ecs_get(ecs, e, t) != NULL;
}

void lum_ecs_remove(lum_Ecs* ecs, lum_Entity e, lum_Component_type t) {
  if (!lum_ecs_alive(ecs, e) || t >= lum_vector_size(&ecs->pools)) return;
  lum_Pool* pool = (lum_Pool*)lum_vector_get(&ecs->pools, t);
  uint32_t index = lum__entity_index(e);
  uint32_t pos = lum__pool_sparse(pool)[index];
  if (pos == Lum_Pool_Sparse_None) return;

  uint32_t last = lum_vector_size(&pool->entities) - 1u;
  if (pos != last) {
    lum__pool_entities(pool)[pos] = lum__pool_entities(pool)[last];
    memmove(lum__pool_components(pool) + (size_t)pos * pool->size,
            lum__pool_components(pool) + (size_t)last * pool->size,
            pool->size);
    lum__pool_sparse(pool)[lum__entity_index(lum__pool_entities(pool)[pos])] = pos;
  }
  pool->entities.size = last;
  pool->components.size = last;
  lum__pool_sparse(pool)[index] = Lum_Pool_Sparse_None;
}

/* ---------- 视图 ---------- */

static bool lum__ecs_view_init(lum_Ecs* ecs, lum_Ecs_view* v, int n, const lum_Component_type* types) {
  v->ecs = ecs;
  v->i = 0;
  v->n = 0;
  for (int k = 0; k < n; ++k) {
    if (types[k] >= lum_vector_size(&ecs->pools)) {
      Lum_Log_Error("ecs: view with unregistered component type %u.", types[k]);
      return false;
    }
  }
  for (int k = 0; k < n; ++k) {
    v->pools[k] = (const lum_Pool*)lum_vector_get_const(&ecs->pools, types[k]);
  }
  /* 按 dense 元素数升序，最小的做主迭代池 */
  for (int k = 1; k < n; ++k) {
    for (int j = k; j > 0 && v->pools[j - 1]->entities.size > v->pools[j]->entities.size; --j) {
      const lum_Pool* tmp = v->pools[j];
      v->pools[j] = v->pools[j - 1];
      v->pools[j - 1] = tmp;
    }
  }
  v->n = n;
  return true;
}

lum_Ecs_view lum_ecs_view_create(lum_Ecs* ecs, lum_Component_type a) {
  lum_Ecs_view v;
  lum__ecs_view_init(ecs, &v, 1, &a);
  return v;
}

lum_Ecs_view lum_ecs_view_create2(lum_Ecs* ecs, lum_Component_type a, lum_Component_type b) {
  lum_Ecs_view v;
  lum_Component_type types[2] = { a, b };
  lum__ecs_view_init(ecs, &v, 2, types);
  return v;
}

lum_Ecs_view lum_ecs_view_create3(lum_Ecs* ecs, lum_Component_type a, lum_Component_type b, lum_Component_type c) {
  lum_Ecs_view v;
  lum_Component_type types[3] = { a, b, c };
  lum__ecs_view_init(ecs, &v, 3, types);
  return v;
}

lum_Entity lum_ecs_view_each(lum_Ecs_view* v, void** out_a) {
  Lum_Assert(v && out_a);
  if (v->n < 1) return Lum_Entity_Null;
  const lum_Pool* p = v->pools[0];
  while (v->i < lum_vector_size((lum_Vector*)&p->entities)) {
    lum_Entity e = lum__pool_entities(p)[v->i];
    uint32_t pos = v->i++;
    if (!lum_ecs_alive(v->ecs, e)) continue;
    *out_a = lum__pool_components(p) + (size_t)pos * p->size;
    return e;
  }
  return Lum_Entity_Null;
}

lum_Entity lum_ecs_view_each2(lum_Ecs_view* v, void** out_a, void** out_b) {
  Lum_Assert(v && out_a && out_b);
  if (v->n < 2) return Lum_Entity_Null;
  const lum_Pool* p0 = v->pools[0];
  const lum_Pool* p1 = v->pools[1];
  while (v->i < lum_vector_size((lum_Vector*)&p0->entities)) {
    lum_Entity e = lum__pool_entities(p0)[v->i];
    uint32_t pos0 = v->i++;
    if (!lum_ecs_alive(v->ecs, e)) continue;
    uint32_t pos1 = lum__pool_sparse(p1)[lum__entity_index(e)];
    if (pos1 == Lum_Pool_Sparse_None) continue;
    *out_a = lum__pool_components(p0) + (size_t)pos0 * p0->size;
    *out_b = lum__pool_components(p1) + (size_t)pos1 * p1->size;
    return e;
  }
  return Lum_Entity_Null;
}

lum_Entity lum_ecs_view_each3(lum_Ecs_view* v, void** out_a, void** out_b, void** out_c) {
  Lum_Assert(v && out_a && out_b && out_c);
  if (v->n < 3) return Lum_Entity_Null;
  const lum_Pool* p0 = v->pools[0];
  const lum_Pool* p1 = v->pools[1];
  const lum_Pool* p2 = v->pools[2];
  while (v->i < lum_vector_size((lum_Vector*)&p0->entities)) {
    lum_Entity e = lum__pool_entities(p0)[v->i];
    uint32_t pos0 = v->i++;
    if (!lum_ecs_alive(v->ecs, e)) continue;
    uint32_t pos1 = lum__pool_sparse(p1)[lum__entity_index(e)];
    if (pos1 == Lum_Pool_Sparse_None) continue;
    uint32_t pos2 = lum__pool_sparse(p2)[lum__entity_index(e)];
    if (pos2 == Lum_Pool_Sparse_None) continue;
    *out_a = lum__pool_components(p0) + (size_t)pos0 * p0->size;
    *out_b = lum__pool_components(p1) + (size_t)pos1 * p1->size;
    *out_c = lum__pool_components(p2) + (size_t)pos2 * p2->size;
    return e;
  }
  return Lum_Entity_Null;
}

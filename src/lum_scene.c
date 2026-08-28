#include "lum_scene.h"
#include "lum_transform.h"
#include "lum_renderer2d.h"
#include "containers/lum_vector.h"
#include "lum_assert.h"
#include "lum_log.h"
#include <string.h>

typedef struct lum_Render_system_entry {
  lum_render_system_fn fn;
  void* user_data;
} lum_Render_system_entry;

typedef struct lum_Update_system_entry {
  lum_update_system_fn fn;
  void* user_data;
} lum_Update_system_entry;

/* 系统/渲染注册表：单例（引擎默认单世界）；lum_scene_init 时初始化/重置 */
static lum_Vector g_render_systems;
static lum_Vector g_update_systems;

int lum_scene_init(lum_Ecs* ecs) {
  if (!ecs) return 0;
  /* 注册顺序必须与 lum_scene.h 的 id 枚举一致 */
  if (lum_ecs_register_component(ecs, "transform",  sizeof(lum_Component_transform))  != lum_Component_transform_id ||
      lum_ecs_register_component(ecs, "hierarchy",  sizeof(lum_Component_hierarchy))  != lum_Component_hierarchy_id ||
      lum_ecs_register_component(ecs, "sprite",     sizeof(lum_Component_sprite))     != lum_Component_sprite_id ||
      lum_ecs_register_component(ecs, "script",     sizeof(lum_Component_script))     != lum_Component_script_id) {
    Lum_Log_Error("scene: builtin component registration failed.");
    return 0;
  }

  if (g_render_systems.elem_size == 0) {
    if (!lum_vector_init(&g_render_systems, sizeof(lum_Render_system_entry))) return 0;
  } else {
    lum_vector_clear(&g_render_systems);   /* 进程内重复 init 时重置注册表 */
  }
  if (g_update_systems.elem_size == 0) {
    if (!lum_vector_init(&g_update_systems, sizeof(lum_Update_system_entry))) return 0;
  } else {
    lum_vector_clear(&g_update_systems);
  }
  return 1;
}

int lum_scene_add_render_system(lum_Ecs* ecs, lum_render_system_fn fn, void* user_data) {
  if (!ecs || !fn || g_render_systems.elem_size == 0) return 0;
  lum_Render_system_entry* entry = (lum_Render_system_entry*)lum_vector_emplace(&g_render_systems);
  if (!entry) return 0;
  entry->fn = fn;
  entry->user_data = user_data;
  return 1;
}

int lum_scene_add_update_system(lum_Ecs* ecs, lum_update_system_fn fn, void* user_data) {
  if (!ecs || !fn || g_update_systems.elem_size == 0) return 0;
  lum_Update_system_entry* entry = (lum_Update_system_entry*)lum_vector_emplace(&g_update_systems);
  if (!entry) return 0;
  entry->fn = fn;
  entry->user_data = user_data;
  return 1;
}

lum_Component_transform* lum_scene_add_transform(lum_Ecs* ecs, lum_Entity e) {
  lum_Component_transform* t = (lum_Component_transform*)lum_ecs_add(ecs, e, lum_Component_transform_id);
  if (!t) return NULL;
  t->scale = lum_vec2_create(1.0f, 1.0f);   /* add 清零，scale 必须回填为 1 */
  t->world = lum_mat4_identity();
  return t;
}

lum_Component_sprite* lum_scene_add_sprite(lum_Ecs* ecs, lum_Entity e, const lum_Texture2d* texture) {
  lum_Component_sprite* sprite = (lum_Component_sprite*)lum_ecs_add(ecs, e, lum_Component_sprite_id);
  if (!sprite) return NULL;
  sprite->texture = texture;
  sprite->size = lum_vec2_create(1.0f, 1.0f);
  sprite->uv = lum_vec4_create(0.0f, 0.0f, 1.0f, 1.0f);
  sprite->color = lum_vec4_create(1.0f, 1.0f, 1.0f, 1.0f);
  return sprite;
}

lum_Component_script* lum_scene_add_script(lum_Ecs* ecs, lum_Entity e,
                                   lum_entity_start_fn start, lum_entity_update_fn update,
                                   void* user_data) {
  lum_Component_script* script = (lum_Component_script*)lum_ecs_add(ecs, e, lum_Component_script_id);
  if (!script) return NULL;
  script->start = start;
  script->update = update;
  script->user_data = user_data;
  return script;
}

/* ---------- 层级 ---------- */

/* 把 child 从其当前父节点的孩子链中摘除（不动 hierarchy 组件本身的去留） */
static void lum__scene_unlink_from_parent(lum_Ecs* ecs, lum_Component_hierarchy* h) {
  if (h->parent == Lum_Entity_Null) return;
  lum_Component_hierarchy* ph = (lum_Component_hierarchy*)lum_ecs_get(ecs, h->parent, lum_Component_hierarchy_id);
  if (!ph) { h->parent = Lum_Entity_Null; return; }

  if (h->prev_sibling != Lum_Entity_Null) {
    lum_Component_hierarchy* prev = (lum_Component_hierarchy*)lum_ecs_get(ecs, h->prev_sibling, lum_Component_hierarchy_id);
    if (prev) prev->next_sibling = h->next_sibling;
  } else {
    ph->first_child = h->next_sibling;   /* 自己是链头：父的 first_child 跳到下一个 */
  }
  if (h->next_sibling != Lum_Entity_Null) {
    lum_Component_hierarchy* next = (lum_Component_hierarchy*)lum_ecs_get(ecs, h->next_sibling, lum_Component_hierarchy_id);
    if (next) next->prev_sibling = h->prev_sibling;
  }
  if (ph->child_count > 0) ph->child_count--;

  h->parent = Lum_Entity_Null;
  h->prev_sibling = Lum_Entity_Null;
  h->next_sibling = Lum_Entity_Null;
}

void lum_scene_set_parent(lum_Ecs* ecs, lum_Entity child, lum_Entity parent) {
  if (!lum_ecs_alive(ecs, child)) return;
  if (parent != Lum_Entity_Null && !lum_ecs_alive(ecs, parent)) return;
  if (parent == child) return;

  /* 环检查：parent 的祖先链上不能出现 child */
  if (parent != Lum_Entity_Null) {
    lum_Entity a = parent;
    while (a != Lum_Entity_Null) {
      if (a == child) {
        Lum_Log_Warn("scene: set_parent rejected, would create a cycle.");
        return;
      }
      const lum_Component_hierarchy* ah = (const lum_Component_hierarchy*)lum_ecs_get(ecs, a, lum_Component_hierarchy_id);
      a = ah ? ah->parent : Lum_Entity_Null;
    }
  }

  lum_Component_hierarchy* h = (lum_Component_hierarchy*)lum_ecs_get(ecs, child, lum_Component_hierarchy_id);
  if (h) lum__scene_unlink_from_parent(ecs, h);

  if (parent == Lum_Entity_Null) {
    if (h) lum_ecs_remove(ecs, child, lum_Component_hierarchy_id);   /* 变根：不再需要 hierarchy */
    return;
  }

  if (!h) {
    h = (lum_Component_hierarchy*)lum_ecs_add(ecs, child, lum_Component_hierarchy_id);
    if (!h) return;
  }
  lum_Component_hierarchy* ph = (lum_Component_hierarchy*)lum_ecs_add(ecs, parent, lum_Component_hierarchy_id);
  if (!ph) return;

  /* 头插：新孩子成为 first_child */
  if (ph->first_child != Lum_Entity_Null) {
    lum_Component_hierarchy* first = (lum_Component_hierarchy*)lum_ecs_get(ecs, ph->first_child, lum_Component_hierarchy_id);
    if (first) first->prev_sibling = child;
  }
  h->next_sibling = ph->first_child;
  h->prev_sibling = Lum_Entity_Null;
  h->parent = parent;
  ph->first_child = child;
  ph->child_count++;
}

lum_Entity lum_scene_parent(const lum_Ecs* ecs, lum_Entity e) {
  const lum_Component_hierarchy* h = (const lum_Component_hierarchy*)lum_ecs_get(ecs, e, lum_Component_hierarchy_id);
  return h ? h->parent : Lum_Entity_Null;
}

lum_Entity lum_scene_first_child(const lum_Ecs* ecs, lum_Entity e) {
  const lum_Component_hierarchy* h = (const lum_Component_hierarchy*)lum_ecs_get(ecs, e, lum_Component_hierarchy_id);
  return h ? h->first_child : Lum_Entity_Null;
}

lum_Entity lum_scene_next_sibling(const lum_Ecs* ecs, lum_Entity e) {
  const lum_Component_hierarchy* h = (const lum_Component_hierarchy*)lum_ecs_get(ecs, e, lum_Component_hierarchy_id);
  return h ? h->next_sibling : Lum_Entity_Null;
}

uint32_t lum_scene_child_count(const lum_Ecs* ecs, lum_Entity e) {
  const lum_Component_hierarchy* h = (const lum_Component_hierarchy*)lum_ecs_get(ecs, e, lum_Component_hierarchy_id);
  return h ? h->child_count : 0u;
}

void lum_scene_destroy_entity(lum_Ecs* ecs, lum_Entity e) {
  if (!lum_ecs_alive(ecs, e)) return;

  const lum_Component_hierarchy* h = (const lum_Component_hierarchy*)lum_ecs_get(ecs, e, lum_Component_hierarchy_id);
  lum_Entity next_child = h ? h->first_child : Lum_Entity_Null;

  lum_scene_set_parent(ecs, e, Lum_Entity_Null);   /* 解链 + 移除自己的 hierarchy */
  lum_ecs_destroy_entity(ecs, e);                  /* 墓碑 */

  while (next_child != Lum_Entity_Null) {
    const lum_Component_hierarchy* child_hierarchy = (const lum_Component_hierarchy*)lum_ecs_get(ecs, next_child, lum_Component_hierarchy_id);
    lum_Entity next = child_hierarchy ? child_hierarchy->next_sibling : Lum_Entity_Null;
    lum_scene_destroy_entity(ecs, next_child);
    next_child = next;
  }
}

/* ---------- 系统与管线 ---------- */

static void lum__scene_compute_world(lum_Ecs* ecs, lum_Entity e, lum_Mat4 parent_world) {
  lum_Component_transform* t = (lum_Component_transform*)lum_ecs_get(ecs, e, lum_Component_transform_id);
  lum_Mat4 world = parent_world;
  if (t) {
    lum_Transform2d local = lum_transform2d_create(t->position, lum_vec2_create(1.0f, 1.0f),
                                                   t->scale, t->rotation, t->origin);
    t->world = lum_mat4_mul(parent_world, lum_transform2d_get_matrix(&local));
    world = t->world;
  }

  const lum_Component_hierarchy* h = (const lum_Component_hierarchy*)lum_ecs_get(ecs, e, lum_Component_hierarchy_id);
  if (!h) return;
  for (lum_Entity child = h->first_child; child != Lum_Entity_Null; ) {
    const lum_Component_hierarchy* child_hierarchy = (const lum_Component_hierarchy*)lum_ecs_get(ecs, child, lum_Component_hierarchy_id);
    lum_Entity next = child_hierarchy ? child_hierarchy->next_sibling : Lum_Entity_Null;
    if (lum_ecs_alive(ecs, child)) lum__scene_compute_world(ecs, child, world);
    child = next;
  }
}

static void lum__sys_script(lum_Ecs* ecs, float dt) {
  /* 先跑完所有 start，再跑 update——新实体的初始化先于任何 update */
  lum_Ecs_view v = lum_ecs_view_create(ecs, lum_Component_script_id);
  lum_Component_script* script; lum_Entity e;
  while ((e = lum_ecs_view_each(&v, (void**)&script)) != Lum_Entity_Null) {
    if (!script->started) {
      script->started = true;
      if (script->start) script->start(ecs, e);
    }
  }
  v = lum_ecs_view_create(ecs, lum_Component_script_id);
  while ((e = lum_ecs_view_each(&v, (void**)&script)) != Lum_Entity_Null) {
    if (script->update) script->update(ecs, e, dt);
  }
}

static void lum__sys_transform(lum_Ecs* ecs) {
  lum_Ecs_view v = lum_ecs_view_create(ecs, lum_Component_transform_id);
  lum_Component_transform* t; lum_Entity e;
  while ((e = lum_ecs_view_each(&v, (void**)&t)) != Lum_Entity_Null) {
    const lum_Component_hierarchy* h = (const lum_Component_hierarchy*)lum_ecs_get(ecs, e, lum_Component_hierarchy_id);
    if (h && h->parent != Lum_Entity_Null && lum_ecs_alive(ecs, h->parent)) {
      continue;   /* 非根：随父级递归处理 */
    }
    lum__scene_compute_world(ecs, e, lum_mat4_identity());
  }
}

void lum_scene_update(lum_Ecs* ecs, float dt) {
  if (!ecs) return;
  lum__sys_script(ecs, dt);
  for (uint32_t i = 0; i < lum_vector_size(&g_update_systems); ++i) {
    const lum_Update_system_entry* entry =
        (const lum_Update_system_entry*)lum_vector_get_const(&g_update_systems, i);
    entry->fn(ecs, dt, entry->user_data);
  }
  lum__sys_transform(ecs);
  lum_ecs_flush(ecs);
}

/* 内置第一环：sprite 系统（不可移除，用户无需注册） */
static void lum__render_system_sprites(lum_Ecs* ecs) {
  lum_renderer2d_set_blend_mode(Lum_Blend_Alpha);
  lum_Ecs_view v = lum_ecs_view_create2(ecs, lum_Component_transform_id, lum_Component_sprite_id);
  lum_Component_transform* t; lum_Component_sprite* sp; lum_Entity e;
  while ((e = lum_ecs_view_each2(&v, (void**)&t, (void**)&sp)) != Lum_Entity_Null) {
    if (!sp->texture) continue;

    lum_Vec4 uv = sp->uv;
    if (sp->flip_x) { float tmp = uv.x; uv.x = uv.z; uv.z = tmp; }
    if (sp->flip_y) { float tmp = uv.y; uv.y = uv.w; uv.w = tmp; }

    /* size 不进层级矩阵：在 world 之后再乘自身尺寸缩放，父级缩放不吞掉它 */
    lum_Transform2d st = lum_transform2d_create(lum_vec2_0(), sp->size, lum_vec2_1(), 0.0f, lum_vec2_0());
    lum_Mat4 model = lum_mat4_mul(t->world, lum_transform2d_get_matrix(&st));
    lum_renderer2d_draw_sprite_matrix_uv(sp->texture, model, uv, sp->layer, sp->color);
  }
}

void lum_scene_render(lum_Ecs* ecs) {
  if (!ecs) return;

  lum__render_system_sprites(ecs);
  for (uint32_t i = 0; i < lum_vector_size(&g_render_systems); ++i) {
    const lum_Render_system_entry* entry =
        (const lum_Render_system_entry*)lum_vector_get_const(&g_render_systems, i);
    entry->fn(ecs, entry->user_data);
  }
}

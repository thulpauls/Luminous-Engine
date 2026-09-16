#include "lum_scene.h"
#include "lum_application.h"   /* lum_Context / lum_Stage / lum_app_add_system */
#include "lum_transform.h"
#include "lum_renderer2d.h"
#include "lum_render_target.h"
#include "containers/lum_vector.h"
#include "lum_assert.h"
#include "lum_log.h"
#include <math.h>
#include <string.h>

typedef struct lum_Script_entry {
  const char* name;
  lum_entity_start_fn start;
  lum_entity_update_fn update;
} lum_Script_entry;

/* 视图与脚本注册表：单例（引擎默认单世界）；lum_scene_init 时初始化/重置。
   视图表按 priority 有序，执行时纯遍历（注册低频、执行每帧）。 */
static lum_Vector g_views;        /* lum_View */
static lum_Vector g_scripts;

/* 引擎内置系统（前向声明） */
static void lum__sys_script    (lum_Ecs* ecs, lum_Context* ctx, void* user_data);
static void lum__sys_transform (lum_Ecs* ecs, lum_Context* ctx, void* user_data);
static void lum__sys_commit    (lum_Ecs* ecs, lum_Context* ctx, void* user_data);
static void lum__sys_view_begin(lum_Ecs* ecs, lum_Context* ctx, void* user_data);
static void lum__sys_sprite    (lum_Ecs* ecs, lum_Context* ctx, void* user_data);

static int lum__views_insert(lum_View view) {
  uint32_t n = lum_vector_size(&g_views);
  uint32_t pos = n;
  for (uint32_t i = 0; i < n; ++i) {
    const lum_View* it = (const lum_View*)lum_vector_get_const(&g_views, i);
    if (it->priority > view.priority) { pos = i; break; }
  }
  if (!lum_vector_reserve(&g_views, n + 1u)) return 0;

  unsigned char* base = g_views.data;
  if (pos < n) {
    memmove(base + (size_t)(pos + 1u) * sizeof(lum_View),
            base + (size_t)pos * sizeof(lum_View),
            (size_t)(n - pos) * sizeof(lum_View));
  }
  *(lum_View*)(base + (size_t)pos * sizeof(lum_View)) = view;
  g_views.size = n + 1u;
  return 1;
}

int lum_scene_init(lum_Ecs* ecs) {
  if (!ecs) return 0;

  /* 同一 world 重复 init：组件已注册过就跳过注册（重复注册会新增池并让 id 漂移） */
  if (lum_ecs_component_type_from_name(ecs, "transform") != lum_Component_transform_id) {
    /* 注册顺序必须与 lum_scene.h 的 id 枚举一致 */
    if (lum_ecs_register_component(ecs, "transform",  sizeof(lum_Component_transform))  != lum_Component_transform_id ||
        lum_ecs_register_component(ecs, "hierarchy",  sizeof(lum_Component_hierarchy))  != lum_Component_hierarchy_id ||
        lum_ecs_register_component(ecs, "sprite",     sizeof(lum_Component_sprite))     != lum_Component_sprite_id ||
        lum_ecs_register_component(ecs, "script",     sizeof(lum_Component_script))     != lum_Component_script_id ||
        lum_ecs_register_component(ecs, "name",       sizeof(lum_Component_name))       != lum_Component_name_id) {
      Lum_Log_Error("scene: builtin component registration failed.");
      return 0;
    }
  }

  if (g_views.elem_size == 0) {
    if (!lum_vector_init(&g_views, sizeof(lum_View))) return 0;
  } else {
    lum_vector_clear(&g_views);
  }
  if (g_scripts.elem_size == 0) {
    if (!lum_vector_init(&g_scripts, sizeof(lum_Script_entry))) return 0;
  } else {
    lum_vector_clear(&g_scripts);
  }
  return 1;
}

/* 把场景层的引擎系统挂进阶段状态机。由 lum_application 的流程初始化调用一次 */
void lum_scene_register_engine_systems(void) {
  lum_flow_add_engine_system(Lum_Stage_Pre,          -1000, lum__sys_script,     NULL);
  lum_flow_add_engine_system(Lum_Stage_Transform,        0, lum__sys_transform,  NULL);
  lum_flow_add_engine_system(Lum_Stage_Commit,           0, lum__sys_commit,     NULL);
  lum_flow_add_engine_system(Lum_Stage_View_Begin,       0, lum__sys_view_begin, NULL);
  lum_flow_add_engine_system(Lum_Stage_Scene,        -1000, lum__sys_sprite,     NULL);
}

void lum_scene_register_script(const char* name, lum_entity_start_fn start, lum_entity_update_fn update) {
  if (!name || (!start && !update)) return;
  if (g_scripts.elem_size == 0) {
    if (!lum_vector_init(&g_scripts, sizeof(lum_Script_entry))) return;
  }
  /* 同名重注册：覆盖回调 */
  for (uint32_t i = 0; i < lum_vector_size(&g_scripts); ++i) {
    lum_Script_entry* entry = (lum_Script_entry*)lum_vector_get(&g_scripts, i);
    if (strcmp(entry->name, name) == 0) {
      entry->start = start;
      entry->update = update;
      return;
    }
  }
  lum_Script_entry* entry = (lum_Script_entry*)lum_vector_emplace(&g_scripts);
  if (!entry) return;
  entry->name = name;   /* 非拥有：调用方传字符串字面量或长命内存 */
  entry->start = start;
  entry->update = update;
}

const char* lum_scene_script_name(lum_entity_start_fn start, lum_entity_update_fn update) {
  for (uint32_t i = 0; i < lum_vector_size(&g_scripts); ++i) {
    const lum_Script_entry* entry = (const lum_Script_entry*)lum_vector_get_const(&g_scripts, i);
    if (entry->start == start && entry->update == update) return entry->name;
  }
  return NULL;
}

bool lum_scene_script_lookup(const char* name, lum_entity_start_fn* out_start, lum_entity_update_fn* out_update) {
  if (out_start) *out_start = NULL;
  if (out_update) *out_update = NULL;
  if (!name) return false;
  for (uint32_t i = 0; i < lum_vector_size(&g_scripts); ++i) {
    const lum_Script_entry* entry = (const lum_Script_entry*)lum_vector_get_const(&g_scripts, i);
    if (strcmp(entry->name, name) == 0) {
      if (out_start) *out_start = entry->start;
      if (out_update) *out_update = entry->update;
      return true;
    }
  }
  return false;
}

/* ---------- 视图 ---------- */

lum_View lum_view_default(const char* name) {
  lum_View view;
  memset(&view, 0, sizeof(view));
  view.name = name;
  lum_camera2d_init(&view.camera, 1.0f, 1.0f);   /* 尺寸在渲染时按视口覆盖 */
  view.category_mask = 0u;                       /* 0 = 不筛选 */
  view.clear_enabled = false;
  return view;
}

int lum_view_add(lum_Ecs* ecs, const lum_View* desc) {
  if (!ecs || !desc) return 0;
  if (g_views.elem_size == 0) return 0;
  return lum__views_insert(*desc);
}

lum_View* lum_view_get(lum_Ecs* ecs, int index) {
  if (!ecs || index < 0 || (uint32_t)index >= lum_vector_size(&g_views)) return NULL;
  return (lum_View*)lum_vector_get(&g_views, (uint32_t)index);
}

int lum_view_count(const lum_Ecs* ecs) {
  (void)ecs;
  return (int)lum_vector_size(&g_views);
}

void lum_view_clear_all(lum_Ecs* ecs) {
  (void)ecs;
  lum_vector_clear(&g_views);
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
  sprite->category = Lum_Sprite_Category_Default;
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

/* ---------- 实体名 ---------- */

static void lum__scene_name_copy(lum_Component_name* name, const char* text) {
  if (!text) text = "";
  size_t length = strlen(text);
  if (length >= Lum_Scene_Name_Max) length = Lum_Scene_Name_Max - 1;   /* 截断，保 NUL */
  memcpy(name->text, text, length);
  name->text[length] = '\0';
}

lum_Component_name* lum_scene_add_name(lum_Ecs* ecs, lum_Entity e, const char* name) {
  lum_Component_name* component = (lum_Component_name*)lum_ecs_add(ecs, e, lum_Component_name_id);
  if (!component) return NULL;
  lum__scene_name_copy(component, name);
  return component;
}

void lum_scene_set_name(lum_Ecs* ecs, lum_Entity e, const char* name) {
  lum_Component_name* component = (lum_Component_name*)lum_ecs_get(ecs, e, lum_Component_name_id);
  if (!component) component = lum_scene_add_name(ecs, e, name);
  else lum__scene_name_copy(component, name);
}

const char* lum_scene_name(const lum_Ecs* ecs, lum_Entity e) {
  const lum_Component_name* component = (const lum_Component_name*)lum_ecs_get(ecs, e, lum_Component_name_id);
  return component ? component->text : NULL;
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

/* ---------- 拾取 ---------- */

lum_Entity lum_scene_pick(const lum_Ecs* ecs, lum_Vec2 world_point) {
  if (!ecs) return Lum_Entity_Null;

  lum_Entity best = Lum_Entity_Null;
  int32_t best_layer = 0;

  lum_Ecs_view v = lum_ecs_view_create2((lum_Ecs*)ecs, lum_Component_transform_id, lum_Component_sprite_id);
  lum_Component_transform* t; lum_Component_sprite* sp; lum_Entity e;
  while ((e = lum_ecs_view_each2(&v, (void**)&t, (void**)&sp)) != Lum_Entity_Null) {
    /* world 列向量长度 = 层级缩放（列主序：列0 = m[0..1]，列1 = m[4..5]）；
       平移 = m[12..13]。旋转不进 AABB——带旋转实体的框按未旋转包住自身尺寸 */
    float scale_x = sqrtf(t->world.m[0] * t->world.m[0] + t->world.m[1] * t->world.m[1]);
    float scale_y = sqrtf(t->world.m[4] * t->world.m[4] + t->world.m[5] * t->world.m[5]);
    float half_w = sp->size.x * scale_x * 0.5f;
    float half_h = sp->size.y * scale_y * 0.5f;
    float cx = t->world.m[12], cy = t->world.m[13];

    if (world_point.x < cx - half_w || world_point.x > cx + half_w ||
        world_point.y < cy - half_h || world_point.y > cy + half_h) continue;
    if (best != Lum_Entity_Null && sp->layer < best_layer) continue;   /* 平局后到者胜：dense 序靠后 = 绘制在上 */
    best = e;
    best_layer = sp->layer;
  }
  return best;
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

static void lum__sys_script(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)user_data;
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
    if (script->update) script->update(ecs, e, ctx->dt);
  }
}

/* ---------- 引擎系统 ---------- */

/* Transform：结构点，local → world（引擎独占阶段） */
static void lum__sys_transform(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ctx; (void)user_data;

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

/* Commit：结构点，回收本帧墓碑（引擎独占阶段） */
static void lum__sys_commit(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ctx; (void)user_data;
  lum_ecs_flush(ecs);
}

/* Sprite：Scene 阶段的普通系统（priority -1000，可被用户系统显式排布）。
   每个视图被调用一次，靠 ctx->view 的分类掩码决定跳过哪些实体 */
static void lum__sys_sprite(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)user_data;
  uint32_t mask = (ctx && ctx->view) ? ctx->view->category_mask : 0u;   /* 0 = 不筛选 */

  lum_renderer2d_set_blend_mode(Lum_Blend_Alpha);
  lum_Ecs_view v = lum_ecs_view_create2(ecs, lum_Component_transform_id, lum_Component_sprite_id);
  lum_Component_transform* t; lum_Component_sprite* sp; lum_Entity e;
  while ((e = lum_ecs_view_each2(&v, (void**)&t, (void**)&sp)) != Lum_Entity_Null) {
    if (!sp->texture) continue;
    if (mask != 0u && (sp->category & mask) == 0u) continue;   /* 本视图不看这一类 */

    lum_Vec4 uv = sp->uv;
    if (sp->flip_x) { float tmp = uv.x; uv.x = uv.z; uv.z = tmp; }
    if (sp->flip_y) { float tmp = uv.y; uv.y = uv.w; uv.w = tmp; }

    /* size 不进层级矩阵：在 world 之后再乘自身尺寸缩放，父级缩放不吞掉它 */
    lum_Transform2d st = lum_transform2d_create(lum_vec2_0(), sp->size, lum_vec2_1(), 0.0f, lum_vec2_0());
    lum_Mat4 model = lum_mat4_mul(t->world, lum_transform2d_get_matrix(&st));
    lum_renderer2d_draw_sprite_matrix_uv(sp->texture, model, uv, sp->layer, sp->color);
  }
}

/* View_Begin：绑目标 / 视口 / 区域清屏 / 相机 VP（引擎独占阶段，每视图被调用一次） */
static void lum__sys_view_begin(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ecs; (void)user_data;
  const lum_View* view = ctx ? ctx->view : NULL;
  if (!view) return;

  if (view->target && view->target->initialized) lum_rt_bind(view->target);
  else lum_rt_bind_screen();

  /* 视口：w/h <= 0 表示沿用当前视口（全屏） */
  int vx = 0, vy = 0, vw = 0, vh = 0;
  if (view->viewport[2] > 0 && view->viewport[3] > 0) {
    vx = view->viewport[0]; vy = view->viewport[1];
    vw = view->viewport[2]; vh = view->viewport[3];
    lum_renderer2d_set_viewport(vx, vy, vw, vh);
  } else {
    lum_renderer2d_get_viewport(&vx, &vy, &vw, &vh);
  }

  /* glClear 只受 scissor 限制、不受 viewport 限制：多视图必须用区域清 */
  if (view->clear_enabled) {
    lum_renderer2d_clear_region(vx, vy, vw, vh, view->clear);
  }

  /* 相机 VP：把相机 viewport 设为视图像素尺寸，使正交投影正好铺满该矩形 */
  lum_Camera2d cam = view->camera;
  if (cam.zoom <= 0.0f) cam.zoom = 1.0f;   /* 兜底：未初始化的相机（zoom=0）会算出退化矩阵 */
  lum_camera2d_set_viewport(&cam, (float)vw, (float)vh);
  lum_renderer2d_set_view_projection(lum_camera2d_get_view_projection_matrix(&cam));
}

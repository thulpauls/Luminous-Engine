#include "components/lum_component_glow.h"
#include "effects/lum_glow.h"
#include "lum_renderer2d.h"
#include "lum_serial.h"

/* 组件 id 缓存：register 时填入；引擎默认单世界（lum_app 持有一个 ecs），
   多世界各自 register 时以各自 world 的注册为准刷新缓存 */
static lum_Component_type g_glow_type = Lum_Component_Invalid;

/* 字段表：默认值同时是 add() 的初始值与存档缺字段的回填值 */
static const lum_Component_field_desc g_glow_fields[] = {
  Lum_Component_Make_Field_Bool(lum_Component_glow, ring,              "ring",            false),
  Lum_Component_Make_Field_F32 (lum_Component_glow, radius,            "radius",          60.0f),
  Lum_Component_Make_Field_I32 (lum_Component_glow, layer,             "layer",           0),
  Lum_Component_Make_Field_Vec4(lum_Component_glow, color,             "color",           1, 1, 1, 1),
  Lum_Component_Make_Field_F32 (lum_Component_glow, core_size,         "core_size",       0.18f),
  Lum_Component_Make_Field_F32 (lum_Component_glow, core_decay,        "core_decay",      1.0f),
  Lum_Component_Make_Field_F32 (lum_Component_glow, edge_decay,        "edge_decay",      4.0f),
  Lum_Component_Make_Field_F32 (lum_Component_glow, halo_intensity,    "halo_intensity",  0.35f),
  Lum_Component_Make_Field_F32 (lum_Component_glow, ring_radius,       "ring_radius",     0.6f),
  Lum_Component_Make_Field_F32 (lum_Component_glow, ring_width,        "ring_width",      0.12f),
  Lum_Component_Make_Field_F32 (lum_Component_glow, ring_intensity,    "ring_intensity",  1.0f),
  Lum_Component_Make_Field_F32 (lum_Component_glow, ring_inner_decay,  "ring_inner_decay",1.0f),
  Lum_Component_Make_Field_F32 (lum_Component_glow, ring_outer_decay,  "ring_outer_decay",1.0f),
};

lum_Component_type lum_component_glow_register(lum_Ecs* ecs) {
  if (!ecs) return Lum_Component_Invalid;
  lum_Component_type existing = lum_ecs_component_type_from_name(ecs, "glow");
  if (existing != Lum_Component_Invalid) {
    g_glow_type = existing;   /* 幂等：同 world 重复注册返回已注册 id */
    return existing;
  }
  g_glow_type = lum_serial_register_component(ecs, "glow", sizeof(lum_Component_glow),
                                              g_glow_fields,
                                              sizeof(g_glow_fields) / sizeof(g_glow_fields[0]));
  return g_glow_type;
}

lum_Component_type lum_component_glow_type(void) {
  return g_glow_type;
}

lum_Component_glow* lum_component_glow_add(lum_Ecs* ecs, lum_Entity e) {
  if (g_glow_type == Lum_Component_Invalid) return NULL;
  lum_Component_glow* glow = (lum_Component_glow*)lum_ecs_add(ecs, e, g_glow_type);
  if (!glow) return NULL;
  glow->ring = false;
  glow->radius = 60.0f;
  glow->layer = 0;
  glow->color = lum_vec4_create(1.0f, 1.0f, 1.0f, 1.0f);
  glow->core_size = 0.18f; glow->core_decay = 1.0f;
  glow->edge_decay = 4.0f; glow->halo_intensity = 0.35f;
  glow->ring_radius = 0.6f; glow->ring_width = 0.12f; glow->ring_intensity = 1.0f;
  glow->ring_inner_decay = 1.0f; glow->ring_outer_decay = 1.0f;
  return glow;
}

void lum_component_glow_render_system(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ctx;
  (void)user_data;
  if (g_glow_type == Lum_Component_Invalid) return;
  lum_renderer2d_set_blend_mode(Lum_Blend_Additive);
  lum_Ecs_view v = lum_ecs_view_create2(ecs, lum_Component_transform_id, g_glow_type);
  lum_Component_transform* t; lum_Component_glow* glow; lum_Entity e;
  while ((e = lum_ecs_view_each2(&v, (void**)&t, (void**)&glow)) != Lum_Entity_Null) {
    /* 渲染中心 = world 平移分量（列主序 m[12..13]）：跟随父级层级变换 */
    lum_Vec2 center = lum_vec2_create(t->world.m[12], t->world.m[13]);
    if (glow->ring) {
      lum_glow_draw_ring_advanced(center, glow->radius, glow->layer, glow->color,
                                  glow->ring_radius, glow->ring_width, glow->ring_intensity,
                                  glow->ring_inner_decay, glow->ring_outer_decay);
    } else {
      lum_glow_draw_advanced(center, glow->radius, glow->layer, glow->color,
                             glow->core_size, glow->core_decay, glow->edge_decay,
                             glow->halo_intensity);
    }
  }
}

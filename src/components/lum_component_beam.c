#include "components/lum_component_beam.h"
#include "effects/lum_beam.h"
#include "lum_renderer2d.h"
#include "lum_serial.h"

static lum_Component_type g_beam_type = Lum_Component_Invalid;

static const lum_Component_field_desc g_beam_fields[] = {
  Lum_Component_Make_Field_Vec2(lum_Component_beam, start_offset, "start",     0, 0),
  Lum_Component_Make_Field_Vec2(lum_Component_beam, end_offset,   "end",       0, 0),
  Lum_Component_Make_Field_F32 (lum_Component_beam, thickness,    "thickness", 10.0f),
  Lum_Component_Make_Field_I32 (lum_Component_beam, mode,         "mode",      0),
  Lum_Component_Make_Field_I32 (lum_Component_beam, layer,        "layer",     0),
  Lum_Component_Make_Field_Vec4(lum_Component_beam, color,        "color",     1, 1, 1, 1),
  Lum_Component_Make_Field_F32 (lum_Component_beam, core_size,    "core_size", 0.25f),
  Lum_Component_Make_Field_F32 (lum_Component_beam, core_decay,   "core_decay",1.0f),
  Lum_Component_Make_Field_F32 (lum_Component_beam, edge_decay,   "edge_decay",4.0f),
};

lum_Component_type lum_component_beam_register(lum_Ecs* ecs) {
  if (!ecs) return Lum_Component_Invalid;
  lum_Component_type existing = lum_ecs_component_type_from_name(ecs, "beam");
  if (existing != Lum_Component_Invalid) {
    g_beam_type = existing;
    return existing;
  }
  g_beam_type = lum_serial_register_component(ecs, "beam", sizeof(lum_Component_beam),
                                              g_beam_fields,
                                              sizeof(g_beam_fields) / sizeof(g_beam_fields[0]));
  return g_beam_type;
}

lum_Component_type lum_component_beam_type(void) {
  return g_beam_type;
}

lum_Component_beam* lum_component_beam_add(lum_Ecs* ecs, lum_Entity e) {
  if (g_beam_type == Lum_Component_Invalid) return NULL;
  lum_Component_beam* beam = (lum_Component_beam*)lum_ecs_add(ecs, e, g_beam_type);
  if (!beam) return NULL;
  beam->start_offset = lum_vec2_create(0.0f, 0.0f);
  beam->end_offset = lum_vec2_create(0.0f, 0.0f);
  beam->thickness = 10.0f;
  beam->mode = Lum_Beam_Mode_Lateral;
  beam->layer = 0;
  beam->color = lum_vec4_create(1.0f, 1.0f, 1.0f, 1.0f);
  beam->core_size = 0.25f; beam->core_decay = 1.0f; beam->edge_decay = 4.0f;
  return beam;
}

void lum_component_beam_render_system(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ctx;
  (void)user_data;
  if (g_beam_type == Lum_Component_Invalid) return;
  lum_renderer2d_set_blend_mode(Lum_Blend_Additive);
  lum_Ecs_view v = lum_ecs_view_create2(ecs, lum_Component_transform_id, g_beam_type);
  lum_Component_transform* t; lum_Component_beam* beam; lum_Entity e;
  while ((e = lum_ecs_view_each2(&v, (void**)&t, (void**)&beam)) != Lum_Entity_Null) {
    /* 端点偏移过 world 矩阵：跟随实体位置与父级层级变换（旋转/缩放带动端点） */
    lum_Vec4 p1 = lum_mat4_mul_vec4(t->world, lum_vec4_create(beam->start_offset.x, beam->start_offset.y, 0.0f, 1.0f));
    lum_Vec4 p2 = lum_mat4_mul_vec4(t->world, lum_vec4_create(beam->end_offset.x, beam->end_offset.y, 0.0f, 1.0f));
    lum_beam_draw_advanced(lum_vec2_create(p1.x, p1.y), lum_vec2_create(p2.x, p2.y),
                           beam->thickness, (lum_Beam_mode)beam->mode, beam->layer,
                           beam->color, beam->core_size, beam->core_decay, beam->edge_decay);
  }
}

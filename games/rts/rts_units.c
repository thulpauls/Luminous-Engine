#include "rts_units.h"
#include "rts_layers.h"

static lum_Component_type g_unit_type = Lum_Component_Invalid;

void rts_units_register(lum_Ecs* ecs) {
  if (!ecs) return;
  if (g_unit_type != Lum_Component_Invalid) return;   /* 幂等 */
  g_unit_type = lum_ecs_register_component(ecs, "rts_unit", sizeof(rts_Unit));
  if (g_unit_type == Lum_Component_Invalid) {
    Lum_Log_Error("rts: unit component registration failed.");
  }
}

lum_Component_type rts_units_type(void) {
  return g_unit_type;
}

lum_Entity rts_units_spawn(lum_Ecs* ecs, lum_Vec2 position, float side,
                           lum_Vec4 color, uint8_t team, const lum_Texture2d* texture) {
  if (!ecs || g_unit_type == Lum_Component_Invalid) return Lum_Entity_Null;

  lum_Entity e = lum_ecs_create_entity(ecs);
  if (e == Lum_Entity_Null) return Lum_Entity_Null;

  /* 三个组件挂在不同池上，所以本次调用内先取的指针不会被后取的 add 失效。
     但【绝不能跨 spawn 调用持有指针】—— 同类型池再 add 会 realloc 搬家。 */
  lum_Component_transform* t = lum_scene_add_transform(ecs, e);
  if (!t) return Lum_Entity_Null;
  t->position = position;

  lum_Component_sprite* s = lum_scene_add_sprite(ecs, e, texture);
  if (!s) return Lum_Entity_Null;
  s->size = lum_vec2_create(side, side);
  s->color = color;
  s->layer = Rts_Layer_Units;   /* 全部单位同层才可能合成一个 draw call */

  rts_Unit* u = (rts_Unit*)lum_ecs_add(ecs, e, g_unit_type);
  if (!u) return Lum_Entity_Null;
  u->selected = 0;
  u->team = team;
  u->speed = 90.0f;

  return e;
}

uint32_t rts_units_count(lum_Ecs* ecs) {
  if (!ecs || g_unit_type == Lum_Component_Invalid) return 0;
  lum_Ecs_view v = lum_ecs_view_create(ecs, g_unit_type);
  rts_Unit* u;
  uint32_t n = 0;
  while (lum_ecs_view_each(&v, (void**)&u) != Lum_Entity_Null) ++n;
  return n;
}

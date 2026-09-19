#include "rts_orders.h"
#include "rts_units.h"

#include <math.h>

/* ---------- 下发指令 ---------- */

/*
* 两趟遍历：第一趟只数"有多少个选中单位"，第二趟才写位置。
*
* 为什么要两趟：网格要在写【第一个】单位之前就定形（列数依赖总数），
* 而 ECS 视图是组件池上的游标（lum_Ecs_view），一趟里边数边写的话，
* 第一个单位拿到的偏移是按"目前只见到 1 个"算出来的，会全挤在中心。
*
* 另一个约束：不能把单位指针攒进数组再写。同类型组件池 add 时会 realloc 搬家，
* 攒下的指针会失效（见 docs/CONTRACT.md 的 "ECS 指针失效"）。所以老老实实遍历两趟。
*/
uint32_t rts_orders_move_selected(lum_Ecs* ecs, const rts_Navgrid* nav, lum_Vec2 target) {
  if (!ecs) return 0;
  const lum_Component_type unit_t = rts_units_type();
  if (unit_t == Lum_Component_Invalid) return 0;

  /* 第一趟：数选中单位，并取最大边长 —— 格距用最大边长，保证大单位也不重叠 */
  uint32_t n = 0;
  float max_side = 0.0f;
  {
    lum_Ecs_view v = lum_ecs_view_create3(ecs, lum_Component_transform_id,
                                          lum_Component_sprite_id, unit_t);
    lum_Component_transform* t;
    lum_Component_sprite* sp;
    rts_Unit* u;
    while (lum_ecs_view_each3(&v, (void**)&t, (void**)&sp, (void**)&u) != Lum_Entity_Null) {
      if (!u->selected) continue;
      ++n;
      const float side = (sp->size.x > sp->size.y) ? sp->size.x : sp->size.y;
      if (side > max_side) max_side = side;
    }
  }
  if (n == 0) return 0;
  if (!(max_side > 0.0f)) max_side = 1.0f;   /* 尺寸异常（0/NaN）时兜个底，别除出 0 格距 */

  /* 网格：列数 = ceil(sqrt(n))，整体以 target 为中心。
     n = 4 时得到 2×2、偏移正好是 (±0.5, ±0.5)×格距 —— 重心恰为 target，便于断言。 */
  uint32_t cols = 1;
  while (cols * cols < n) ++cols;
  const uint32_t rows = (n + cols - 1) / cols;
  const float spacing = max_side * Rts_Orders_Spread;

  /* 第二趟：按 dense 序（稳定）依次发目标点，并给每个单位算一条路 */
  uint32_t issued = 0;
  uint32_t i = 0;
  {
    lum_Ecs_view v = lum_ecs_view_create2(ecs, lum_Component_transform_id, unit_t);
    lum_Component_transform* t;
    rts_Unit* u;
    while (lum_ecs_view_each2(&v, (void**)&t, (void**)&u) != Lum_Entity_Null) {
      if (!u->selected) continue;

      const float col = (float)(i % cols) - (float)(cols - 1) * 0.5f;
      const float row = (float)(i / cols) - (float)(rows - 1) * 0.5f;
      lum_Vec2 slot = lum_vec2_create(target.x + col * spacing, target.y + row * spacing);

      u->path.count = 0;
      u->path.truncated = 0;
      u->path_index = 0;

      if (nav) {
        /* 把终点夹到最近的可通行格 —— 玩家点在墙上时落到墙边，而不是"没反应"。
           夹后的点【必须】同时写进 move_target：否则单位永远够不到原目标，
           会一步一重规划。 */
        slot = rts_navgrid_clamp_to_free(nav, slot);
        u->move_target = slot;
        if (!rts_path_find(nav, t->position, slot, &u->path)) {
          u->has_move = 0;      /* 没路可走就不接指令，避免直线撞墙 */
          continue;
        }
      } else {
        u->move_target = slot;
        u->path.points[0] = slot;   /* 没有网格：一个路点 = 直线趋近（M2 行为） */
        u->path.count = 1;
      }

      u->has_move = 1;
      ++issued;
      ++i;
    }
  }
  return issued;
}

/* ---------- 推进 ---------- */

/*
* 走一步。返回 true 表示这条指令已经完成（调用方负责清 has_move）。
*
* 用一个循环是因为：一帧内可能连着"吃掉"好几个已经到达的路点
* （比如路点间距比一步位移还短）。循环上限用来兜住意外的死循环。
*/
static bool rts__advance_one(lum_Component_transform* t, rts_Unit* u,
                             const rts_Navgrid* nav, float dt) {
  for (int32_t guard = 0; guard < (int32_t)Rts_Path_Max_Waypoints + 2; ++guard) {

    /* 路点走完但还没到最终目标 —— 只有"路径被截断"才会出现这种情况。
       就地重新规划一段；规划不出来就只能作罢（清 truncated，免得每帧重试）。 */
    if (u->path_index >= u->path.count && u->path.truncated) {
      u->path.truncated = 0;
      if (nav && rts_path_find(nav, t->position, u->move_target, &u->path)) {
        u->path_index = 0;
        continue;
      }
    }

    const bool has_wp = (u->path_index < u->path.count);
    const lum_Vec2 aim = has_wp ? u->path.points[u->path_index] : u->move_target;
    const float tol = has_wp ? Rts_Orders_Waypoint_Tol : Rts_Orders_Arrive;

    const float dx = aim.x - t->position.x;
    const float dy = aim.y - t->position.y;
    const float dist2 = dx * dx + dy * dy;

    if (dist2 <= tol * tol) {
      if (has_wp) { ++u->path_index; continue; }   /* 吃掉这个路点，看下一个 */
      t->position = u->move_target;                /* 终点：落成精确值 */
      return true;
    }

    const float step = u->speed * dt;
    if (step * step >= dist2) {
      /* 这一步就能跨过它 —— 精确落点，绝不越过 */
      t->position = aim;
      if (has_wp) { ++u->path_index; continue; }
      return true;
    }

    const float k = step / sqrtf(dist2);           /* dist2 > tol² ≥ 0，不会除零 */
    t->position = lum_vec2_create(t->position.x + dx * k, t->position.y + dy * k);
    return false;                                  /* 这一步走完了，但还没到 */
  }
  return false;
}

void rts_orders_update(lum_Ecs* ecs, const rts_Navgrid* nav, float dt) {
  /* dt <= 0 一律不推进：非步进阶段 ctx->dt 就是 0，不能靠 dt 顺手当"帧标记"用。
     NaN 也会被这个写法挡住（!(NaN > 0) 为真）。 */
  if (!ecs || !(dt > 0.0f)) return;
  const lum_Component_type unit_t = rts_units_type();
  if (unit_t == Lum_Component_Invalid) return;

  lum_Ecs_view v = lum_ecs_view_create2(ecs, lum_Component_transform_id, unit_t);
  lum_Component_transform* t;
  rts_Unit* u;
  while (lum_ecs_view_each2(&v, (void**)&t, (void**)&u) != Lum_Entity_Null) {
    if (!u->has_move) continue;

    /* speed <= 0 的单位永远到不了，别让它一直占着"正在执行"的状态 */
    if (!(u->speed > 0.0f)) { u->has_move = 0; continue; }

    if (rts__advance_one(t, u, nav, dt)) u->has_move = 0;
  }
}

uint32_t rts_orders_active_count(lum_Ecs* ecs) {
  if (!ecs) return 0;
  const lum_Component_type unit_t = rts_units_type();
  if (unit_t == Lum_Component_Invalid) return 0;

  uint32_t n = 0;
  lum_Ecs_view v = lum_ecs_view_create(ecs, unit_t);
  rts_Unit* u;
  while (lum_ecs_view_each(&v, (void**)&u) != Lum_Entity_Null) {
    if (u->has_move) ++n;
  }
  return n;
}

uint32_t rts_orders_remaining_waypoints(lum_Ecs* ecs) {
  if (!ecs) return 0;
  const lum_Component_type unit_t = rts_units_type();
  if (unit_t == Lum_Component_Invalid) return 0;

  uint32_t n = 0;
  lum_Ecs_view v = lum_ecs_view_create(ecs, unit_t);
  rts_Unit* u;
  while (lum_ecs_view_each(&v, (void**)&u) != Lum_Entity_Null) {
    if (u->has_move && u->path.count > u->path_index) {
      n += (uint32_t)(u->path.count - u->path_index);
    }
  }
  return n;
}

/* ---------- 世界空间可视化 ---------- */

void rts_orders_draw_mark(lum_Vec2 world, int32_t layer) {
  const lum_Vec4 c = lum_vec4_create(0.45f, 1.00f, 0.55f, 0.75f);
  const float h = 7.0f;
  const float th = 1.5f;

  const lum_Vec2 p[4] = {
    { world.x - h, world.y - h },
    { world.x + h, world.y - h },
    { world.x + h, world.y + h },
    { world.x - h, world.y + h }
  };
  for (int i = 0; i < 4; ++i) {
    lum_renderer2d_draw_line(p[i], p[(i + 1) & 3], th, layer, c);
  }
}

void rts_orders_draw_paths(lum_Ecs* ecs, int32_t layer) {
  if (!ecs) return;
  const lum_Component_type unit_t = rts_units_type();
  if (unit_t == Lum_Component_Invalid) return;

  const lum_Vec4 c = lum_vec4_create(0.35f, 0.85f, 1.00f, 0.55f);
  const float th = 1.5f;

  lum_Ecs_view v = lum_ecs_view_create2(ecs, lum_Component_transform_id, unit_t);
  lum_Component_transform* t;
  rts_Unit* u;
  while (lum_ecs_view_each2(&v, (void**)&t, (void**)&u) != Lum_Entity_Null) {
    if (!u->selected || !u->has_move) continue;

    lum_Vec2 prev = t->position;
    for (uint8_t i = u->path_index; i < u->path.count; ++i) {
      lum_renderer2d_draw_line(prev, u->path.points[i], th, layer, c);
      prev = u->path.points[i];
    }
    /* 路点用尽（截断后正在重规划）时，把最终目标也连出来，免得线看着断开 */
    if (u->path_index >= u->path.count) {
      lum_renderer2d_draw_line(prev, u->move_target, th, layer, c);
    }
  }
}

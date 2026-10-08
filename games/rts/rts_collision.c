#include "rts_collision.h"
#include "rts_units.h"

#include <math.h>

#define RTS_SQRT2 1.41421356f

/* 碰撞箱半宽 = 精灵尺寸的一半（取较大边，保证方形精灵完全被包住） */
static float rts__half_of(const lum_Component_sprite* sp) {
  const float side = (sp->size.x > sp->size.y) ? sp->size.x : sp->size.y;
  const float half = side * 0.5f;
  return (half > 0.0f) ? half : 0.0f;
}

/* 这个位置可不可以站。nav 为 NULL 时一律放行 */
static bool rts__free_at(const rts_Navgrid* nav, lum_Vec2 p) {
  if (!nav) return true;
  int32_t cx = 0, cy = 0;
  if (!rts_navgrid_world_to_cell(p, &cx, &cy)) return false;
  return !rts_navgrid_blocked(nav, cx, cy);
}

/* 从 from 推到 to 允不允许。
   除了"目的地可站"，还额外放行一种情况：单位【当前就陷在被堵格里】。
   道理和 A* 的"起点格豁免"完全一样 —— 否则它会每次都被拒绝、被永久冻在原地
   （实测踩过：把单位堆到墙里之后，重叠对数一动不动）。正常玩法不会出现
   单位在墙里，但建筑盖在单位身上、地形变动之类的将来会。 */
static bool rts__dest_ok(const rts_Navgrid* nav, lum_Vec2 from, lum_Vec2 to) {
  if (!nav) return true;
  if (rts__free_at(nav, to)) return true;
  return !rts__free_at(nav, from);
}

/* 把单位挪到 to：整段推不进去就退化成只推一个轴 ——
   贴着墙被挤的单位于是沿墙滑，而不是卡死或者嵌进墙里。
   返回 1 表示确实动了。 */
static uint32_t rts__try_move(const rts_Navgrid* nav, lum_Component_transform* t,
                              lum_Vec2 to) {
  const lum_Vec2 here = t->position;
  const lum_Vec2 p = lum_vec2_sub(to, here);
  if (p.x == 0.0f && p.y == 0.0f) return 0;

  if (rts__dest_ok(nav, here, to)) {
    t->position = to;
    return 1;
  }
  const lum_Vec2 only_x = lum_vec2_create(here.x + p.x, here.y);
  const lum_Vec2 only_y = lum_vec2_create(here.x, here.y + p.y);
  /* 分量为 0 的那个回退分支等于"原地不动"，排掉，免得污染"动过没有"的读数 */
  if (p.x != 0.0f && rts__dest_ok(nav, here, only_x)) {
    t->position = only_x;
    return 1;
  }
  if (p.y != 0.0f && rts__dest_ok(nav, here, only_y)) {
    t->position = only_y;
    return 1;
  }
  return 0;   /* 完全推不动：留在原地，交给下一轮或下一帧 */
}

uint32_t rts_collision_resolve(lum_Ecs* ecs, const rts_Navgrid* nav) {
  if (!ecs) return 0;
  const lum_Component_type unit_t = rts_units_type();
  if (unit_t == Lum_Component_Invalid) return 0;

  uint32_t applied = 0;

  for (int32_t iter = 0; iter < Rts_Collision_Iterations; ++iter) {

    /* 两两判交，发现重叠【立刻就地推开】。
       这里用的是"就地应用"（Gauss-Seidel），而不是"先累加再统一应用"（Jacobi）。
       我最初写的是后者，理由是"结果与遍历顺序无关"。两者都会收敛，但同一场景实测
       （34 个单位完全对称地叠成一列、间距 3、周围无任何障碍，每 20 帧采样重叠对数）：
         Jacobi        45 → 33（卡住约 140 帧）→ … → 0    第 360 帧归零
         Gauss-Seidel  31 → 26（卡住约  80 帧）→ … → 0    第 180 帧归零
       差一倍的根源是公式决定的：完全对称的堆叠里，中间单位受到的上下推力大小相等、
       方向相反，累加后精确抵消，于是 Jacobi 只能靠两端一点一点往里解；
       就地应用是"边算边改"，一次扫描就能把修正沿着链传下去。
       代价是结果与遍历顺序有关（先被扫到的单位占一点点便宜），视觉上无所谓。

       注：我第一次量到的是"卡在 33 上 300 帧不动"，但那次堆叠位置紧挨着一个方柱，
       把"对称抵消"和"撞墙推不动"两个原因混在了一起；后来换到空地、
       用同一二进制的 env 开关重做 A/B，才有上面这组数。 */
    uint32_t i = 0;
    lum_Ecs_view va = lum_ecs_view_create3(ecs, lum_Component_transform_id,
                                          lum_Component_sprite_id, unit_t);
    lum_Component_transform* ta;
    lum_Component_sprite* sa;
    rts_Unit* ua;
    while (lum_ecs_view_each3(&va, (void**)&ta, (void**)&sa, (void**)&ua) != Lum_Entity_Null) {
      const float ha = rts__half_of(sa);

      /* 内层是同一批单位的第二趟遍历。两个视图各是一份游标、互不共享状态，
         所以嵌套遍历是安全的。 */
      uint32_t j = 0;
      lum_Ecs_view vb = lum_ecs_view_create3(ecs, lum_Component_transform_id,
                                            lum_Component_sprite_id, unit_t);
      lum_Component_transform* tb;
      lum_Component_sprite* sb;
      rts_Unit* ub;
      while (lum_ecs_view_each3(&vb, (void**)&tb, (void**)&sb, (void**)&ub) != Lum_Entity_Null) {
        if (j > i) {   /* 每对只处理一次 */
          (void)ua; (void)ub;
          const float hb = rts__half_of(sb);

          /* 宽相：两个箱子的外接圆不相交，箱子必不相交。
             方形半宽 h 的外接半径是 h·√2，所以临界中心距 = (ha+hb)·√2。
             这一层让绝大多数远隔的单位根本不进 SAT。
             读的是【当前】位置 —— 就地应用后位置会变，必须重新读。 */
          const float reach = (ha + hb) * RTS_SQRT2;
          const float dx = tb->position.x - ta->position.x;
          const float dy = tb->position.y - ta->position.y;
          if (dx * dx + dy * dy <= reach * reach) {
            const lum_Box ba = { ta->position, lum_vec2_create(ha, ha), 0.0f };
            const lum_Box bb = { tb->position, lum_vec2_create(hb, hb), 0.0f };
            lum_Contact c;
            if (lum_collide_box_box(&ba, &bb, &c) && c.depth > Rts_Collision_Slop) {
              /* 引擎约定 normal 方向是 A→B，depth 是【整段】最小平移量。
                 两边各承担一半，才是对称地把它们分开；
                 若两边各推一整个 depth，就会分开到两倍距离。 */
              const lum_Vec2 p = lum_vec2_scale(c.normal, c.depth * 0.5f);

              /* 每次挪动都限幅：密集时一步可能推得很大，一次弹开看着像"炸了" */
              lum_Vec2 pa = lum_vec2_sub(ta->position, p);
              lum_Vec2 pb = lum_vec2_add(tb->position, p);
              const lum_Vec2 da = lum_vec2_sub(pa, ta->position);
              const lum_Vec2 db = lum_vec2_sub(pb, tb->position);
              if (da.x * da.x + da.y * da.y > Rts_Collision_Max_Push * Rts_Collision_Max_Push) {
                pa = lum_vec2_add(ta->position,
                                  lum_vec2_scale(da, Rts_Collision_Max_Push / sqrtf(da.x * da.x + da.y * da.y)));
              }
              if (db.x * db.x + db.y * db.y > Rts_Collision_Max_Push * Rts_Collision_Max_Push) {
                pb = lum_vec2_add(tb->position,
                                  lum_vec2_scale(db, Rts_Collision_Max_Push / sqrtf(db.x * db.x + db.y * db.y)));
              }

              applied += rts__try_move(nav, ta, pa);   /* A 沿 -normal */
              applied += rts__try_move(nav, tb, pb);   /* B 沿 +normal */
            }
          }
        }
        ++j;
      }
      ++i;
    }
  }

  return applied;
}

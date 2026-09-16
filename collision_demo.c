/* collision_demo —— lum_collision（纯几何碰撞层）的图形测试样例。
 *
 * 演示内容：
 *   - 多种凸多边形（正 N 边形 / 随机凸包 / OBB）与圆之间的两两碰撞
 *   - 宽相（lum_shape_bounds 的 AABB 粗筛）→ 窄相（lum_collide 的 SAT / 最近点）
 *   - 用 Contact 的 normal / depth / point 做位置修正（去穿透）与冲量响应（弹性 + 摩擦）
 *   - 四种场景：堆叠 / 斜坡 / 零重力互撞 / 压力测试
 *
 * 按键：SPACE 换场景  R 重置  G 重力开关  A 显示 AABB  N 显示接触
 *       鼠标左键 在光标处生成随机形状    Q 退出
 *
 * 说明：本样例只用"纯几何层"，不涉及 GL/ECS。静态几何与动态刚体统一表示成 Body
 *       （静态体 inv_mass = 0），因此同一套 solve_pair 可处理任意组合。 */

#include "luminous.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------- 常量 ---------------- */

#define Max_Bodies       110
#define Max_Verts        10       /* 单个形状顶点上限 */
#define Max_Contacts     768      /* 调试绘制用接触记录上限 */
#define Solver_Iterations 4       /* 每步求解迭代次数（堆叠收敛用） */
#define Fixed_Step       (1.0f / 120.0f)
#define Max_Steps_Per_Frame 4

#define Gravity_Y          (-900.0f)
#define Restitution        0.22f
#define Friction           0.35f
#define Restitution_Vel_Threshold 40.0f
#define Penetration_Slop   0.35f
#define Correction_Percent 0.7f

/* 绘制层级：渲染器按 (layer, material, texture, primitive) 排序，
   同层内不同 primitive 会被重排（圆是 Triangles、多边形是 Triangle_Fan），
   所以静态/动态/描边必须放在不同层才能保证前后关系正确。 */
#define Layer_Static_Fill  0
#define Layer_Dynamic_Fill 1
#define Layer_Outline      2
#define Layer_Aabb         5
#define Layer_Contact      6
#define Layer_Hud          20

/* ---------------- 类型 ---------------- */

typedef enum {
  Body_Poly = 0,   /* 凸多边形（正 N 边形 / 随机凸包） */
  Body_Box,        /* 有向矩形 OBB：走 lum_shape_box 路径 */
  Body_Circle,     /* 真圆：走 圆×圆 / 圆×多边形 路径 */
} Body_kind;

typedef struct {
  Body_kind kind;
  bool      is_static;         /* 静态体：不积分，inv_mass = 0 */
  bool      alive;

  lum_Vec2  pos;               /* 质心位置（世界） */
  lum_Vec2  vel;
  float     angle, ang_vel;

  lum_Vec2  local[Max_Verts];  /* 本地顶点（已相对质心） */
  uint32_t  count;
  lum_Vec2  half;              /* Box 半宽 */
  float     radius;            /* Circle 半径 */

  lum_Vec2  world[Max_Verts];  /* 每帧缓存的世界顶点（渲染 + 多边形窄相） */
  uint32_t  world_count;

  float     inv_mass;
  float     inv_inertia;       /* 简化：按包围半径当圆盘估算 */
  float     bound_radius;

  lum_Vec4  color;
  float     flash;             /* 命中高亮衰减 [0,1] */
} Body;

typedef enum {
  Mode_Stack = 0,   /* 重力 + 地面 + 两侧墙：堆叠 */
  Mode_Slope,       /* 斜坡 + 台阶 + 立柱：滑动 / 翻滚 */
  Mode_ZeroG,       /* 零重力盒内互撞：纯碰撞响应 */
  Mode_Stress,      /* 大量随机多边形压测 */
  Mode_Count,
} Demo_mode;

static const char* kModeName[Mode_Count] = { "STACK", "SLOPE", "ZERO-G", "STRESS" };

/* ---------------- 全局状态 ---------------- */

static Body      g_bodies[Max_Bodies];
static uint32_t  g_body_count;

static Demo_mode g_mode = Mode_Stack;
static bool      g_show_aabb = false;
static bool      g_show_contact = true;
static bool      g_gravity_on = true;

static float     g_accumulator = 0.0f;

/* 统计（只在每步的第 0 次迭代里累计，避免被 Solver_Iterations 放大） */
static uint32_t  g_stat_pairs, g_stat_broad_pass, g_stat_contacts;
static uint32_t  g_stat_solver_tests;   /* 全部迭代的真实配对测试次数 */

typedef struct { lum_Vec2 point, normal; float depth; } Contact_dbg;
static Contact_dbg g_contacts[Max_Contacts];
static uint32_t    g_contact_count;

static lum_Font* g_font;

/* 确定性随机：同一场景每次重置结果一致，便于对比 */
static uint32_t g_rng = 20240901u;
static float frand(void) {
  g_rng = g_rng * 1664525u + 1013904223u;
  return (float)((g_rng >> 8) & 0xFFFFFFu) / 16777216.0f;
}
static float frand_range(float a, float b) { return a + (b - a) * frand(); }
static uint32_t irand_range(uint32_t a, uint32_t b) {
  return a + (uint32_t)(frand() * (float)(b - a + 1u)) % (b - a + 1u);
}

/* ---------------- 刚体构造 ---------------- */

static lum_Vec4 palette(uint32_t i) {
  static const lum_Vec4 kColors[6] = {
    { 0.36f, 0.72f, 1.00f, 0.85f },
    { 1.00f, 0.62f, 0.30f, 0.85f },
    { 0.45f, 0.90f, 0.55f, 0.85f },
    { 0.95f, 0.45f, 0.62f, 0.85f },
    { 0.75f, 0.60f, 1.00f, 0.85f },
    { 0.95f, 0.88f, 0.45f, 0.85f },
  };
  return kColors[i % 6u];
}

/* 顶点须已相对质心 */
static void body_finish(Body* b, lum_Vec4 color) {
  float max_r2 = 1.0f;
  for (uint32_t i = 0; i < b->count; ++i) {
    float r2 = lum_vec2_len_square(b->local[i]);
    if (r2 > max_r2) max_r2 = r2;
  }
  b->bound_radius = sqrtf(max_r2);
  b->color = color;
  if (b->is_static) {
    b->inv_mass = 0.0f;
    b->inv_inertia = 0.0f;
  } else {
    b->inv_mass = 1.0f;   /* 统一质量 */
    b->inv_inertia = 2.0f * b->inv_mass / (b->bound_radius * b->bound_radius);
  }
}

static Body* body_alloc(void) {
  if (g_body_count >= Max_Bodies) return NULL;
  Body* b = &g_bodies[g_body_count++];
  memset(b, 0, sizeof(*b));
  b->alive = true;
  return b;
}

/* 用世界坐标顶点建多边形：内部转成"质心位置 + 本地顶点" */
static Body* body_add_polygon(const lum_Vec2* world_verts, uint32_t n, lum_Vec4 color, lum_Vec2 vel, bool is_static) {
  if (n < 3u || n > Max_Verts) return NULL;
  Body* b = body_alloc();
  if (!b) return NULL;

  lum_Polygon p = { world_verts, n };
  lum_Vec2 c = lum_polygon_centroid(&p);

  b->kind = Body_Poly;
  b->is_static = is_static;
  b->pos = c;
  b->vel = vel;
  b->count = n;
  for (uint32_t i = 0; i < n; ++i) b->local[i] = lum_vec2_sub(world_verts[i], c);
  body_finish(b, color);
  return b;
}

static Body* body_add_regular(uint32_t sides, float radius, lum_Vec2 pos, float angle, lum_Vec4 color, lum_Vec2 vel) {
  lum_Vec2 v[Max_Verts];
  uint32_t n = lum_polygon_make_regular(sides, radius, angle, v, Max_Verts);
  if (!n) return NULL;
  for (uint32_t i = 0; i < n; ++i) v[i] = lum_vec2_add(v[i], pos);   /* make_regular 以原点为中心 */
  return body_add_polygon(v, n, color, vel, false);
}

/* 随机不规则凸多边形：随机点 + Andrew monotone chain 凸包 */
static int cmp_vec2(const void* pa, const void* pb) {
  const lum_Vec2* a = (const lum_Vec2*)pa;
  const lum_Vec2* b = (const lum_Vec2*)pb;
  if (a->x != b->x) return (a->x < b->x) ? -1 : 1;
  return (a->y < b->y) ? -1 : (a->y > b->y);
}
static float cross3(lum_Vec2 o, lum_Vec2 a, lum_Vec2 b) {
  return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

static Body* body_add_random_convex(lum_Vec2 pos, float radius, lum_Vec4 color, lum_Vec2 vel) {
  lum_Vec2 pts[12];
  uint32_t n = irand_range(6u, 9u);
  for (uint32_t i = 0; i < n; ++i) {
    float a = frand_range(0.0f, 2.0f * (float)Lum_Pi);
    float r = radius * frand_range(0.55f, 1.0f);
    pts[i] = lum_vec2_create(cosf(a) * r, sinf(a) * r);
  }
  qsort(pts, n, sizeof(lum_Vec2), cmp_vec2);

  lum_Vec2 hull[24];
  uint32_t k = 0;
  for (uint32_t i = 0; i < n; ++i) {                        /* 下凸壳 */
    while (k >= 2u && cross3(hull[k - 2u], hull[k - 1u], pts[i]) <= 0.0f) --k;
    hull[k++] = pts[i];
  }
  uint32_t lower = k + 1u;
  for (uint32_t i = n - 1u; i-- > 0u;) {                    /* 上凸壳 */
    while (k >= lower && cross3(hull[k - 2u], hull[k - 1u], pts[i]) <= 0.0f) --k;
    hull[k++] = pts[i];
  }
  if (k >= 2u) --k;                                         /* 去掉重复首点 */

  if (k < 3u || k > Max_Verts) {
    return body_add_regular(irand_range(3u, 7u), radius, pos, 0.0f, color, vel);
  }
  for (uint32_t i = 0; i < k; ++i) hull[i] = lum_vec2_add(hull[i], pos);
  return body_add_polygon(hull, k, color, vel, false);
}

static Body* body_add_box(lum_Vec2 pos, lum_Vec2 half, float angle, lum_Vec4 color, lum_Vec2 vel) {
  Body* b = body_alloc();
  if (!b) return NULL;
  b->kind = Body_Box;
  b->pos = pos;
  b->vel = vel;
  b->angle = angle;
  b->half = half;
  body_finish(b, color);
  return b;
}

static Body* body_add_circle(lum_Vec2 pos, float radius, lum_Vec4 color, lum_Vec2 vel) {
  Body* b = body_alloc();
  if (!b) return NULL;
  b->kind = Body_Circle;
  b->pos = pos;
  b->vel = vel;
  b->radius = radius;
  body_finish(b, color);
  return b;
}

static Body* body_add_static_rect(lum_Vec2 center, lum_Vec2 half, float angle, lum_Vec4 color) {
  lum_Vec2 v[4];
  if (!lum_polygon_make_box(center, half, angle, v, 4u)) return NULL;
  return body_add_polygon(v, 4u, color, lum_vec2_0(), true);
}

/* ---------------- 每帧缓存与形状 ---------------- */

static void body_update_world(Body* b) {
  if (b->kind == Body_Circle) { b->world_count = 0; return; }

  if (b->kind == Body_Box) {
    b->world_count = lum_polygon_make_box(b->pos, b->half, b->angle, b->world, Max_Verts);
    return;
  }

  float c = cosf(b->angle), s = sinf(b->angle);
  for (uint32_t i = 0; i < b->count; ++i) {
    lum_Vec2 l = b->local[i];
    b->world[i] = lum_vec2_create(b->pos.x + l.x * c - l.y * s,
                                  b->pos.y + l.x * s + l.y * c);
  }
  b->world_count = b->count;
}

static lum_Shape body_shape(const Body* b) {
  switch (b->kind) {
    case Body_Circle: return lum_shape_circle(b->pos, b->radius);
    case Body_Box:    return lum_shape_box(b->pos, b->half, b->angle);
    default:          return lum_shape_polygon(b->world, b->world_count);
  }
}

/* ---------------- 求解 ---------------- */

static void record_contact(const lum_Contact* c) {
  if (g_contact_count >= Max_Contacts) return;
  g_contacts[g_contact_count].point = c->point;
  g_contacts[g_contact_count].normal = c->normal;
  g_contacts[g_contact_count].depth = c->depth;
  ++g_contact_count;
}

/* 位置修正：按逆质量分配；留 Penetration_Slop 不修正以抑制抖动 */
static void solve_position(Body* a, Body* b, const lum_Contact* c) {
  float inv_sum = a->inv_mass + b->inv_mass;
  if (inv_sum <= 0.0f) return;

  float depth = c->depth - Penetration_Slop;
  if (depth <= 0.0f) return;

  lum_Vec2 push = lum_vec2_scale(c->normal, depth * Correction_Percent / inv_sum);
  a->pos = lum_vec2_sub(a->pos, lum_vec2_scale(push, a->inv_mass));
  b->pos = lum_vec2_add(b->pos, lum_vec2_scale(push, b->inv_mass));
}

/* 冲量响应：法向弹性 + 切向库仑摩擦，含转动。多次迭代时不重复施力（vn 一旦 >= 0 就跳过） */
static void solve_impulse(Body* a, Body* b, const lum_Contact* c) {
  lum_Vec2 ra = lum_vec2_sub(c->point, a->pos);
  lum_Vec2 rb = lum_vec2_sub(c->point, b->pos);

  lum_Vec2 va = lum_vec2_add(a->vel, lum_vec2_scale(lum_vec2_create(-ra.y, ra.x), a->ang_vel));
  lum_Vec2 vb = lum_vec2_add(b->vel, lum_vec2_scale(lum_vec2_create(-rb.y, rb.x), b->ang_vel));

  float vn = lum_vec2_dot(lum_vec2_sub(vb, va), c->normal);
  if (vn >= 0.0f) return;   /* 已在分离 */

  float ra_cn = ra.x * c->normal.y - ra.y * c->normal.x;
  float rb_cn = rb.x * c->normal.y - rb.y * c->normal.x;
  float inv_sum = a->inv_mass + b->inv_mass
                + ra_cn * ra_cn * a->inv_inertia
                + rb_cn * rb_cn * b->inv_inertia;
  if (inv_sum <= 0.0f) return;

  float e = (vn < -Restitution_Vel_Threshold) ? Restitution : 0.0f;
  float j = -(1.0f + e) * vn / inv_sum;
  lum_Vec2 impulse = lum_vec2_scale(c->normal, j);

  a->vel = lum_vec2_sub(a->vel, lum_vec2_scale(impulse, a->inv_mass));
  a->ang_vel -= a->inv_inertia * (ra.x * impulse.y - ra.y * impulse.x);
  b->vel = lum_vec2_add(b->vel, lum_vec2_scale(impulse, b->inv_mass));
  b->ang_vel += b->inv_inertia * (rb.x * impulse.y - rb.y * impulse.x);

  /* 摩擦：用更新后的接触点速度求切向冲量，钳制在库仑锥 |jt| <= mu * j 内 */
  lum_Vec2 tangent = lum_vec2_create(-c->normal.y, c->normal.x);
  va = lum_vec2_add(a->vel, lum_vec2_scale(lum_vec2_create(-ra.y, ra.x), a->ang_vel));
  vb = lum_vec2_add(b->vel, lum_vec2_scale(lum_vec2_create(-rb.y, rb.x), b->ang_vel));
  float vt = lum_vec2_dot(lum_vec2_sub(vb, va), tangent);

  float ra_ct = ra.x * tangent.y - ra.y * tangent.x;
  float rb_ct = rb.x * tangent.y - rb.y * tangent.x;
  float inv_sum_t = a->inv_mass + b->inv_mass
                  + ra_ct * ra_ct * a->inv_inertia
                  + rb_ct * rb_ct * b->inv_inertia;
  if (inv_sum_t <= 0.0f) return;

  float jt = -vt / inv_sum_t;
  float max_f = Friction * j;
  if (jt >  max_f) jt =  max_f;
  if (jt < -max_f) jt = -max_f;

  lum_Vec2 ft = lum_vec2_scale(tangent, jt);
  a->vel = lum_vec2_sub(a->vel, lum_vec2_scale(ft, a->inv_mass));
  a->ang_vel -= a->inv_inertia * (ra.x * ft.y - ra.y * ft.x);
  b->vel = lum_vec2_add(b->vel, lum_vec2_scale(ft, b->inv_mass));
  b->ang_vel += b->inv_inertia * (rb.x * ft.y - rb.y * ft.x);
}

/* 一对刚体：宽相 AABB 粗筛 → 窄相 lum_collide → 修正 + 冲量 */
static void solve_pair(Body* a, Body* b, bool first_iter) {
  lum_Shape sa = body_shape(a);
  lum_Shape sb = body_shape(b);

  if (first_iter) ++g_stat_pairs;

  /* --- 宽相：AABB 粗筛（必要不充分，允许假阳性） --- */
  lum_Vec2 amn, amx, bmn, bmx;
  lum_shape_bounds(&sa, &amn, &amx);
  lum_shape_bounds(&sb, &bmn, &bmx);
  if (amx.x < bmn.x || bmx.x < amn.x || amx.y < bmn.y || bmx.y < amn.y) return;
  if (first_iter) ++g_stat_broad_pass;

  /* --- 窄相：精确判定 + 接触信息 --- */
  lum_Contact c;
  if (!lum_collide(&sa, &sb, &c)) return;

  if (first_iter) {
    ++g_stat_contacts;
    record_contact(&c);
  }

  solve_position(a, b, &c);
  solve_impulse(a, b, &c);
  a->flash = 1.0f;
  b->flash = 1.0f;
}

static void integrate(Body* b, float dt) {
  if (b->is_static) return;

  if (g_gravity_on && g_mode != Mode_ZeroG) b->vel.y += Gravity_Y * dt;

  b->vel = lum_vec2_scale(b->vel, 1.0f / (1.0f + 0.25f * dt));   /* 线性阻尼 */
  b->ang_vel *= 1.0f / (1.0f + 0.60f * dt);                      /* 角阻尼 */

  b->pos = lum_vec2_add(b->pos, lum_vec2_scale(b->vel, dt));
  b->angle += b->ang_vel * dt;

  /* 视野范围夹取，防止逃逸 */
  if (b->pos.x < -520.0f) { b->pos.x = -520.0f; b->vel.x =  fabsf(b->vel.x) * 0.5f; }
  if (b->pos.x >  520.0f) { b->pos.x =  520.0f; b->vel.x = -fabsf(b->vel.x) * 0.5f; }
  if (b->pos.y >  360.0f) { b->pos.y =  360.0f; b->vel.y = -fabsf(b->vel.y) * 0.5f; }
}

static void physics_step(float dt) {
  for (uint32_t i = 0; i < g_body_count; ++i) {
    integrate(&g_bodies[i], dt);
    if (g_bodies[i].flash > 0.0f) {
      g_bodies[i].flash -= dt * 3.0f;
      if (g_bodies[i].flash < 0.0f) g_bodies[i].flash = 0.0f;
    }
  }

  for (int iter = 0; iter < Solver_Iterations; ++iter) {
    bool first = (iter == 0);
    for (uint32_t i = 0; i < g_body_count; ++i) body_update_world(&g_bodies[i]);

    for (uint32_t i = 0; i < g_body_count; ++i) {
      for (uint32_t j = i + 1u; j < g_body_count; ++j) {
        if (g_bodies[i].is_static && g_bodies[j].is_static) continue;   /* 静态×静态永不检测 */
        solve_pair(&g_bodies[i], &g_bodies[j], first);
        ++g_stat_solver_tests;
      }
    }
  }
}

/* ---------------- 场景构建 ---------------- */

static void scene_clear(void) {
  g_body_count = 0;
  g_contact_count = 0;
  g_accumulator = 0.0f;
  g_rng = 20240901u + (uint32_t)g_mode * 7919u;   /* 每个场景固定种子 */
}

static const lum_Vec4 kWallColor = { 0.17f, 0.19f, 0.25f, 1.0f };

static void scene_add_arena(void) {
  body_add_static_rect(lum_vec2_create(0.0f, -300.0f), lum_vec2_create(560.0f, 22.0f), 0.0f, kWallColor);
  body_add_static_rect(lum_vec2_create(-530.0f, 0.0f), lum_vec2_create(22.0f, 320.0f), 0.0f, kWallColor);
  body_add_static_rect(lum_vec2_create( 530.0f, 0.0f), lum_vec2_create(22.0f, 320.0f), 0.0f, kWallColor);
}

static void scene_build_stack(void) {
  scene_add_arena();

  for (uint32_t i = 0; i < 7u; ++i) {          /* 正 N 边形塔 */
    body_add_regular(3u + i % 4u, 22.0f + (float)(i % 3u) * 5.0f,
                     lum_vec2_create(-330.0f, -250.0f + (float)i * 60.0f),
                     frand_range(0.0f, 6.28f), palette(i), lum_vec2_0());
  }
  for (uint32_t i = 0; i < 6u; ++i) {          /* 圆 */
    body_add_circle(lum_vec2_create(-140.0f, -255.0f + (float)i * 54.0f), 20.0f,
                    palette(i + 2u), lum_vec2_0());
  }
  for (uint32_t i = 0; i < 6u; ++i) {          /* 随机凸多边形 */
    body_add_random_convex(lum_vec2_create(60.0f, -250.0f + (float)i * 58.0f), 26.0f,
                           palette(i + 4u), lum_vec2_0());
  }
  for (uint32_t i = 0; i < 5u; ++i) {          /* OBB */
    body_add_box(lum_vec2_create(280.0f, -255.0f + (float)i * 56.0f),
                 lum_vec2_create(30.0f, 16.0f), frand_range(0.0f, 6.28f),
                 palette(i + 1u), lum_vec2_0());
  }
}

static void scene_build_slope(void) {
  scene_add_arena();

  body_add_static_rect(lum_vec2_create(-180.0f, -90.0f), lum_vec2_create(260.0f, 18.0f), -0.32f, kWallColor);
  body_add_static_rect(lum_vec2_create( 220.0f, -190.0f), lum_vec2_create(150.0f, 16.0f), 0.12f, kWallColor);
  body_add_static_rect(lum_vec2_create( 400.0f, -110.0f), lum_vec2_create( 80.0f, 16.0f), 0.26f, kWallColor);
  body_add_static_rect(lum_vec2_create(  20.0f, -230.0f), lum_vec2_create( 12.0f, 48.0f), 0.0f,  kWallColor);

  for (uint32_t i = 0; i < 8u; ++i) {
    lum_Vec2 p = lum_vec2_create(-440.0f + (float)i * 42.0f, 210.0f + (float)(i % 3u) * 50.0f);
    lum_Vec2 v = lum_vec2_create(140.0f, 0.0f);
    if (i % 3u == 0u)      body_add_circle(p, 19.0f, palette(i), v);
    else if (i % 3u == 1u) body_add_regular(3u + i % 5u, 24.0f, p, 0.3f * (float)i, palette(i + 1u), v);
    else                   body_add_random_convex(p, 25.0f, palette(i + 2u), v);
  }
  for (uint32_t i = 0; i < 4u; ++i) {
    body_add_box(lum_vec2_create(-40.0f + (float)i * 70.0f, 300.0f), lum_vec2_create(30.0f, 14.0f),
                 frand_range(0.0f, 6.28f), palette(i + 3u), lum_vec2_create(0.0f, -30.0f));
  }
}

static void scene_build_zerog(void) {
  body_add_static_rect(lum_vec2_create(0.0f, -300.0f), lum_vec2_create(540.0f, 22.0f), 0.0f, kWallColor);
  body_add_static_rect(lum_vec2_create(0.0f,  300.0f), lum_vec2_create(540.0f, 22.0f), 0.0f, kWallColor);
  body_add_static_rect(lum_vec2_create(-520.0f, 0.0f), lum_vec2_create(22.0f, 322.0f), 0.0f, kWallColor);
  body_add_static_rect(lum_vec2_create( 520.0f, 0.0f), lum_vec2_create(22.0f, 322.0f), 0.0f, kWallColor);

  for (uint32_t i = 0; i < 22u; ++i) {
    lum_Vec2 p = lum_vec2_create(frand_range(-420.0f, 420.0f), frand_range(-210.0f, 210.0f));
    lum_Vec2 v = lum_vec2_create(frand_range(-300.0f, 300.0f), frand_range(-300.0f, 300.0f));
    Body* b = NULL;
    switch (i % 3u) {
      case 0u: b = body_add_circle(p, frand_range(14.0f, 26.0f), palette(i), v); break;
      case 1u: b = body_add_regular(irand_range(3u, 7u), frand_range(18.0f, 30.0f), p, frand_range(0.0f, 6.28f), palette(i + 1u), v); break;
      default: b = body_add_random_convex(p, frand_range(20.0f, 30.0f), palette(i + 2u), v); break;
    }
    if (b) b->ang_vel = frand_range(-3.5f, 3.5f);
  }
}

static void scene_build_stress(void) {
  scene_add_arena();
  for (uint32_t i = 0; i < 64u; ++i) {
    lum_Vec2 p = lum_vec2_create(frand_range(-440.0f, 440.0f), 40.0f + (float)(i / 8u) * 74.0f);
    Body* b;
    if (i % 4u == 0u)      b = body_add_circle(p, frand_range(10.0f, 16.0f), palette(i), lum_vec2_0());
    else if (i % 4u == 1u) b = body_add_regular(irand_range(3u, 6u), frand_range(12.0f, 20.0f), p, 0.0f, palette(i + 1u), lum_vec2_0());
    else                   b = body_add_random_convex(p, frand_range(11.0f, 18.0f), palette(i + 2u), lum_vec2_0());
    if (b) b->ang_vel = frand_range(-2.5f, 2.5f);
  }
}

static void scene_build(Demo_mode mode) {
  g_mode = mode;
  scene_clear();
  switch (mode) {
    case Mode_Slope:  scene_build_slope();  break;
    case Mode_ZeroG:  scene_build_zerog();  break;
    case Mode_Stress: scene_build_stress(); break;
    case Mode_Stack:
    default:          scene_build_stack();  break;
  }
  for (uint32_t i = 0; i < g_body_count; ++i) body_update_world(&g_bodies[i]);
}

/* ---------------- 绘制 ---------------- */

static void draw_text(const char* text, float sx, float sy, lum_Vec4 color) {
  if (!g_font) return;
  lum_tr_draw_layer(g_font, text, lum_app_screen_to_world(lum_vec2_create(sx, sy)), Layer_Hud, color);
}

static void draw_body(const Body* b, int32_t fill_layer) {
  float glow = b->flash;
  lum_Vec4 fill = b->color;
  lum_Vec4 line = lum_vec4_create(lum_minf(fill.x + glow * 0.7f, 1.0f),
                                  lum_minf(fill.y + glow * 0.7f, 1.0f),
                                  lum_minf(fill.z + glow * 0.7f, 1.0f), 1.0f);
  if (b->is_static) {
    fill = lum_vec4_create(b->color.x, b->color.y, b->color.z, 1.0f);
    line = lum_vec4_create(0.40f, 0.45f, 0.56f, 1.0f);
  }

  if (b->kind == Body_Circle) {
    /* 轮廓与填充必须同层相邻：渲染器同层内还会按 primitive 重排 */
    lum_renderer2d_draw_circle(b->pos, b->radius + 2.0f, fill_layer, line);
    lum_renderer2d_draw_circle(b->pos, b->radius, fill_layer, fill);
    /* 半径指示线：让圆的旋转可见 */
    lum_Vec2 tip = lum_vec2_add(b->pos, lum_vec2_scale(lum_vec2_create(cosf(b->angle), sinf(b->angle)), b->radius));
    lum_renderer2d_draw_line(b->pos, tip, 1.5f, Layer_Outline, lum_vec4_create(1.0f, 1.0f, 1.0f, 0.4f));
    return;
  }

  if (b->world_count >= 3u) {
    lum_renderer2d_draw_polygon_convex(b->world, b->world_count, fill_layer, fill);
    for (uint32_t i = 0; i < b->world_count; ++i) {
      lum_renderer2d_draw_line(b->world[i], b->world[(i + 1u) % b->world_count], 2.0f,
                               Layer_Outline, line);
    }
  }
}

static void draw_debug_overlays(void) {
  if (g_show_aabb) {
    lum_Vec4 box_color = lum_vec4_create(0.30f, 0.58f, 0.88f, 0.60f);
    for (uint32_t i = 0; i < g_body_count; ++i) {
      lum_Shape s = body_shape(&g_bodies[i]);
      lum_Vec2 mn, mx;
      lum_shape_bounds(&s, &mn, &mx);
      lum_Vec2 p0 = lum_vec2_create(mn.x, mn.y), p1 = lum_vec2_create(mx.x, mn.y);
      lum_Vec2 p2 = lum_vec2_create(mx.x, mx.y), p3 = lum_vec2_create(mn.x, mx.y);
      lum_renderer2d_draw_line(p0, p1, 1.0f, Layer_Aabb, box_color);
      lum_renderer2d_draw_line(p1, p2, 1.0f, Layer_Aabb, box_color);
      lum_renderer2d_draw_line(p2, p3, 1.0f, Layer_Aabb, box_color);
      lum_renderer2d_draw_line(p3, p0, 1.0f, Layer_Aabb, box_color);
    }
  }

  if (g_show_contact) {
    lum_Vec4 n_color = lum_vec4_create(1.00f, 0.35f, 0.35f, 0.95f);
    lum_Vec4 p_color = lum_vec4_create(1.00f, 0.95f, 0.45f, 0.95f);
    for (uint32_t i = 0; i < g_contact_count; ++i) {
      const Contact_dbg* c = &g_contacts[i];
      float len = 10.0f + c->depth * 2.5f;
      lum_renderer2d_draw_line(c->point, lum_vec2_add(c->point, lum_vec2_scale(c->normal, len)),
                               2.0f, Layer_Contact, n_color);
      lum_renderer2d_draw_circle(c->point, 2.5f, Layer_Contact, p_color);
    }
  }
}

/* ---------------- 阶段系统 ---------------- */

static void sys_start(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ecs; (void)ctx; (void)user_data;
  lum_renderer2d_set_clear_color(lum_vec4_create(0.055f, 0.060f, 0.080f, 1.0f));
  g_font = lum_assets_font("resources/FiraCode-Regular.ttf", 15);
  if (!g_font) Lum_Log_Warn("collision_demo: font load failed, HUD disabled.");
  scene_build(Mode_Stack);
}

/* 输入与积分放 Input 阶段：本 demo 不依赖 ECS 的 world 矩阵 */
static void sys_input(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ecs; (void)user_data;
  float dt = ctx->dt;

  if (lum_input_is_key_pressed(Lum_Key_Q))     lum_app_quit();
  if (lum_input_is_key_pressed(Lum_Key_Space)) scene_build((Demo_mode)((g_mode + 1) % Mode_Count));
  if (lum_input_is_key_pressed(Lum_Key_R))     scene_build(g_mode);
  if (lum_input_is_key_pressed(Lum_Key_A))     g_show_aabb = !g_show_aabb;
  if (lum_input_is_key_pressed(Lum_Key_N))     g_show_contact = !g_show_contact;
  if (lum_input_is_key_pressed(Lum_Key_G))     g_gravity_on = !g_gravity_on;

  if (lum_input_is_mouse_pressed(Lum_Mouse_Button_Left)) {
    lum_Vec2 p = lum_app_screen_to_world(lum_input_get_mouse_position());
    Body* b = NULL;
    switch (irand_range(0u, 2u)) {
      case 0u: b = body_add_circle(p, frand_range(14.0f, 26.0f), palette(g_body_count), lum_vec2_0()); break;
      case 1u: b = body_add_regular(irand_range(3u, 7u), frand_range(18.0f, 30.0f), p, frand_range(0.0f, 6.28f), palette(g_body_count + 1u), lum_vec2_0()); break;
      default: b = body_add_random_convex(p, frand_range(20.0f, 32.0f), palette(g_body_count + 2u), lum_vec2_0()); break;
    }
    if (b) { b->ang_vel = frand_range(-4.0f, 4.0f); body_update_world(b); }
  }

  /* 固定步长积分：物理表现与帧率解耦 */
  if (dt > 1.0f / 30.0f) dt = 1.0f / 30.0f;

  /* 每帧重置统计与接触记录；physics_step 内部只累加 */
  g_stat_pairs = g_stat_broad_pass = g_stat_contacts = g_stat_solver_tests = 0;
  g_contact_count = 0;

  g_accumulator += dt;
  int steps = 0;
  while (g_accumulator >= Fixed_Step && steps < Max_Steps_Per_Frame) {
    physics_step(Fixed_Step);
    g_accumulator -= Fixed_Step;
    ++steps;
  }
}

/* 世界内容画在 Scene 阶段（无视图时每帧一次，沿用 app 相机的 VP） */
static void sys_scene(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ecs; (void)ctx; (void)user_data;
  lum_renderer2d_set_blend_mode(Lum_Blend_Alpha);

  /* 静态几何在后景层，动态体在前景层（数组前部是静态体） */
  for (uint32_t i = 0; i < g_body_count; ++i) {
    draw_body(&g_bodies[i], g_bodies[i].is_static ? Layer_Static_Fill : Layer_Dynamic_Fill);
  }
  draw_debug_overlays();
}

/* HUD 画在 Screen 阶段：像素空间、左上原点 */
static void sys_screen(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ecs; (void)ctx; (void)user_data;

  lum_Vec4 white = lum_vec4_create(0.92f, 0.94f, 0.98f, 1.0f);
  lum_Vec4 dim   = lum_vec4_create(0.60f, 0.65f, 0.74f, 1.0f);
  lum_Vec4 hot   = lum_vec4_create(1.00f, 0.78f, 0.35f, 1.0f);
  char line[192];

  draw_text("collision demo - lum_collision (pure geometry layer)", 18.0f, 28.0f, white);

  snprintf(line, sizeof(line), "mode %-7s  bodies %u  contacts %u  contacts shown %u",
           kModeName[g_mode], (unsigned)g_body_count, (unsigned)g_stat_contacts, (unsigned)g_contact_count);
  draw_text(line, 18.0f, 50.0f, hot);

  snprintf(line, sizeof(line), "wide phase: pairs %u -> passed %u (AABB rejected %u)",
           (unsigned)g_stat_pairs, (unsigned)g_stat_broad_pass,
           (unsigned)(g_stat_pairs - g_stat_broad_pass));
  draw_text(line, 18.0f, 72.0f, dim);

  snprintf(line, sizeof(line), "narrow phase: SAT / closest-point hits %u   solver tests/step %u   draw calls %u",
           (unsigned)g_stat_contacts, (unsigned)g_stat_solver_tests, (unsigned)lum_renderer2d_get_draw_call_count());
  draw_text(line, 18.0f, 94.0f, dim);

  snprintf(line, sizeof(line), "[SPACE] mode  [R] reset  [G] gravity %s  [A] aabb %s  [N] contact %s  [LMB] spawn  [Q] quit",
           g_gravity_on ? "on" : "off", g_show_aabb ? "on" : "off", g_show_contact ? "on" : "off");
  draw_text(line, 18.0f, 116.0f, dim);
}

int main(void) {
  lum_App_desc desc;
  lum_app_init_desc(&desc);
  desc.log_level = Lum_Log_Level_Info;
  desc.title = "Luminous Collision Demo";
  desc.width = 1120;
  desc.height = 680;

  /* 全部编排都在阶段上：没有回调，只有注册 */
  lum_app_add_system(Lum_Stage_Start,  0, sys_start,  NULL);
  lum_app_add_system(Lum_Stage_Input,  0, sys_input,  NULL);
  lum_app_add_system(Lum_Stage_Scene,  0, sys_scene,  NULL);
  lum_app_add_system(Lum_Stage_Screen, 0, sys_screen, NULL);

  lum_app_run(&desc);
  return 0;
}

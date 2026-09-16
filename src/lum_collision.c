#include "lum_collision.h"
#include "lum_assert.h"
#include <float.h>
#include <math.h>
#include <string.h>

/* 判定阈值：同时用于"退化边""零长度向量""点在边上"等场景 */
#define Lum_Collision_Epsilon 1e-6f

static float lum__cross(lum_Vec2 a, lum_Vec2 b) { return a.x * b.y - a.y * b.x; }

static lum_Vec2 lum__perp(lum_Vec2 v) { return lum_vec2_create(-v.y, v.x); }

static lum_Vec2 lum__normalise_or(lum_Vec2 v, lum_Vec2 fallback) {
  float len = lum_vec2_len(v);
  if (len <= Lum_Collision_Epsilon) return fallback;
  return lum_vec2_scale(v, 1.0f / len);
}

static lum_Vec2 lum__midpoint(lum_Vec2 a, lum_Vec2 b) {
  return lum_vec2_scale(lum_vec2_add(a, b), 0.5f);
}

static void lum__contact_reset(lum_Contact* c) {
  if (!c) return;
  memset(c, 0, sizeof(*c));
}

static bool lum__contact_flip_normal(lum_Contact* c) {
  if (c) c->normal = lum_vec2_scale(c->normal, -1.0f);
  return true;
}

/* 线段 p0→p1 与 q0→q1 求交；t 为第一段上的参数 [0,1]。平行/共线按未命中处理 */
static int lum__segment_intersect(lum_Vec2 p0, lum_Vec2 p1, lum_Vec2 q0, lum_Vec2 q1,
                                  float* out_t, lum_Vec2* out_point) {
  lum_Vec2 r = lum_vec2_sub(p1, p0);
  lum_Vec2 s = lum_vec2_sub(q1, q0);
  float denom = lum__cross(r, s);
  if (fabsf(denom) <= Lum_Collision_Epsilon) return 0;

  lum_Vec2 qp = lum_vec2_sub(q0, p0);
  float t = lum__cross(qp, s) / denom;
  float u = lum__cross(qp, r) / denom;
  if (t < 0.0f || t > 1.0f || u < 0.0f || u > 1.0f) return 0;

  if (out_t) *out_t = t;
  if (out_point) *out_point = lum_vec2_add(p0, lum_vec2_scale(r, t));
  return 1;
}

/* 射线 o + t*d 与线段 q0→q1 求交；d 必须已归一化，故 t 即世界距离 */
static int lum__ray_segment(lum_Vec2 o, lum_Vec2 d, lum_Vec2 q0, lum_Vec2 q1, float max_t, float* out_t) {
  lum_Vec2 s = lum_vec2_sub(q1, q0);
  float denom = lum__cross(d, s);
  if (fabsf(denom) <= Lum_Collision_Epsilon) return 0;

  lum_Vec2 qp = lum_vec2_sub(q0, o);
  float t = lum__cross(qp, s) / denom;
  float u = lum__cross(qp, d) / denom;
  if (t < 0.0f || t > max_t || u < 0.0f || u > 1.0f) return 0;

  if (out_t) *out_t = t;
  return 1;
}

/* 边法线，方向调整为"朝向来向"（与 dir 反向） */
static lum_Vec2 lum__edge_normal_facing(lum_Vec2 e0, lum_Vec2 e1, lum_Vec2 dir) {
  lum_Vec2 n = lum__normalise_or(lum__perp(lum_vec2_sub(e1, e0)), lum_vec2_create(1.0f, 0.0f));
  if (lum_vec2_dot(n, dir) > 0.0f) n = lum_vec2_scale(n, -1.0f);
  return n;
}

static lum_Vec2 lum__closest_point_on_segment(lum_Vec2 p, lum_Vec2 a, lum_Vec2 b) {
  lum_Vec2 ab = lum_vec2_sub(b, a);
  float len2 = lum_vec2_len_square(ab);
  if (len2 <= Lum_Collision_Epsilon) return a;
  float t = lum_clamp(lum_vec2_dot(lum_vec2_sub(p, a), ab) / len2, 0.0f, 1.0f);
  return lum_vec2_add(a, lum_vec2_scale(ab, t));
}

/* 多边形上离 p 最近的点；out_edge 回传所在边下标 */
static lum_Vec2 lum__closest_point_on_polygon(const lum_Polygon* poly, lum_Vec2 p, uint32_t* out_edge) {
  lum_Vec2 best = poly->points[0];
  float best_d2 = FLT_MAX;
  uint32_t best_i = 0;
  for (uint32_t i = 0; i < poly->count; ++i) {
    lum_Vec2 q = lum__closest_point_on_segment(p, poly->points[i], poly->points[(i + 1) % poly->count]);
    float d2 = lum_vec2_distance_square(p, q);
    if (d2 < best_d2) { best_d2 = d2; best = q; best_i = i; }
  }
  if (out_edge) *out_edge = best_i;
  return best;
}

/* ---------- 多边形工具 ---------- */

float lum_polygon_signed_area(const lum_Polygon* poly) {
  Lum_Assert(poly && poly->points);
  if (poly->count < 3) return 0.0f;

  float acc = 0.0f;
  for (uint32_t i = 0; i < poly->count; ++i) {
    acc += lum__cross(poly->points[i], poly->points[(i + 1) % poly->count]);
  }
  return acc * 0.5f;
}

bool lum_polygon_is_convex(const lum_Polygon* poly) {
  Lum_Assert(poly && poly->points);
  if (poly->count < 3) return false;

  int sign = 0;
  for (uint32_t i = 0; i < poly->count; ++i) {
    lum_Vec2 a = poly->points[i];
    lum_Vec2 b = poly->points[(i + 1) % poly->count];
    lum_Vec2 c = poly->points[(i + 2) % poly->count];
    float z = lum__cross(lum_vec2_sub(b, a), lum_vec2_sub(c, b));
    if (fabsf(z) <= Lum_Collision_Epsilon) continue;   /* 共线边容忍 */

    int s = z > 0.0f ? 1 : -1;
    if (sign == 0) sign = s;
    else if (s != sign) return false;
  }
  return sign != 0;   /* 全共线 = 退化，判非凸 */
}

lum_Vec2 lum_polygon_centroid(const lum_Polygon* poly) {
  Lum_Assert(poly && poly->points);
  if (poly->count == 0) return lum_vec2_0();
  if (poly->count < 3) return poly->points[0];

  float area2 = 0.0f, cx = 0.0f, cy = 0.0f;
  for (uint32_t i = 0; i < poly->count; ++i) {
    lum_Vec2 a = poly->points[i];
    lum_Vec2 b = poly->points[(i + 1) % poly->count];
    float f = lum__cross(a, b);
    area2 += f;
    cx += (a.x + b.x) * f;
    cy += (a.y + b.y) * f;
  }

  if (fabsf(area2) <= Lum_Collision_Epsilon) {   /* 退化：顶点平均 */
    lum_Vec2 sum = lum_vec2_0();
    for (uint32_t i = 0; i < poly->count; ++i) sum = lum_vec2_add(sum, poly->points[i]);
    return lum_vec2_scale(sum, 1.0f / (float)poly->count);
  }

  float inv = 1.0f / (3.0f * area2);   /* 1/(6A)，A = area2/2 */
  return lum_vec2_create(cx * inv, cy * inv);
}

void lum_polygon_bounds(const lum_Polygon* poly, lum_Vec2* out_min, lum_Vec2* out_max) {
  Lum_Assert(poly && poly->points);
  if (poly->count == 0) return;

  lum_Vec2 mn = poly->points[0], mx = poly->points[0];
  for (uint32_t i = 1; i < poly->count; ++i) {
    lum_Vec2 p = poly->points[i];
    if (p.x < mn.x) mn.x = p.x;
    if (p.y < mn.y) mn.y = p.y;
    if (p.x > mx.x) mx.x = p.x;
    if (p.y > mx.y) mx.y = p.y;
  }
  if (out_min) *out_min = mn;
  if (out_max) *out_max = mx;
}

uint32_t lum_polygon_transform(const lum_Polygon* poly, lum_Mat4 mat,
                               lum_Vec2* out_points, uint32_t out_capacity) {
  Lum_Assert(poly && poly->points);
  if (!out_points || poly->count == 0 || poly->count > out_capacity) return 0;

  for (uint32_t i = 0; i < poly->count; ++i) {
    lum_Vec4 p = lum_mat4_mul_vec4(mat, lum_vec4_create(poly->points[i].x, poly->points[i].y, 0.0f, 1.0f));
    out_points[i] = lum_vec2_create(p.x, p.y);
  }
  return poly->count;
}

uint32_t lum_polygon_make_box(lum_Vec2 center, lum_Vec2 half_size, float rotation,
                              lum_Vec2* out_points, uint32_t out_capacity) {
  if (!out_points || out_capacity < 4u) return 0;

  float c = cosf(rotation), s = sinf(rotation);
  static const float unit[4][2] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } };
  for (uint32_t i = 0; i < 4u; ++i) {
    float lx = unit[i][0] * half_size.x, ly = unit[i][1] * half_size.y;
    out_points[i] = lum_vec2_create(center.x + lx * c - ly * s, center.y + lx * s + ly * c);
  }
  return 4u;
}

uint32_t lum_polygon_make_regular(uint32_t sides, float radius, float rotation,
                                  lum_Vec2* out_points, uint32_t out_capacity) {
  if (!out_points || sides < 3u || sides > out_capacity) return 0;

  float step = 2.0f * (float)Lum_Pi / (float)sides;
  for (uint32_t i = 0; i < sides; ++i) {
    float a = rotation + (float)i * step;
    out_points[i] = lum_vec2_create(cosf(a) * radius, sinf(a) * radius);
  }
  return sides;
}

/* ---------- 包含判定 ---------- */

bool lum_polygon_contains_point(const lum_Polygon* poly, lum_Vec2 p) {
  Lum_Assert(poly && poly->points);
  if (poly->count < 3) return false;

  int sign = 0;
  for (uint32_t i = 0; i < poly->count; ++i) {
    lum_Vec2 a = poly->points[i];
    lum_Vec2 b = poly->points[(i + 1) % poly->count];
    float z = lum__cross(lum_vec2_sub(b, a), lum_vec2_sub(p, a));
    if (fabsf(z) <= Lum_Collision_Epsilon) continue;   /* 落在边上：算内部 */

    int s = z > 0.0f ? 1 : -1;
    if (sign == 0) sign = s;
    else if (s != sign) return false;
  }
  return true;   /* 凸多边形：所有叉积同号 = 在内 */
}

/* ---------- 多边形 × 多边形：SAT ---------- */

static void lum__project_polygon(const lum_Polygon* poly, lum_Vec2 axis, float* out_min, float* out_max) {
  float mn = lum_vec2_dot(poly->points[0], axis);
  float mx = mn;
  for (uint32_t i = 1; i < poly->count; ++i) {
    float v = lum_vec2_dot(poly->points[i], axis);
    if (v < mn) mn = v;
    if (v > mx) mx = v;
  }
  *out_min = mn;
  *out_max = mx;
}

static lum_Vec2 lum__support_polygon(const lum_Polygon* poly, lum_Vec2 axis) {
  lum_Vec2 best = poly->points[0];
  float best_d = lum_vec2_dot(best, axis);
  for (uint32_t i = 1; i < poly->count; ++i) {
    float d = lum_vec2_dot(poly->points[i], axis);
    if (d > best_d) { best_d = d; best = poly->points[i]; }
  }
  return best;
}

bool lum_collide_polygon_polygon(const lum_Polygon* a, const lum_Polygon* b, lum_Contact* out) {
  Lum_Assert(a && a->points && b && b->points);
  if (a->count < 3 || b->count < 3) return false;

  float best_depth = FLT_MAX;
  lum_Vec2 best_axis = lum_vec2_create(1.0f, 0.0f);

  /* 分离轴 = 双方所有边的法线 */
  for (int pass = 0; pass < 2; ++pass) {
    const lum_Polygon* poly = (pass == 0) ? a : b;
    for (uint32_t i = 0; i < poly->count; ++i) {
      lum_Vec2 edge = lum_vec2_sub(poly->points[(i + 1) % poly->count], poly->points[i]);
      lum_Vec2 axis = lum__perp(edge);
      float len = lum_vec2_len(axis);
      if (len <= Lum_Collision_Epsilon) continue;   /* 退化边 */
      axis = lum_vec2_scale(axis, 1.0f / len);

      float min_a, max_a, min_b, max_b;
      lum__project_polygon(a, axis, &min_a, &max_a);
      lum__project_polygon(b, axis, &min_b, &max_b);

      float overlap = lum_minf(max_a, max_b) - lum_maxf(min_a, min_b);
      if (overlap <= 0.0f) return false;   /* 找到分离轴 */

      if (overlap < best_depth) { best_depth = overlap; best_axis = axis; }
    }
  }

  if (!out) return true;

  /* 定向为 A → B */
  lum_Vec2 delta = lum_vec2_sub(lum_polygon_centroid(b), lum_polygon_centroid(a));
  if (lum_vec2_dot(best_axis, delta) < 0.0f) best_axis = lum_vec2_scale(best_axis, -1.0f);

  out->hit = true;
  out->normal = best_axis;
  out->depth = best_depth;
  /* 接触点取 A 沿 +normal 的支撑点与 B 沿 -normal 的支撑点的中点 */
  out->point = lum__midpoint(lum__support_polygon(a, best_axis),
                             lum__support_polygon(b, lum_vec2_scale(best_axis, -1.0f)));
  return true;
}

/* ---------- 圆 × 多边形 ---------- */

bool lum_collide_circle_polygon(lum_Circle a, const lum_Polygon* b, lum_Contact* out) {
  Lum_Assert(b && b->points);
  if (b->count < 3 || a.radius < 0.0f) return false;

  uint32_t edge = 0;
  lum_Vec2 closest = lum__closest_point_on_polygon(b, a.center, &edge);
  float dist = lum_vec2_distance(a.center, closest);
  bool inside = lum_polygon_contains_point(b, a.center);

  float depth;
  lum_Vec2 push_dir;   /* A（圆）的分离方向 */
  if (inside) {
    depth = a.radius + dist;   /* 推到最近边再穿出半径 */
    push_dir = lum__normalise_or(lum_vec2_sub(closest, a.center), lum_vec2_create(1.0f, 0.0f));
  } else {
    if (dist >= a.radius) return false;   /* 相切不算命中 */
    depth = a.radius - dist;
    push_dir = lum__normalise_or(lum_vec2_sub(a.center, closest), lum_vec2_create(1.0f, 0.0f));
  }

  if (!out) return true;

  /* 圆表面朝多边形一侧的点：在外时背向 push_dir，在内时顺着 push_dir */
  lum_Vec2 surface = inside ? lum_vec2_add(a.center, lum_vec2_scale(push_dir, a.radius))
                            : lum_vec2_sub(a.center, lum_vec2_scale(push_dir, a.radius));
  out->hit = true;
  out->normal = lum_vec2_scale(push_dir, -1.0f);   /* A → B */
  out->depth = depth;
  out->point = lum__midpoint(surface, closest);
  return true;
}

/* ---------- 圆 × 圆 ---------- */

bool lum_collide_circle_circle(lum_Circle a, lum_Circle b, lum_Contact* out) {
  if (a.radius < 0.0f || b.radius < 0.0f) return false;

  float radius_sum = a.radius + b.radius;
  lum_Vec2 delta = lum_vec2_sub(b.center, a.center);
  float dist2 = lum_vec2_len_square(delta);
  if (dist2 >= radius_sum * radius_sum) return false;   /* 相切不算命中 */

  float dist = sqrtf(dist2);
  lum_Vec2 normal = lum__normalise_or(delta, lum_vec2_create(1.0f, 0.0f));   /* 同心：任取 +x */

  if (!out) return true;

  out->hit = true;
  out->normal = normal;
  out->depth = radius_sum - dist;
  out->point = lum__midpoint(lum_vec2_add(a.center, lum_vec2_scale(normal, a.radius)),
                             lum_vec2_sub(b.center, lum_vec2_scale(normal, b.radius)));
  return true;
}

/* ---------- 点 × 多边形 ---------- */

bool lum_collide_point_polygon(lum_Vec2 p, const lum_Polygon* poly, lum_Contact* out) {
  Lum_Assert(poly && poly->points);
  if (!lum_polygon_contains_point(poly, p)) return false;
  if (!out) return true;

  uint32_t edge = 0;
  lum_Vec2 closest = lum__closest_point_on_polygon(poly, p, &edge);
  float dist = lum_vec2_distance(p, closest);
  lum_Vec2 push_dir = lum__normalise_or(lum_vec2_sub(closest, p), lum_vec2_create(1.0f, 0.0f));

  out->hit = true;
  out->normal = lum_vec2_scale(push_dir, -1.0f);   /* A（点）→ B（多边形） */
  out->depth = dist;
  out->point = closest;
  return true;
}

/* ---------- Box 便捷路径（展开为 4 顶点多边形后复用） ---------- */

bool lum_collide_circle_box(lum_Circle a, const lum_Box* b, lum_Contact* out) {
  if (!b) return false;
  lum_Vec2 verts[4];
  if (!lum_polygon_make_box(b->center, b->half_size, b->rotation, verts, 4u)) return false;
  lum_Polygon poly = { verts, 4u };
  return lum_collide_circle_polygon(a, &poly, out);
}

bool lum_collide_box_polygon(const lum_Box* a, const lum_Polygon* b, lum_Contact* out) {
  if (!a) return false;
  lum_Vec2 verts[4];
  if (!lum_polygon_make_box(a->center, a->half_size, a->rotation, verts, 4u)) return false;
  lum_Polygon poly = { verts, 4u };
  return lum_collide_polygon_polygon(&poly, b, out);
}

bool lum_collide_box_box(const lum_Box* a, const lum_Box* b, lum_Contact* out) {
  if (!a || !b) return false;
  lum_Vec2 va[4], vb[4];
  if (!lum_polygon_make_box(a->center, a->half_size, a->rotation, va, 4u)) return false;
  if (!lum_polygon_make_box(b->center, b->half_size, b->rotation, vb, 4u)) return false;
  lum_Polygon pa = { va, 4u }, pb = { vb, 4u };
  return lum_collide_polygon_polygon(&pa, &pb, out);
}

/* ---------- 线段族 ---------- */

bool lum_segment_polygon_overlap(const lum_Segment* seg, const lum_Polygon* poly, lum_Contact* out) {
  Lum_Assert(seg && poly && poly->points);
  if (poly->count < 3) return false;

  float best_t = FLT_MAX;
  lum_Vec2 best_point = seg->start;
  uint32_t best_edge = 0;
  int found = 0;

  for (uint32_t i = 0; i < poly->count; ++i) {
    float t = 0.0f;
    lum_Vec2 pt = seg->start;
    if (lum__segment_intersect(seg->start, seg->end, poly->points[i],
                               poly->points[(i + 1) % poly->count], &t, &pt)) {
      if (!found || t < best_t) { best_t = t; best_point = pt; best_edge = i; found = 1; }
    }
  }

  if (!found) {
    /* 无边界穿越：整段落在多边形内也算重叠 */
    if (!lum_polygon_contains_point(poly, seg->start)) return false;
    best_t = 0.0f;
    best_point = seg->start;
    best_edge = 0;
  }

  if (!out) return true;

  lum_Vec2 dir = lum_vec2_sub(seg->end, seg->start);
  out->hit = true;
  out->depth = 0.0f;   /* 线段无厚度 */
  out->point = best_point;
  out->normal = lum__edge_normal_facing(poly->points[best_edge],
                                        poly->points[(best_edge + 1) % poly->count], dir);
  return true;
}

bool lum_segment_circle_overlap(const lum_Segment* seg, lum_Circle c, lum_Contact* out) {
  Lum_Assert(seg);
  if (c.radius < 0.0f) return false;

  lum_Vec2 closest = lum__closest_point_on_segment(c.center, seg->start, seg->end);
  float dist = lum_vec2_distance(c.center, closest);
  if (dist >= c.radius) return false;   /* 相切不算命中 */

  if (!out) return true;

  lum_Vec2 seg_dir = lum__normalise_or(lum_vec2_sub(seg->end, seg->start), lum_vec2_create(1.0f, 0.0f));

  out->hit = true;
  out->depth = 0.0f;
  out->point = closest;
  /* 与多边形边法线同一语义：指向线段一侧（即把线段推离圆的分离方向） */
  out->normal = lum__normalise_or(lum_vec2_sub(closest, c.center),
                                  lum_vec2_scale(lum__perp(seg_dir), -1.0f));
  return true;
}

bool lum_segment_segment_overlap(const lum_Segment* a, const lum_Segment* b, lum_Contact* out) {
  Lum_Assert(a && b);

  float t = 0.0f;
  lum_Vec2 pt = a->start;
  if (!lum__segment_intersect(a->start, a->end, b->start, b->end, &t, &pt)) return false;
  if (!out) return true;

  lum_Vec2 dir_a = lum_vec2_sub(a->end, a->start);
  out->hit = true;
  out->depth = 0.0f;
  out->point = pt;
  out->normal = lum__edge_normal_facing(b->start, b->end, dir_a);   /* 以 B 的面朝向来向 */
  return true;
}

/* ---------- 射线 ---------- */

bool lum_raycast_polygon(const lum_Polygon* poly, lum_Vec2 origin, lum_Vec2 dir,
                         float max_distance, lum_Ray_hit* out) {
  Lum_Assert(poly && poly->points);
  if (poly->count < 3 || max_distance < 0.0f) return false;

  lum_Vec2 d = lum__normalise_or(dir, lum_vec2_0());
  if (lum_vec2_len_square(d) <= Lum_Collision_Epsilon) return false;

  float best_t = max_distance;
  uint32_t best_edge = 0;
  int found = 0;
  for (uint32_t i = 0; i < poly->count; ++i) {
    float t = 0.0f;
    if (lum__ray_segment(origin, d, poly->points[i], poly->points[(i + 1) % poly->count], max_distance, &t)) {
      if (!found || t < best_t) { best_t = t; best_edge = i; found = 1; }
    }
  }
  if (!found) return false;

  if (!out) return true;

  /* 命中面外法线：按绕向判定，逆时针多边形的外向法线为 (edge.y, -edge.x) */
  float orient = lum_polygon_signed_area(poly) >= 0.0f ? 1.0f : -1.0f;
  lum_Vec2 edge = lum_vec2_sub(poly->points[(best_edge + 1) % poly->count], poly->points[best_edge]);

  out->hit = true;
  out->distance = best_t;
  out->point = lum_vec2_add(origin, lum_vec2_scale(d, best_t));
  out->normal = lum__normalise_or(lum_vec2_create(edge.y * orient, -edge.x * orient),
                                  lum_vec2_create(1.0f, 0.0f));
  return true;
}

bool lum_raycast_circle(lum_Circle c, lum_Vec2 origin, lum_Vec2 dir,
                        float max_distance, lum_Ray_hit* out) {
  if (c.radius < 0.0f || max_distance < 0.0f) return false;

  lum_Vec2 d = lum__normalise_or(dir, lum_vec2_0());
  if (lum_vec2_len_square(d) <= Lum_Collision_Epsilon) return false;

  lum_Vec2 oc = lum_vec2_sub(origin, c.center);
  float b = lum_vec2_dot(oc, d);
  float k = lum_vec2_len_square(oc) - c.radius * c.radius;
  float disc = b * b - k;
  if (disc < 0.0f) return false;

  float sq = sqrtf(disc);
  float t = -b - sq;
  if (t < 0.0f) t = -b + sq;   /* 起点在圆内：取穿出点 */
  if (t < 0.0f || t > max_distance) return false;

  if (!out) return true;

  out->hit = true;
  out->distance = t;
  out->point = lum_vec2_add(origin, lum_vec2_scale(d, t));
  out->normal = lum__normalise_or(lum_vec2_sub(out->point, c.center), lum_vec2_create(1.0f, 0.0f));
  return true;
}

bool lum_raycast_shape(const lum_Shape* s, lum_Vec2 origin, lum_Vec2 dir,
                       float max_distance, lum_Ray_hit* out) {
  if (!s) return false;

  switch (s->type) {
    case Lum_Shape_Circle:
      return lum_raycast_circle(s->as.circle, origin, dir, max_distance, out);
    case Lum_Shape_Polygon:
      return lum_raycast_polygon(&s->as.polygon, origin, dir, max_distance, out);
    case Lum_Shape_Box: {
      lum_Vec2 verts[4];
      if (!lum_polygon_make_box(s->as.box.center, s->as.box.half_size, s->as.box.rotation, verts, 4u)) return false;
      lum_Polygon poly = { verts, 4u };
      return lum_raycast_polygon(&poly, origin, dir, max_distance, out);
    }
    default:
      return false;   /* 点/线段无体积，不支持射线求交 */
  }
}

/* ---------- 形状构造 ---------- */

lum_Shape lum_shape_point(lum_Vec2 p) {
  lum_Shape s; memset(&s, 0, sizeof(s));
  s.type = Lum_Shape_Point;
  s.as.point = p;
  return s;
}

lum_Shape lum_shape_circle(lum_Vec2 center, float radius) {
  lum_Shape s; memset(&s, 0, sizeof(s));
  s.type = Lum_Shape_Circle;
  s.as.circle.center = center;
  s.as.circle.radius = radius;
  return s;
}

lum_Shape lum_shape_box(lum_Vec2 center, lum_Vec2 half_size, float rotation) {
  lum_Shape s; memset(&s, 0, sizeof(s));
  s.type = Lum_Shape_Box;
  s.as.box.center = center;
  s.as.box.half_size = half_size;
  s.as.box.rotation = rotation;
  return s;
}

lum_Shape lum_shape_polygon(const lum_Vec2* points, uint32_t count) {
  lum_Shape s; memset(&s, 0, sizeof(s));
  s.type = Lum_Shape_Polygon;
  s.as.polygon.points = points;
  s.as.polygon.count = count;
  return s;
}

lum_Shape lum_shape_segment(lum_Vec2 start, lum_Vec2 end) {
  lum_Shape s; memset(&s, 0, sizeof(s));
  s.type = Lum_Shape_Segment;
  s.as.segment.start = start;
  s.as.segment.end = end;
  return s;
}

/* ---------- 宽相 ---------- */

void lum_shape_bounds(const lum_Shape* s, lum_Vec2* out_min, lum_Vec2* out_max) {
  Lum_Assert(s);
  if (!out_min || !out_max) return;

  switch (s->type) {
    case Lum_Shape_Point:
      *out_min = s->as.point;
      *out_max = s->as.point;
      return;
    case Lum_Shape_Circle:
      *out_min = lum_vec2_create(s->as.circle.center.x - s->as.circle.radius,
                                 s->as.circle.center.y - s->as.circle.radius);
      *out_max = lum_vec2_create(s->as.circle.center.x + s->as.circle.radius,
                                 s->as.circle.center.y + s->as.circle.radius);
      return;
    case Lum_Shape_Box: {
      lum_Vec2 verts[4];
      if (!lum_polygon_make_box(s->as.box.center, s->as.box.half_size, s->as.box.rotation, verts, 4u)) return;
      lum_Polygon poly = { verts, 4u };
      lum_polygon_bounds(&poly, out_min, out_max);
      return;
    }
    case Lum_Shape_Polygon:
      if (s->as.polygon.count == 0) return;
      lum_polygon_bounds(&s->as.polygon, out_min, out_max);
      return;
    case Lum_Shape_Segment:
      *out_min = lum_vec2_create(lum_minf(s->as.segment.start.x, s->as.segment.end.x),
                                 lum_minf(s->as.segment.start.y, s->as.segment.end.y));
      *out_max = lum_vec2_create(lum_maxf(s->as.segment.start.x, s->as.segment.end.x),
                                 lum_maxf(s->as.segment.start.y, s->as.segment.end.y));
      return;
    default:
      return;
  }
}

/* ---------- 通用分发 ---------- */

/* 归一化后的形状：Point → 半径 0 的圆；Box → 4 顶点多边形（借 scratch 存储） */
typedef enum lum__Coll_kind {
  lum__Coll_Circle = 0,
  lum__Coll_Poly,
  lum__Coll_Segment,
} lum__Coll_kind;

typedef struct lum__Coll_shape {
  lum__Coll_kind kind;
  lum_Circle     circle;
  lum_Polygon    poly;
  lum_Segment    segment;
  lum_Vec2       scratch[4];
} lum__Coll_shape;

static bool lum__coll_prepare(const lum_Shape* s, lum__Coll_shape* out) {
  memset(out, 0, sizeof(*out));

  switch (s->type) {
    case Lum_Shape_Point:
      out->kind = lum__Coll_Circle;
      out->circle.center = s->as.point;
      out->circle.radius = 0.0f;
      return true;
    case Lum_Shape_Circle:
      out->kind = lum__Coll_Circle;
      out->circle = s->as.circle;
      return true;
    case Lum_Shape_Box: {
      uint32_t n = lum_polygon_make_box(s->as.box.center, s->as.box.half_size, s->as.box.rotation,
                                        out->scratch, 4u);
      if (!n) return false;
      out->kind = lum__Coll_Poly;
      out->poly.points = out->scratch;
      out->poly.count = n;
      return true;
    }
    case Lum_Shape_Polygon:
      out->kind = lum__Coll_Poly;
      out->poly = s->as.polygon;
      return true;
    case Lum_Shape_Segment:
      out->kind = lum__Coll_Segment;
      out->segment = s->as.segment;
      return true;
    default:
      return false;
  }
}

bool lum_collide(const lum_Shape* a, const lum_Shape* b, lum_Contact* out_contact) {
  if (!a || !b) return false;

  lum__Coll_shape ca, cb;
  if (!lum__coll_prepare(a, &ca) || !lum__coll_prepare(b, &cb)) return false;

  /* 无接触结果请求时，各具体实现内部会跳过接触计算 */
  lum_Contact local;
  lum_Contact* c = out_contact ? out_contact : &local;
  lum__contact_reset(c);

  bool want = (out_contact != NULL);

  switch (ca.kind) {
    case lum__Coll_Circle:
      switch (cb.kind) {
        case lum__Coll_Circle:  return lum_collide_circle_circle(ca.circle, cb.circle, want ? c : NULL);
        case lum__Coll_Poly:    return lum_collide_circle_polygon(ca.circle, &cb.poly, want ? c : NULL);
        case lum__Coll_Segment: {
          bool hit = lum_segment_circle_overlap(&cb.segment, ca.circle, want ? c : NULL);
          return (hit && want) ? lum__contact_flip_normal(c) : hit;   /* 交换了 A/B */
        }
      }
      break;

    case lum__Coll_Poly:
      switch (cb.kind) {
        case lum__Coll_Circle: {
          bool hit = lum_collide_circle_polygon(cb.circle, &ca.poly, want ? c : NULL);
          return (hit && want) ? lum__contact_flip_normal(c) : hit;
        }
        case lum__Coll_Poly:    return lum_collide_polygon_polygon(&ca.poly, &cb.poly, want ? c : NULL);
        case lum__Coll_Segment: {
          bool hit = lum_segment_polygon_overlap(&cb.segment, &ca.poly, want ? c : NULL);
          return (hit && want) ? lum__contact_flip_normal(c) : hit;
        }
      }
      break;

    case lum__Coll_Segment:
      switch (cb.kind) {
        case lum__Coll_Circle:  return lum_segment_circle_overlap(&ca.segment, cb.circle, want ? c : NULL);
        case lum__Coll_Poly:    return lum_segment_polygon_overlap(&ca.segment, &cb.poly, want ? c : NULL);
        case lum__Coll_Segment: return lum_segment_segment_overlap(&ca.segment, &cb.segment, want ? c : NULL);
      }
      break;
  }
  return false;
}

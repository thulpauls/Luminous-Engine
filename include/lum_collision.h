#ifndef Lum_Collision_H
#define Lum_Collision_H

#include "glimmer/lum_math2d.h"
#include <stdint.h>
#include <stdbool.h>

/*
* 2D 碰撞检测（纯几何层）：无 GL、无 ECS 依赖，可独立单测。
*
* 形状集合：点 / 圆 / 有向矩形(OBB) / 凸多边形 / 线段。
*
* 约定：
*   - 只支持【凸】多边形。SAT 对非凸会给出错误结果而非报错，入口用
*     lum_polygon_is_convex 自行校验（本层不隐式校验，避免热路径开销）。
*   - 返回 1/0 表示是否相交；接触信息写入 out_contact —— 允许传 NULL，
*     只要布尔结果时跳过接触计算。
*   - 相切（穿透深度恰为 0）一律不算命中，各函数语义一致；
*     例外是 lum_polygon_contains_point：边界算内部（包含判定的常规约定）。
*   - normal 方向统一为 A → B：分离时把 A 沿 -normal、B 沿 +normal 各推 depth。
*   - 例外：线段（Segment）无内外之分，其参与的组合 depth 恒为 0，normal 语义改为
*     "指向线段一侧"（即把线段推离另一形状的分离方向，等价于命中面的外向法线）。
*     A/B 顺序只影响该法线是否取反（A 为线段时直接给出，B 为线段时取反）。
*   - 全程不分配内存、不改动入参；顶点缓冲区由调用方提供（栈数组即可）。
*   - 所有输出参数在返回 0（未命中）时行为未定义，调用方应先看返回值。
*/

typedef struct lum_Polygon {
  const lum_Vec2* points;   /* 顶点视图，不拥有内存；顺/逆时针均可，仅凸 */
  uint32_t        count;    /* >= 3 */
} lum_Polygon;

typedef struct lum_Circle  { lum_Vec2 center; float radius; } lum_Circle;
typedef struct lum_Box     { lum_Vec2 center; lum_Vec2 half_size; float rotation; } lum_Box;
typedef struct lum_Segment { lum_Vec2 start, end; } lum_Segment;

typedef enum lum_Shape_type {
  Lum_Shape_Point = 0,
  Lum_Shape_Circle,
  Lum_Shape_Box,
  Lum_Shape_Polygon,
  Lum_Shape_Segment,
} lum_Shape_type;

/* 标签联合：既提供单一通用入口，也保留各具体路径供热路径直调 */
typedef struct lum_Shape {
  lum_Shape_type type;
  union {
    lum_Vec2    point;
    lum_Circle  circle;
    lum_Box     box;
    lum_Polygon polygon;
    lum_Segment segment;
  } as;
} lum_Shape;

/* 接触信息：足以实现分离（去穿透）与反弹 */
typedef struct lum_Contact {
  bool     hit;
  lum_Vec2 normal;   /* 单位法线，方向 A → B；分离时 A 沿 -normal、B 沿 +normal 推 depth */
  float    depth;    /* 穿透深度 >= 0（最小平移量 MTV 的长度） */
  lum_Vec2 point;    /* 接触点（两形状相对表面的中点，近似） */
} lum_Contact;

/* 射线命中 */
typedef struct lum_Ray_hit {
  bool     hit;
  float    distance;  /* 沿 dir 的世界距离（dir 无需归一化，内部会归一） */
  lum_Vec2 point;     /* 命中点 */
  lum_Vec2 normal;    /* 命中面外法线（多边形按绕向判定；圆为径向朝外） */
} lum_Ray_hit;

/* ---------- 形状构造 ---------- */

lum_Shape lum_shape_point  (lum_Vec2 p);
lum_Shape lum_shape_circle (lum_Vec2 center, float radius);
lum_Shape lum_shape_box    (lum_Vec2 center, lum_Vec2 half_size, float rotation);
lum_Shape lum_shape_polygon(const lum_Vec2* points, uint32_t count);
lum_Shape lum_shape_segment(lum_Vec2 start, lum_Vec2 end);

/* ---------- 多边形工具：建形 / 校验 / 变换 ---------- */

float    lum_polygon_signed_area(const lum_Polygon* poly);   /* 符号面积：>0 逆时针，<0 顺时针 */
bool     lum_polygon_is_convex   (const lum_Polygon* poly);  /* 共线边容忍；全共线判非凸 */
lum_Vec2 lum_polygon_centroid    (const lum_Polygon* poly);  /* 面积加权；退化时用顶点平均 */
void     lum_polygon_bounds      (const lum_Polygon* poly, lum_Vec2* out_min, lum_Vec2* out_max);

/* 本地多边形过 world 矩阵写入调用方缓冲（世界空间窄相用）；容量不足返回 0 */
uint32_t lum_polygon_transform(const lum_Polygon* poly, lum_Mat4 mat,
                               lum_Vec2* out_points, uint32_t out_capacity);

/* 常用凸形生成（逆时针）；容量不足返回 0 */
uint32_t lum_polygon_make_box    (lum_Vec2 center, lum_Vec2 half_size, float rotation,
                                  lum_Vec2* out_points, uint32_t out_capacity);
uint32_t lum_polygon_make_regular(uint32_t sides, float radius, float rotation,
                                  lum_Vec2* out_points, uint32_t out_capacity);   /* 以原点为中心 */

/* ---------- 相交检测 ---------- */

/* 通用入口：内部按 (type, type) 分发；Box 展开为 4 顶点多边形，Point 视作半径 0 的圆 */
bool lum_collide(const lum_Shape* a, const lum_Shape* b, lum_Contact* out_contact);

/* 具体路径（热路径直调，省一次分发）。多边形类走 SAT，圆走最近点，Box 复用多边形 */
bool lum_collide_polygon_polygon(const lum_Polygon* a, const lum_Polygon* b, lum_Contact* out);
bool lum_collide_circle_polygon (lum_Circle a, const lum_Polygon* b,  lum_Contact* out);
bool lum_collide_circle_circle  (lum_Circle a, lum_Circle b,          lum_Contact* out);
bool lum_collide_circle_box     (lum_Circle a, const lum_Box* b,      lum_Contact* out);
bool lum_collide_box_polygon    (const lum_Box* a, const lum_Polygon* b, lum_Contact* out);
bool lum_collide_box_box        (const lum_Box* a, const lum_Box* b,  lum_Contact* out);
bool lum_collide_point_polygon  (lum_Vec2 p, const lum_Polygon* poly, lum_Contact* out);

/* 线段族：depth 恒为 0，normal 为命中面外向法线（指向线段一侧） */
bool lum_segment_polygon_overlap(const lum_Segment* seg, const lum_Polygon* poly, lum_Contact* out);
bool lum_segment_circle_overlap (const lum_Segment* seg, lum_Circle c,              lum_Contact* out);
bool lum_segment_segment_overlap(const lum_Segment* a,   const lum_Segment* b,      lum_Contact* out);

/* ---------- 查询 ---------- */

bool lum_polygon_contains_point(const lum_Polygon* poly, lum_Vec2 p);

/* 射线首次与边界相交（起点在形状内时返回穿出点）；超出 max_distance 视为未命中 */
bool lum_raycast_polygon(const lum_Polygon* poly, lum_Vec2 origin, lum_Vec2 dir,
                         float max_distance, lum_Ray_hit* out);
bool lum_raycast_circle (lum_Circle c,            lum_Vec2 origin, lum_Vec2 dir,
                         float max_distance, lum_Ray_hit* out);
bool lum_raycast_shape  (const lum_Shape* s,      lum_Vec2 origin, lum_Vec2 dir,
                         float max_distance, lum_Ray_hit* out);   /* 仅支持 圆/Box/多边形 */

/* ---------- 宽相 ---------- */

void lum_shape_bounds(const lum_Shape* s, lum_Vec2* out_min, lum_Vec2* out_max);

#endif //Lum_Collision_H

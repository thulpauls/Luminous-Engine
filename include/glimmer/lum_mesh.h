#ifndef Lum_Mesh_H
#define Lum_Mesh_H

#include "glimmer/lum_math2d.h"
#include <stdbool.h>
#include <stdint.h>

/*
* 3D 网格（glimmer 的 3D 渲染器用）。
*
* 顶点格式在 M3 从"位置 + 法线"扩展为"位置 + 法线 + uv"（24 → 32 字节）。
* **布局仍是单一的固定格式**：M3 只有这一种格式，为它写一套"顶点布局描述符"等于为一个
* 还不存在的需求付出抽象成本。等 M8 载入 OBJ（可能有/没有法线、有/没有 uv）真的出现
* 第二种格式时再做描述符 —— 那时也只需要改 lum_mesh_create 里的属性设置这一处。
*
* 刻意把几何生成与 GPU 上传【分成两半】：
*   · 纯 CPU 的一半（build_box / bounds）不碰 GL，可以 headless 断言 ——
*     glimmer_check 就是靠这个在没有窗口的情况下验证顶点数、索引范围、法线、面朝向与 uv；
*   · 碰 GL 的一半（create / destroy / bind）只做 VAO/VBO/EBO 的搬运与属性设置。
* 这条分法同时也是"渲染器要能脱离窗口被测试"的延续：能算的都别放进 GL 里。
*/

typedef struct lum_Vertex3d {
  lum_Vec3 position;
  lum_Vec3 normal;
  lum_Vec2 uv;                        /* M3 起有纹理坐标：每个面拿到一张完整的 0..1 贴图 */
} lum_Vertex3d;                     /* 32 字节；改这里要同步改 lum_mesh_create 的属性设置与 glimmer_check 的偏移断言 */

typedef struct lum_Mesh {
  bool     initialized;
  uint32_t vao, vbo, ebo;
  uint32_t vertex_count, index_count;
  lum_Vec3 bounds_min, bounds_max;  /* 由顶点算出，M4 的视锥剔除直接用 */
} lum_Mesh;

/* 立方体：6 面 × 4 顶点（每面独立法线，才能平直着色），36 个索引 */
#define Lum_Mesh_Box_Vertex_Count 24u
#define Lum_Mesh_Box_Index_Count  36u

/* 生成以原点为中心、半长 half_extents 的立方体（纯 CPU，不碰 GL） */
void lum_mesh_build_box(lum_Vertex3d* vertices, uint32_t* indices, lum_Vec3 half_extents);

/* 顶点包围盒（纯 CPU）。count == 0 时输出全零 */
void lum_mesh_bounds(const lum_Vertex3d* vertices, uint32_t count, lum_Vec3* out_min, lum_Vec3* out_max);

/* 上传到 GPU。任何一步失败都清干净并返回 0（不留半个 GPU 资源）；成功返回 1 */
int  lum_mesh_create(lum_Mesh* mesh, const lum_Vertex3d* vertices, uint32_t vertex_count,
                     const uint32_t* indices, uint32_t index_count);
int  lum_mesh_create_box(lum_Mesh* mesh, lum_Vec3 half_extents);
void lum_mesh_destroy(lum_Mesh* mesh);

void lum_mesh_bind(const lum_Mesh* mesh);

uint32_t lum_mesh_vertex_count(const lum_Mesh* mesh);
uint32_t lum_mesh_index_count(const lum_Mesh* mesh);
void     lum_mesh_get_bounds(const lum_Mesh* mesh, lum_Vec3* out_min, lum_Vec3* out_max);

#endif //Lum_Mesh_H

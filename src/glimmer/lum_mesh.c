#include "glimmer/lum_mesh.h"
#include "glimmer/lum_log.h"
#include "glimmer/lum_assert.h"

#include <glad/glad.h>
#include <stddef.h>

/*
* 立方体的 6 个面。每面 4 个顶点、法线是常量（平直着色），
* 所以顶点数 24 而不是 8 —— 8 个共享顶点会让法线在面上被插值成球面。
* 顶点/索引顺序都按逆时针（CCW）从外侧看，将来开背面剔除（M4）不用改数据。
*/
void lum_mesh_build_box(lum_Vertex3d* vertices, uint32_t* indices, lum_Vec3 half_extents) {
  Lum_Assert(vertices && indices);
  if (!vertices || !indices) return;

  const float hx = half_extents.x, hy = half_extents.y, hz = half_extents.z;

  /* 每面：法线 + 四个角（顺序保证从面外侧看是 CCW） */
  const lum_Vec3 normals[6] = {
    lum_vec3_create( 0.0f,  0.0f,  1.0f),   /* +Z */
    lum_vec3_create( 0.0f,  0.0f, -1.0f),   /* -Z */
    lum_vec3_create( 1.0f,  0.0f,  0.0f),   /* +X */
    lum_vec3_create(-1.0f,  0.0f,  0.0f),   /* -X */
    lum_vec3_create( 0.0f,  1.0f,  0.0f),   /* +Y */
    lum_vec3_create( 0.0f, -1.0f,  0.0f),   /* -Y */
  };
  const float corners[6][4][3] = {
    { { -hx, -hy,  hz }, {  hx, -hy,  hz }, {  hx,  hy,  hz }, { -hx,  hy,  hz } },
    { {  hx, -hy, -hz }, { -hx, -hy, -hz }, { -hx,  hy, -hz }, {  hx,  hy, -hz } },
    { {  hx, -hy,  hz }, {  hx, -hy, -hz }, {  hx,  hy, -hz }, {  hx,  hy,  hz } },
    { { -hx, -hy, -hz }, { -hx, -hy,  hz }, { -hx,  hy,  hz }, { -hx,  hy, -hz } },
    { { -hx,  hy,  hz }, {  hx,  hy,  hz }, {  hx,  hy, -hz }, { -hx,  hy, -hz } },
    { { -hx, -hy, -hz }, {  hx, -hy, -hz }, {  hx, -hy,  hz }, { -hx, -hy,  hz } },
  };

  /* 每个面的 4 个角依次拿到单位正方形的四个角 —— 也就是"每个面贴一张完整贴图"。
     顺序与上面的 corners 一致，所以 uv 在面内不会漏角也不会重叠。 */
  const lum_Vec2 face_uv[4] = {
    { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f },
  };

  for (uint32_t f = 0; f < 6u; ++f) {
    for (uint32_t v = 0; v < 4u; ++v) {
      lum_Vertex3d* out = &vertices[f * 4u + v];
      out->position = lum_vec3_create(corners[f][v][0], corners[f][v][1], corners[f][v][2]);
      out->normal   = normals[f];
      out->uv       = face_uv[v];
    }
    uint32_t* idx = &indices[f * 6u];
    const uint32_t base = f * 4u;
    idx[0] = base + 0u; idx[1] = base + 1u; idx[2] = base + 2u;
    idx[3] = base + 0u; idx[4] = base + 2u; idx[5] = base + 3u;
  }
}

void lum_mesh_bounds(const lum_Vertex3d* vertices, uint32_t count, lum_Vec3* out_min, lum_Vec3* out_max) {
  Lum_Assert(vertices && out_min && out_max);
  if (!vertices || !out_min || !out_max) return;

  if (count == 0u) {
    *out_min = lum_vec3_0();
    *out_max = lum_vec3_0();
    return;
  }

  *out_min = vertices[0].position;
  *out_max = vertices[0].position;
  for (uint32_t i = 1u; i < count; ++i) {
    const lum_Vec3 p = vertices[i].position;
    if (p.x < out_min->x) out_min->x = p.x;
    if (p.y < out_min->y) out_min->y = p.y;
    if (p.z < out_min->z) out_min->z = p.z;
    if (p.x > out_max->x) out_max->x = p.x;
    if (p.y > out_max->y) out_max->y = p.y;
    if (p.z > out_max->z) out_max->z = p.z;
  }
}

int lum_mesh_create(lum_Mesh* mesh, const lum_Vertex3d* vertices, uint32_t vertex_count,
                    const uint32_t* indices, uint32_t index_count) {
  Lum_Assert(mesh);
  if (!mesh) return 0;
  if (!vertices || vertex_count == 0u || !indices || index_count == 0u) {
    Lum_Log_Warn("lum_mesh_create: 空的顶点或索引，拒绝创建");
    return 0;
  }

  mesh->initialized = false;
  mesh->vao = mesh->vbo = mesh->ebo = 0u;
  mesh->vertex_count = mesh->index_count = 0u;

  glGenVertexArrays(1, &mesh->vao);
  glGenBuffers(1, &mesh->vbo);
  glGenBuffers(1, &mesh->ebo);
  if (mesh->vao == 0u || mesh->vbo == 0u || mesh->ebo == 0u) {
    Lum_Log_Error("lum_mesh_create: 创建 GL 缓冲失败");
    lum_mesh_destroy(mesh);
    return 0;
  }

  glBindVertexArray(mesh->vao);

  glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)((size_t)vertex_count * sizeof(lum_Vertex3d)),
               vertices, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)((size_t)index_count * sizeof(uint32_t)),
               indices, GL_STATIC_DRAW);

  /* 固定顶点布局（唯一的格式，见头文件）：0 = 位置，1 = 法线，2 = uv。
     location 3..5 留给渲染器的实例矩阵（见 lum_renderer3d.c）。 */
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(lum_Vertex3d),
                        (void*)offsetof(lum_Vertex3d, position));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(lum_Vertex3d),
                        (void*)offsetof(lum_Vertex3d, normal));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(lum_Vertex3d),
                        (void*)offsetof(lum_Vertex3d, uv));
  glEnableVertexAttribArray(2);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  mesh->vertex_count = vertex_count;
  mesh->index_count  = index_count;
  lum_mesh_bounds(vertices, vertex_count, &mesh->bounds_min, &mesh->bounds_max);
  mesh->initialized = true;
  return 1;
}

int lum_mesh_create_box(lum_Mesh* mesh, lum_Vec3 half_extents) {
  lum_Vertex3d vertices[Lum_Mesh_Box_Vertex_Count];
  uint32_t indices[Lum_Mesh_Box_Index_Count];
  lum_mesh_build_box(vertices, indices, half_extents);
  return lum_mesh_create(mesh, vertices, Lum_Mesh_Box_Vertex_Count, indices, Lum_Mesh_Box_Index_Count);
}

void lum_mesh_destroy(lum_Mesh* mesh) {
  if (!mesh) return;
  if (mesh->ebo) glDeleteBuffers(1, &mesh->ebo);
  if (mesh->vbo) glDeleteBuffers(1, &mesh->vbo);
  if (mesh->vao) glDeleteVertexArrays(1, &mesh->vao);
  mesh->ebo = mesh->vbo = mesh->vao = 0u;
  mesh->initialized = false;
  mesh->vertex_count = mesh->index_count = 0u;
}

void lum_mesh_bind(const lum_Mesh* mesh) {
  if (!mesh || !mesh->initialized) return;
  glBindVertexArray(mesh->vao);
}

uint32_t lum_mesh_vertex_count(const lum_Mesh* mesh) { return mesh ? mesh->vertex_count : 0u; }
uint32_t lum_mesh_index_count(const lum_Mesh* mesh)  { return mesh ? mesh->index_count  : 0u; }

void lum_mesh_get_bounds(const lum_Mesh* mesh, lum_Vec3* out_min, lum_Vec3* out_max) {
  Lum_Assert(mesh && out_min && out_max);
  if (!mesh || !out_min || !out_max) return;
  *out_min = mesh->bounds_min;
  *out_max = mesh->bounds_max;
}

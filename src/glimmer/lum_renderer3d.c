#include "glimmer/lum_renderer3d.h"
#include "glimmer/lum_shader.h"
#include "glimmer/lum_material.h"
#include "glimmer/lum_log.h"
#include "glimmer/lum_assert.h"

#include <glad/glad.h>
#include <stddef.h>

/*
* 内置着色器：固定方向光 + 环境光，逐片元 lambert。
* 刻意保持"能一眼看出几何对不对"的形态 —— 没有贴图、没有高光、没有材质参数，
* 于是画面里出现的任何明暗异常都只可能来自几何/法线/相机。
*
* M2 起模型矩阵改从**实例属性**来（location 2..5 各一列），不再有 u_model：
* 这样"画 1 个"和"画 N 个"走的是同一条着色器路径（见头文件）。
* 法线用 mat3(model) 变换（只对等比缩放正确，见头文件说明）。
*/
static const char* lum__renderer3d_vertex_shader =
  "#version 330 core\n"
  "\n"
  "layout(location = 0) in vec3 a_position;\n"
  "layout(location = 1) in vec3 a_normal;\n"
  "layout(location = 2) in vec2 a_uv;\n"
  "layout(location = 3) in vec4 a_model0;\n"
  "layout(location = 4) in vec4 a_model1;\n"
  "layout(location = 5) in vec4 a_model2;\n"
  "layout(location = 6) in vec4 a_model3;\n"
  "\n"
  "uniform mat4 u_view_projection;\n"
  "uniform vec2 u_uv_scale;\n"
  "\n"
  "out vec3 v_normal;\n"
  "out vec2 v_uv;\n"
  "\n"
  "void main() {\n"
  "  mat4 model = mat4(a_model0, a_model1, a_model2, a_model3);\n"
  "  v_normal = mat3(model) * a_normal;\n"
  "  v_uv = a_uv * u_uv_scale;\n"
  "  gl_Position = u_view_projection * model * vec4(a_position, 1.0);\n"
  "}\n";

static const char* lum__renderer3d_fragment_shader =
  "#version 330 core\n"
  "\n"
  "in vec3 v_normal;\n"
  "in vec2 v_uv;\n"
  "\n"
  "uniform sampler2D u_albedo;\n"
  "uniform vec4 u_color;\n"
  "\n"
  "out vec4 FragColor;\n"
  "\n"
  "void main() {\n"
  "  vec3 n = normalize(v_normal);\n"
  "  vec3 to_light = normalize(vec3(0.45, 0.80, 0.55));\n"
  "  float lambert = max(dot(n, to_light), 0.0);\n"
  "  float ambient = 0.28;\n"
  "  vec3 base = u_color.rgb * texture(u_albedo, v_uv).rgb;\n"
  "  FragColor = vec4(base * (ambient + 0.72 * lambert), u_color.a);\n"
  "}\n";

/* 顶点属性位置：0 = 位置，1 = 法线，2 = uv（见 lum_mesh.c）；
   模型矩阵占 3..6（M3 加了 uv 之后整体后移一位） */
#define Lum_Renderer3d_Model_Attrib_Base 3u
#define Lum_Renderer3d_Model_Attrib_Count 4u

/* 实例矩阵超过这个数量就先"孤儿化"缓冲再写。
   小批量（尤其是 draw_mesh 的单实例路径）直接 glBufferSubData 更省 ——
   每帧为 64 字节重新分配一次缓冲反而是在给驱动的分配器添乱。 */
#define Lum_Renderer3d_Orphan_Threshold 256u

/* material == NULL 或未初始化时用的兜底材质：白色、不贴图、uv 不平铺 */
static const lum_Material3d lum__material3d_default = {
  true, NULL, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }
};

typedef struct lum_Renderer3d {
  bool initialized;
  lum_Shader shader;
  lum_Texture2d white_texture;   /* 1×1 白：材质没给 albedo 时也走同一条采样路径（着色器里不用分支） */
  lum_Mat4 view_projection;
  bool depth_test;
  lum_Renderer3d_stats stats;

  /* 实例矩阵缓冲：按需增长，永不缩小（容量即上界） */
  uint32_t instance_vbo;
  uint32_t instance_capacity;
  /* 已经配好"实例属性"的那个网格的 VAO。
     注意：我们写进去的配置（location 2..5 指向实例缓冲、divisor = 1）与具体网格无关，
     所以即使这块缓存命中了一个已销毁并重建的网格，配置本身依然正确 —— 它只是省掉重复的 GL 调用。 */
  const lum_Mesh* instance_attribs_mesh;
} lum_Renderer3d;

static lum_Renderer3d g_renderer3d;

/* 把实例矩阵缓冲挂到网格 VAO 的 location 2..5 上（每个网格只做一次） */
static void lum__renderer3d_configure_instance_attribs(const lum_Mesh* mesh) {
  if (g_renderer3d.instance_attribs_mesh == mesh) return;

  glBindVertexArray(mesh->vao);
  glBindBuffer(GL_ARRAY_BUFFER, g_renderer3d.instance_vbo);
  for (GLuint i = 0u; i < Lum_Renderer3d_Model_Attrib_Count; ++i) {
    const GLuint loc = Lum_Renderer3d_Model_Attrib_Base + i;
    glEnableVertexAttribArray(loc);
    /* 一列一个 vec4：stride = 整个 mat4，offset = 第 i 列 */
    glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(lum_Mat4),
                          (const void*)(size_t)(i * sizeof(lum_Vec4)));
    glVertexAttribDivisor(loc, 1u);          /* 每实例前进一次，而不是每顶点 */
  }
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  g_renderer3d.instance_attribs_mesh = mesh;
}

static void lum__renderer3d_upload_instances(const lum_Mat4* models, uint32_t count) {
  const GLsizeiptr bytes = (GLsizeiptr)((size_t)count * sizeof(lum_Mat4));

  glBindBuffer(GL_ARRAY_BUFFER, g_renderer3d.instance_vbo);
  if (count > g_renderer3d.instance_capacity) {
    /* 扩容：直接重新分配（旧内容本来也不需要保留） */
    glBufferData(GL_ARRAY_BUFFER, bytes, models, GL_DYNAMIC_DRAW);
    g_renderer3d.instance_capacity = count;
  } else if (count >= Lum_Renderer3d_Orphan_Threshold) {
    /* 大批量重写：先孤儿化（只分配、不拷贝），再写入 ——
       否则驱动可能等上一帧用完这块缓冲，造成每帧同步停顿 */
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)((size_t)g_renderer3d.instance_capacity * sizeof(lum_Mat4)),
                 NULL, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, models);
  } else {
    glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, models);
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

int lum_renderer3d_init(void) {
  if (g_renderer3d.initialized) return 1;

  g_renderer3d.view_projection = lum_mat4_identity();
  g_renderer3d.depth_test = true;
  g_renderer3d.stats = (lum_Renderer3d_stats){ 0u, 0u, 0u, 0u };
  g_renderer3d.instance_vbo = 0u;
  g_renderer3d.instance_capacity = 0u;
  g_renderer3d.instance_attribs_mesh = NULL;

  if (!lum_shader_create_from_source(&g_renderer3d.shader,
                                     lum__renderer3d_vertex_shader,
                                     lum__renderer3d_fragment_shader)) {
    Lum_Log_Error("lum_renderer3d_init: 3D 着色器编译/链接失败");
    return 0;
  }

  if (!lum_texture2d_create_white(&g_renderer3d.white_texture)) {
    Lum_Log_Error("lum_renderer3d_init: 1x1 白纹理创建失败");
    lum_shader_destroy(&g_renderer3d.shader);
    return 0;
  }
  /* u_albedo 永远是纹理单元 0：设一次即可（M3 只有一个采样器） */
  lum_shader_use(&g_renderer3d.shader);
  lum_shader_uniform_set1i(&g_renderer3d.shader, "u_albedo", 0);

  glGenBuffers(1, &g_renderer3d.instance_vbo);
  if (g_renderer3d.instance_vbo == 0u) {
    Lum_Log_Error("lum_renderer3d_init: 创建实例矩阵缓冲失败");
    lum_texture2d_destroy(&g_renderer3d.white_texture);
    lum_shader_destroy(&g_renderer3d.shader);
    return 0;
  }

  /* 深度：只用到 test + write，函数固定 LESS（右手系、近小远大） */
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);

  g_renderer3d.initialized = true;
  return 1;
}

void lum_renderer3d_shutdown(void) {
  if (!g_renderer3d.initialized) return;
  if (g_renderer3d.instance_vbo) glDeleteBuffers(1, &g_renderer3d.instance_vbo);
  g_renderer3d.instance_vbo = 0u;
  g_renderer3d.instance_capacity = 0u;
  g_renderer3d.instance_attribs_mesh = NULL;
  lum_texture2d_destroy(&g_renderer3d.white_texture);
  lum_shader_destroy(&g_renderer3d.shader);
  glDisable(GL_DEPTH_TEST);
  g_renderer3d.initialized = false;
}

bool lum_renderer3d_is_initialized(void) { return g_renderer3d.initialized; }

void lum_renderer3d_set_viewport(int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return;
  glViewport(x, y, w, h);
}

void lum_renderer3d_begin_frame(void) {
  if (!g_renderer3d.initialized) return;
  /* 只重置【每帧的统计】。刻意【不】碰深度状态：它是"模式"而不是"每帧瞬态"，
     帧初强行恢复默认会把调用方刚设的 set_depth_test(false) 覆盖掉 ——
     探针第一次跑就是这么发现的（深度开/关两次读数完全相同，开关等于失效）。
     深度默认值由 init 设定（开、写深度），之后一直保持到有人改它。 */
  g_renderer3d.stats = (lum_Renderer3d_stats){ 0u, 0u, 0u, 0u };
}

void lum_renderer3d_clear(lum_Vec4 color) {
  if (!g_renderer3d.initialized) return;
  glClearColor(color.x, color.y, color.z, color.w);
  glDepthMask(GL_TRUE);          /* 深度写被关过的话清不掉深度，先打开 */
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void lum_renderer3d_set_camera(const lum_Camera3d* camera) {
  if (!g_renderer3d.initialized) return;
  g_renderer3d.view_projection = camera ? lum_camera3d_view_projection(camera)
                                        : lum_mat4_identity();
}

void lum_renderer3d_set_depth_test(bool enabled) {
  if (!g_renderer3d.initialized) return;
  g_renderer3d.depth_test = enabled;
  if (enabled) glEnable(GL_DEPTH_TEST);
  else         glDisable(GL_DEPTH_TEST);
}

bool lum_renderer3d_depth_test_enabled(void) { return g_renderer3d.depth_test; }

void lum_renderer3d_draw_instanced(const lum_Mesh* mesh, const lum_Material3d* material,
                                   const lum_Mat4* models, uint32_t count) {
  if (!g_renderer3d.initialized || !mesh || !mesh->initialized || mesh->index_count == 0u) return;
  if (count == 0u || !models) return;

  /* 未初始化或传 NULL 都退到兜底材质，调用方不必为"没贴图"准备一个材质对象 */
  const lum_Material3d* mat =
    (material && material->initialized) ? material : &lum__material3d_default;

  lum__renderer3d_configure_instance_attribs(mesh);
  lum__renderer3d_upload_instances(models, count);

  lum_shader_use(&g_renderer3d.shader);
  /* 按名字设置：内部是 glGetUniformLocation。M5 换 UBO + 位置缓存（计划里写明） */
  lum_shader_uniform_set4m(&g_renderer3d.shader, "u_view_projection", g_renderer3d.view_projection);
  lum_shader_uniform_set4v(&g_renderer3d.shader, "u_color", mat->color);
  lum_shader_uniform_set2v(&g_renderer3d.shader, "u_uv_scale", mat->uv_scale);

  /* 没有 albedo 就绑白纹理：着色器里永远采样，不需要"有没有贴图"的分支 */
  lum_texture2d_bind(mat->albedo ? mat->albedo : &g_renderer3d.white_texture, 0u);

  lum_mesh_bind(mesh);
  glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)mesh->index_count, GL_UNSIGNED_INT, 0, (GLsizei)count);
  glBindVertexArray(0);

  g_renderer3d.stats.draw_calls += 1u;
  g_renderer3d.stats.meshes     += 1u;
  g_renderer3d.stats.instances  += count;
  g_renderer3d.stats.triangles  += (mesh->index_count / 3u) * count;
}

void lum_renderer3d_draw_mesh(const lum_Mesh* mesh, const lum_Material3d* material, lum_Mat4 model) {
  /* 画 1 个实例 —— 与实例化共用一条路径，避免"批量走一条、单个走另一条"的双份实现 */
  lum_renderer3d_draw_instanced(mesh, material, &model, 1u);
}

void lum_renderer3d_end_frame(void) {
  /* 立即提交模型下无事可做。留这个函数是给将来换延迟提交时的接口位置 */
}

const lum_Renderer3d_stats* lum_renderer3d_get_stats(void) { return &g_renderer3d.stats; }
uint32_t lum_renderer3d_get_draw_call_count(void) { return g_renderer3d.stats.draw_calls; }
uint32_t lum_renderer3d_get_triangle_count(void)  { return g_renderer3d.stats.triangles; }
uint32_t lum_renderer3d_get_instance_count(void)  { return g_renderer3d.stats.instances; }

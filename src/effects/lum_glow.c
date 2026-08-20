#include "lum_glow.h"
#include "lum_renderer2d.h"
#include "lum_shader.h"
#include "lum_material.h"
#include "lum_transform.h"
#include "lum_log.h"
#include <string.h>
#include "lum_assert.h"

static const char* glow_vertex_shader =
  "#version 330 core\n"
  "layout(location=0) in vec3 aPos;\n"
  "layout(location=1) in vec2 aUV;\n"
  "layout(location=2) in vec4 aColor;\n"
  "uniform mat4 u_view_projection;\n"
  "out vec2 vUV;\n"
  "out vec4 vColor;\n"
  "void main(){ vUV=aUV; vColor=aColor; gl_Position=u_view_projection*vec4(aPos,1.0); }\n";

static const char* glow_fragment_shader =
  "#version 330 core\n"
  "in vec2 vUV;\n"
  "in vec4 vColor;\n"
  "out vec4 FragColor;\n"
  "uniform float u_core_size;\n"
  "uniform float u_core_decay;\n"
  "uniform float u_edge_decay;\n"
  "void main(){\n"
  "  float d = length(vUV - 0.5) * 2.0;\n"                                        /* 0(中心)..1(边) */
  "  float core = u_core_decay * exp(-d * d / (u_core_size * u_core_size + 1e-4));\n" /* 亮核：半径比例 u_core_size 的窄高斯 ×强度 */
  "  float halo = exp(-d * d * u_edge_decay) * 0.35;\n"                           /* 外晕：衰减 u_edge_decay 的宽高斯 ×0.35，柔和散射 */
  "  float a = core + halo;\n"
  "  if (a <= 0.01) discard;\n"                                                   /* 提高阈值：discard 暗环，避免边缘脏色 */
  "  vec3 col = vColor.rgb * a;\n"
  "  float lum = max(max(col.r, col.g), col.b);\n"                                /* 最亮通道 = 亮度近似 */
  "  float over = clamp(lum - 1.0, 0.0, 1.0);\n"                                  /* 过曝量：lum>1 时趋白 */
  "  col = mix(col, vec3(lum), over);\n"                                          /* 单色保持本色，多色重叠趋白 */
  "  FragColor = vec4(col, 1.0);\n"                                               /* additive 下 alpha 不参与 */
  "}\n";

#define Lum_Glow_Variant_Capacity 8u

typedef struct lum_Glow {
  bool initialized;
  lum_Shader shader;
  lum_Material_variant_pool variants;
} lum_Glow;

static lum_Glow g_glow;

int lum_glow_init(void) {
  Lum_Assert(!g_glow.initialized && "lum_glow: double init");
  memset(&g_glow, 0, sizeof(g_glow));

  if (!lum_shader_create_from_source(&g_glow.shader, glow_vertex_shader, glow_fragment_shader)) {
    Lum_Log_Error("glow: shader create failed.");
    return 0;
  }
  if (!lum_material_variant_pool_init(&g_glow.variants, &g_glow.shader, Lum_Glow_Variant_Capacity, 3u)) {
    lum_shader_destroy(&g_glow.shader);
    Lum_Log_Error("glow: variant pool init failed.");
    return 0;
  }
  g_glow.initialized = true;
  return 1;
}

void lum_glow_shutdown(void) {
  if (!g_glow.initialized) return;
  lum_material_variant_pool_destroy(&g_glow.variants);
  lum_shader_destroy(&g_glow.shader);
  memset(&g_glow, 0, sizeof(g_glow));
}

bool lum_glow_is_initialized(void) {
  return g_glow.initialized;
}

void lum_glow_draw_layer_advanced(lum_Vec2 center, float radius, int32_t layer, lum_Vec4 color, float core_size, float core_decay, float edge_decay) {
  if (!g_glow.initialized) return;
  if (radius < 0.0f) return;

  lum_Vec2 size = lum_vec2_create(radius * 2.0f, radius * 2.0f);
  lum_Transform2d t = lum_transform2d_create(center, size, lum_vec2_1(), 0.0f, lum_vec2_scale(size, 0.5f));
  lum_Mat4 model = lum_transform2d_get_matrix(&t);

  lum_Vertex v[4];
  lum_build_quad(model, color, v);

  lum_Material_parameter key[3];
  key[0] = lum_material_construct_parameter_1f("u_core_size", core_size);
  key[1] = lum_material_construct_parameter_1f("u_core_decay", core_decay);
  key[2] = lum_material_construct_parameter_1f("u_edge_decay", edge_decay);
  lum_Material* material = lum_material_variant_pool_acquire(&g_glow.variants, key);
  if (!material) return;

  static const uint32_t idx[6] = { 0, 1, 2, 2, 3, 0 };
  lum_renderer2d_draw_mesh(v, 4, idx, 6, Lum_Primitive_Triangles, material, NULL, layer);
}

void lum_glow_draw_layer(lum_Vec2 center, float radius, int32_t layer, lum_Vec4 color) {
  lum_glow_draw_layer_advanced(center, radius, layer, color, Lum_Glow_Default_Core_Size, Lum_Glow_Default_Core_Decay, Lum_Glow_Default_Edge_Decay);
}

void lum_glow_draw_advanced(lum_Vec2 center, float radius, lum_Vec4 color, float core_size, float core_decay, float edge_decay) {
  lum_glow_draw_layer_advanced(center, radius, 0, color, core_size, core_decay, edge_decay);
}

void lum_glow_draw(lum_Vec2 center, float radius, lum_Vec4 color) {
  lum_glow_draw_layer(center, radius, 0, color);
}

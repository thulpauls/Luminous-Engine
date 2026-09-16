#include "effects/lum_glow.h"
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
  "uniform float u_halo_intensity;\n"
  "void main(){\n"
  "  float d = length(vUV - 0.5) * 2.0;\n"                                        /* 0(中心)..1(边) */
  "  float core = u_core_decay * exp(-d * d / (u_core_size * u_core_size + 1e-4));\n" /* 亮核：半径比例 u_core_size 的窄高斯 ×强度 */
  "  float halo = u_halo_intensity * exp(-d * d * u_edge_decay);\n"               /* 外晕：衰减 u_edge_decay 的宽高斯 ×外围亮度乘数 */
  "  float a = core + halo;\n"
  "  if (a <= 0.01) discard;\n"                                                   /* 提高阈值：discard 暗环，避免边缘脏色 */
  "  vec3 col = vColor.rgb * a;\n"
  "  float lum = max(max(col.r, col.g), col.b);\n"                                /* 最亮通道 = 亮度近似 */
  "  float over = clamp(lum - 1.0, 0.0, 1.0);\n"                                  /* 过曝量：lum>1 时趋白 */
  "  col = mix(col, vec3(lum), over);\n"                                          /* 单色保持本色，多色重叠趋白 */
  "  FragColor = vec4(col, 1.0);\n"                                               /* additive 下 alpha 不参与 */
  "}\n";

static const char* ring_fragment_shader =
  "#version 330 core\n"
  "in vec2 vUV;\n"
  "in vec4 vColor;\n"
  "out vec4 FragColor;\n"
  "uniform float u_ring_radius;\n"
  "uniform float u_ring_width;\n"
  "uniform float u_ring_intensity;\n"
  "uniform float u_ring_inner_decay;\n"
  "uniform float u_ring_outer_decay;\n"
  "void main(){\n"
  "  float d = length(vUV - 0.5) * 2.0;\n"                                        /* 0(中心)..1(边) */
  "  float dist = d - u_ring_radius;\n"                                           /* 到环峰值半径的带符号距离：内负外正 */
  "  float decay = dist >= 0.0 ? u_ring_outer_decay : u_ring_inner_decay;\n"      /* 内外侧独立衰减速率 */
  "  float ring = u_ring_intensity * exp(-(dist * dist) * decay / (u_ring_width * u_ring_width + 1e-4));\n" /* 亮环：非对称高斯 */
  "  if (ring <= 0.01) discard;\n"                                                /* discard 环外暗部，保持中空 */
  "  vec3 col = vColor.rgb * ring;\n"
  "  float lum = max(max(col.r, col.g), col.b);\n"
  "  float over = clamp(lum - 1.0, 0.0, 1.0);\n"
  "  col = mix(col, vec3(lum), over);\n"
  "  FragColor = vec4(col, 1.0);\n"                                               /* additive 下 alpha 不参与 */
  "}\n";

#define Lum_Glow_Variant_Capacity 8u

typedef struct lum_Glow {
  bool initialized;
  lum_Shader shader;                       /* 光晕着色器 */
  lum_Shader ring_shader;                  /* 光圈着色器 */
  lum_Material_variant_pool variants;      /* 光晕材质变体池 */
  lum_Material_variant_pool ring_variants; /* 光圈材质变体池 */
} lum_Glow;

static lum_Glow g_glow;

int lum_glow_init(void) {
  Lum_Assert(!g_glow.initialized && "lum_glow: double init");
  memset(&g_glow, 0, sizeof(g_glow));

  if (!lum_shader_create_from_source(&g_glow.shader, glow_vertex_shader, glow_fragment_shader)) {
    Lum_Log_Error("glow: shader create failed.");
    return 0;
  }
  if (!lum_shader_create_from_source(&g_glow.ring_shader, glow_vertex_shader, ring_fragment_shader)) {
    lum_shader_destroy(&g_glow.shader);
    Lum_Log_Error("glow: ring shader create failed.");
    return 0;
  }
  if (!lum_material_variant_pool_init(&g_glow.variants, &g_glow.shader, Lum_Glow_Variant_Capacity, 4u)) {
    lum_shader_destroy(&g_glow.ring_shader);
    lum_shader_destroy(&g_glow.shader);
    Lum_Log_Error("glow: variant pool init failed.");
    return 0;
  }
  if (!lum_material_variant_pool_init(&g_glow.ring_variants, &g_glow.ring_shader, Lum_Glow_Variant_Capacity, 5u)) {
    lum_material_variant_pool_destroy(&g_glow.variants);
    lum_shader_destroy(&g_glow.ring_shader);
    lum_shader_destroy(&g_glow.shader);
    Lum_Log_Error("glow: ring variant pool init failed.");
    return 0;
  }
  g_glow.initialized = true;
  return 1;
}

void lum_glow_shutdown(void) {
  if (!g_glow.initialized) return;
  lum_material_variant_pool_destroy(&g_glow.ring_variants);
  lum_material_variant_pool_destroy(&g_glow.variants);
  lum_shader_destroy(&g_glow.ring_shader);
  lum_shader_destroy(&g_glow.shader);
  memset(&g_glow, 0, sizeof(g_glow));
}

bool lum_glow_is_initialized(void) {
  return g_glow.initialized;
}

void lum_glow_draw_advanced(lum_Vec2 center, float radius, int32_t layer, lum_Vec4 color, float core_size, float core_decay, float edge_decay, float halo_intensity) {
  if (!g_glow.initialized) return;
  if (radius < 0.0f) return;

  lum_Vec2 size = lum_vec2_create(radius * 2.0f, radius * 2.0f);
  lum_Transform2d t = lum_transform2d_create(center, size, lum_vec2_1(), 0.0f, lum_vec2_scale(size, 0.5f));
  lum_Mat4 model = lum_transform2d_get_matrix(&t);

  lum_Vertex v[4];
  lum_renderer2d_build_quad(model, color, v);

  lum_Material_parameter key[4];
  key[0] = lum_material_construct_parameter_1f("u_core_size", core_size);
  key[1] = lum_material_construct_parameter_1f("u_core_decay", core_decay);
  key[2] = lum_material_construct_parameter_1f("u_edge_decay", edge_decay);
  key[3] = lum_material_construct_parameter_1f("u_halo_intensity", halo_intensity);
  lum_Material* material = lum_material_variant_pool_acquire(&g_glow.variants, key);
  if (!material) return;

  static const uint32_t idx[6] = { 0, 1, 2, 2, 3, 0 };
  lum_renderer2d_draw_mesh(v, 4, idx, 6, Lum_Primitive_Triangles, material, NULL, layer);
}

void lum_glow_draw(lum_Vec2 center, float radius, int32_t layer, lum_Vec4 color) {
  lum_glow_draw_advanced(center, radius, layer, color, Lum_Glow_Default_Core_Size, Lum_Glow_Default_Core_Decay, Lum_Glow_Default_Edge_Decay, Lum_Glow_Default_Halo_Intensity);
}

void lum_glow_draw_ring_advanced(lum_Vec2 center, float radius, int32_t layer, lum_Vec4 color, float ring_radius, float ring_width, float ring_intensity, float ring_inner_decay, float ring_outer_decay) {
  if (!g_glow.initialized) return;
  if (radius < 0.0f) return;

  lum_Vec2 size = lum_vec2_create(radius * 2.0f, radius * 2.0f);
  lum_Transform2d t = lum_transform2d_create(center, size, lum_vec2_1(), 0.0f, lum_vec2_scale(size, 0.5f));
  lum_Mat4 model = lum_transform2d_get_matrix(&t);

  lum_Vertex v[4];
  lum_renderer2d_build_quad(model, color, v);

  lum_Material_parameter key[5];
  key[0] = lum_material_construct_parameter_1f("u_ring_radius", ring_radius);
  key[1] = lum_material_construct_parameter_1f("u_ring_width", ring_width);
  key[2] = lum_material_construct_parameter_1f("u_ring_intensity", ring_intensity);
  key[3] = lum_material_construct_parameter_1f("u_ring_inner_decay", ring_inner_decay);
  key[4] = lum_material_construct_parameter_1f("u_ring_outer_decay", ring_outer_decay);
  lum_Material* material = lum_material_variant_pool_acquire(&g_glow.ring_variants, key);
  if (!material) return;

  static const uint32_t idx[6] = { 0, 1, 2, 2, 3, 0 };
  lum_renderer2d_draw_mesh(v, 4, idx, 6, Lum_Primitive_Triangles, material, NULL, layer);
}

void lum_glow_draw_ring(lum_Vec2 center, float radius, int32_t layer, lum_Vec4 color) {
  lum_glow_draw_ring_advanced(center, radius, layer, color, Lum_Glow_Default_Ring_Radius, Lum_Glow_Default_Ring_Width, Lum_Glow_Default_Ring_Intensity, Lum_Glow_Default_Ring_Inner_Decay, Lum_Glow_Default_Ring_Outer_Decay);
}

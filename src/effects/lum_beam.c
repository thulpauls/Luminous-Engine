#include "effects/lum_beam.h"
#include "glimmer/lum_renderer2d.h"
#include "glimmer/lum_shader.h"
#include "glimmer/lum_material.h"
#include "glimmer/lum_transform.h"
#include "glimmer/lum_log.h"
#include "glimmer/lum_assert.h"
#include <math.h>
#include <string.h>

static const char* beam_vertex_shader =
  "#version 330 core\n"
  "layout(location=0) in vec3 aPos;\n"
  "layout(location=1) in vec2 aUV;\n"
  "layout(location=2) in vec4 aColor;\n"
  "uniform mat4 u_view_projection;\n"
  "out vec2 vUV;\n"
  "out vec4 vColor;\n"
  "void main(){ vUV=aUV; vColor=aColor; gl_Position=u_view_projection*vec4(aPos,1.0); }\n";

static const char* beam_fragment_shader =
  "#version 330 core\n"
  "in vec2 vUV;\n"
  "in vec4 vColor;\n"
  "out vec4 FragColor;\n"
  "uniform int u_mode;\n"
  "uniform float u_core_size;\n"
  "uniform float u_core_decay;\n"
  "uniform float u_edge_decay;\n"
  "void main(){\n"
  "  float d;\n"
  "  if (u_mode == 0) {\n"
  "    d = abs(vUV.y - 0.5) * 2.0;\n"      /* 横向：中心线 → 两侧 */
  "  } else if (u_mode == 1) {\n"
  "    d = abs(vUV.x - 0.5) * 2.0;\n"      /* 纵向：中心点 → 两端 */
  "  } else {\n"
  "    d = length(vUV - 0.5) * 2.0;\n"     /* 中心：同时向宽度和长度方向衰减 */
  "  }\n"
  "  float core = u_core_decay * exp(-d * d / (u_core_size * u_core_size + 1e-4));\n" /* 亮核：窄高斯 ×强度 */
  "  float edge = exp(-d * d * u_edge_decay) * 0.35;\n"                          /* 边缘：宽高斯 ×0.35 */
  "  float a = core + edge;\n"
  "  if (a <= 0.01) discard;\n"
  "  vec3 col = vColor.rgb * a;\n"
  "  float lum = max(max(col.r, col.g), col.b);\n"                                /* 最亮通道 = 亮度近似 */
  "  float over = clamp(lum - 1.0, 0.0, 1.0);\n"                                  /* 过曝量：lum>1 时趋白 */
  "  col = mix(col, vec3(lum), over);\n"                                          /* 单色保持本色，多色重叠趋白 */
  "  FragColor = vec4(col, 1.0);\n"                                               /* additive 下 alpha 不参与 */
  "}\n";

#define Lum_Beam_Variant_Capacity 8u

typedef struct lum_Beam {
  bool initialized;
  lum_Shader shader;
  lum_Material_variant_pool variants;
} lum_Beam;

static lum_Beam g_beam;

int lum_beam_init(void) {
  Lum_Assert(!g_beam.initialized && "lum_beam: double init");
  memset(&g_beam, 0, sizeof(g_beam));

  if (!lum_shader_create_from_source(&g_beam.shader, beam_vertex_shader, beam_fragment_shader)) {
    Lum_Log_Error("beam: shader create failed.");
    return 0;
  }
  if (!lum_material_variant_pool_init(&g_beam.variants, &g_beam.shader, Lum_Beam_Variant_Capacity, 4u)) {
    lum_shader_destroy(&g_beam.shader);
    Lum_Log_Error("beam: variant pool init failed.");
    return 0;
  }
  g_beam.initialized = true;
  return 1;
}

void lum_beam_shutdown(void) {
  if (!g_beam.initialized) return;
  lum_material_variant_pool_destroy(&g_beam.variants);
  lum_shader_destroy(&g_beam.shader);
  memset(&g_beam, 0, sizeof(g_beam));
}

bool lum_beam_is_initialized(void) {
  return g_beam.initialized;
}

void lum_beam_draw_advanced(lum_Vec2 start, lum_Vec2 end, float thickness, lum_Beam_mode mode, int32_t layer, lum_Vec4 color, float core_size, float core_decay, float edge_decay) {
  if (!g_beam.initialized) return;
  if (thickness <= 0.0f) return;

  lum_Vec2 dir = lum_vec2_sub(end, start);
  float length = lum_vec2_len(dir);
  if (length < 1e-4f) return;

  lum_Vec2 center = lum_vec2_scale(lum_vec2_add(start, end), 0.5f);
  lum_Vec2 size = lum_vec2_create(length, thickness);
  float angle = atan2f(dir.y, dir.x);

  lum_Transform2d t = lum_transform2d_create(center, size, lum_vec2_1(), angle, lum_vec2_scale(size, 0.5f));
  lum_Mat4 model = lum_transform2d_get_matrix(&t);

  lum_Vertex v[4];
  lum_renderer2d_build_quad(model, color, v);

  lum_Material_parameter key[4];
  key[0] = lum_material_construct_parameter_1i("u_mode", (int)mode);
  key[1] = lum_material_construct_parameter_1f("u_core_size", core_size);
  key[2] = lum_material_construct_parameter_1f("u_core_decay", core_decay);
  key[3] = lum_material_construct_parameter_1f("u_edge_decay", edge_decay);
  lum_Material* material = lum_material_variant_pool_acquire(&g_beam.variants, key);
  if (!material) return;

  static const uint32_t idx[6] = { 0, 1, 2, 2, 3, 0 };
  lum_renderer2d_draw_mesh(v, 4, idx, 6, Lum_Primitive_Triangles, material, NULL, layer);
}

void lum_beam_draw(lum_Vec2 start, lum_Vec2 end, float thickness, lum_Beam_mode mode, int32_t layer, lum_Vec4 color) {
  lum_beam_draw_advanced(start, end, thickness, mode, layer, color, Lum_Beam_Default_Core_Size, Lum_Beam_Default_Core_Decay, Lum_Beam_Default_Edge_Decay);
}

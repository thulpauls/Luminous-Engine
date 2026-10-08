/* glimmer_material3d —— M3 的验收示例（见 docs/design/glimmer3d.md）。
 *
 * 一件核心事实：**draw call 数 = 网格数 × 材质数，与实例总数无关。**
 * 所以这里画 M 个材质 × 每材质 N/M 个实例，并自检 draw_calls == M（而不是 N）。
 *
 * 场景：4096 个立方体，按 4 个材质分成 4 组（每组 1024 个），每组一次实例化绘制。
 * 贴图是**程序生成**的棋盘格（不依赖任何资源文件，颜色可预测，便于自动化验证）。
 * 四个材质各自的贴图带 sRGB 标志 —— 颜色贴图就该标它（光照必须在线性空间里算）。
 *
 * 键：[S] 切换所有贴图的 sRGB 标志（画面会明显变亮/变暗，这就是那条可量化的 A/B）；
 *     [Q]/[ESC] 退出。脚本化测量跑完自动退出。
 */

#include "glimmer/glimmer.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAT_WIN_W 960
#define MAT_WIN_H 600
#define MAT_COUNT 4u
#define MAT_INSTANCES 1024u          /* 每个材质的实例数 → 总数 4096 */
#define MAT_TEX_SIZE 64
#define MAT_HALF_CUBE 0.25f

/* 程序生成棋盘格：底色 + 更亮的一格，四通道 RGBA */
static void build_checker(unsigned char* out, int size, int cells, float r, float g, float b) {
  const int cell = size / cells;
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const int checker = (((x / cell) + (y / cell)) & 1);
      const float k = checker ? 1.0f : 0.55f;          /* 亮格 / 暗格 */
      unsigned char* px = &out[(y * size + x) * 4];
      px[0] = (unsigned char)(r * k * 255.0f);
      px[1] = (unsigned char)(g * k * 255.0f);
      px[2] = (unsigned char)(b * k * 255.0f);
      px[3] = 255;
    }
  }
}

static double wall_ms(void) {
  struct timespec ts;
  timespec_get(&ts, TIME_UTC);
  return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

static double median_of(double* v, uint32_t n) {
  for (uint32_t i = 1; i < n; ++i) {
    const double key = v[i];
    uint32_t j = i;
    while (j > 0u && v[j - 1u] > key) { v[j] = v[j - 1u]; --j; }
    v[j] = key;
  }
  if (n == 0u) return 0.0;
  return (n & 1u) ? v[n / 2u] : 0.5 * (v[n / 2u - 1u] + v[n / 2u]);
}

/* 四个材质的贴图（程序生成）与材质本体 */
static lum_Texture2d g_textures[MAT_COUNT];
static lum_Material3d g_materials[MAT_COUNT];
static bool g_srgb = true;

static int create_textures(void) {
  unsigned char* pixels = (unsigned char*)malloc((size_t)MAT_TEX_SIZE * MAT_TEX_SIZE * 4u);
  if (!pixels) return 0;
  const float hues[MAT_COUNT][3] = {
    { 0.95f, 0.35f, 0.25f }, { 0.30f, 0.85f, 0.40f },
    { 0.30f, 0.55f, 0.98f }, { 0.95f, 0.85f, 0.30f },
  };
  uint32_t flags = Lum_Texture_Flag_Mipmaps | Lum_Texture_Flag_Filter_Linear;
  if (g_srgb) flags |= Lum_Texture_Flag_Srgb;

  int ok = 1;
  for (uint32_t m = 0; m < MAT_COUNT; ++m) {
    build_checker(pixels, MAT_TEX_SIZE, 8, hues[m][0], hues[m][1], hues[m][2]);
    if (!lum_texture2d_create(&g_textures[m], MAT_TEX_SIZE, MAT_TEX_SIZE, 4, pixels, flags)) ok = 0;
  }
  free(pixels);
  return ok;
}

static void destroy_textures(void) {
  for (uint32_t m = 0; m < MAT_COUNT; ++m) lum_texture2d_destroy(&g_textures[m]);
}

int main(void) {
  if (!lum_log_init()) { fprintf(stderr, "glimmer_material3d: log init failed\n"); return 1; }
  lum_log_set_min_level(Lum_Log_Level_Info);
  if (!lum_window_init("glimmer material3d (M3)", MAT_WIN_W, MAT_WIN_H)) {
    fprintf(stderr, "glimmer_material3d: window init failed\n"); return 1;
  }
  lum_input_init(); lum_time_init();
  lum_window_set_vsync(false);                 /* 测量必须关垂直同步（见 M2 的教训） */
  if (!lum_renderer3d_init()) { fprintf(stderr, "glimmer_material3d: renderer3d init failed\n"); return 1; }

  const uint32_t fb_w = lum_window_get_framebuffer_width();
  const uint32_t fb_h = lum_window_get_framebuffer_height();
  lum_renderer3d_set_viewport(0, 0, (int)fb_w, (int)fb_h);

  lum_Camera3d cam;
  lum_camera3d_init(&cam, 1.0f);
  lum_camera3d_set_viewport(&cam, (float)fb_w, (float)fb_h);
  cam.position = lum_vec3_create(0.0f, 0.0f, 22.0f);
  cam.target   = lum_vec3_0();

  lum_Mesh cube;
  if (!lum_mesh_create_box(&cube, lum_vec3_create(MAT_HALF_CUBE, MAT_HALF_CUBE, MAT_HALF_CUBE))) {
    fprintf(stderr, "glimmer_material3d: mesh creation failed\n"); return 1;
  }
  if (!create_textures()) { fprintf(stderr, "glimmer_material3d: texture creation failed\n"); return 1; }
  for (uint32_t m = 0; m < MAT_COUNT; ++m) {
    lum_material3d_init(&g_materials[m]);
    lum_material3d_set_albedo(&g_materials[m], &g_textures[m]);
    lum_material3d_set_uv_scale(&g_materials[m], lum_vec2_create(1.0f, 1.0f));
  }

  /* 每个材质一组实例矩阵：排成一面墙，四组横向并排，便于肉眼分辨材质分组 */
  lum_Mat4* groups[MAT_COUNT];
  const uint32_t cols = 32u;                                   /* 每组的网格列数（32×32 = 1024） */
  const float spacing = 0.75f;
  for (uint32_t m = 0; m < MAT_COUNT; ++m) {
    groups[m] = (lum_Mat4*)malloc((size_t)MAT_INSTANCES * sizeof(lum_Mat4));
    if (!groups[m]) { fprintf(stderr, "glimmer_material3d: out of memory\n"); return 1; }
    for (uint32_t i = 0; i < MAT_INSTANCES; ++i) {
      const float x = ((float)(i % cols) - (float)(cols - 1) * 0.5f) * spacing
                    + ((float)m - 1.5f) * (float)cols * spacing * 1.15f;
      const float y = ((float)(i / cols) - (float)(cols - 1) * 0.5f) * spacing;
      groups[m][i] = lum_mat4_translate(x, y, -4.0f);
    }
  }

  const lum_Vec4 bg = lum_vec4_create(0.05f, 0.06f, 0.09f, 1.0f);
  Lum_Log_Info("glimmer_material3d: up (%ux%u), %u materials x %u instances, sRGB=%d",
               fb_w, fb_h, MAT_COUNT, MAT_INSTANCES, (int)g_srgb);
  printf("glimmer_material3d: 批次单位 = （网格，材质）；本场景 = 1 个网格 x %u 个材质\n", MAT_COUNT);

  /* 两遍：第一遍整轮丢弃（进程刚起来时 CPU 还在升频、驱动在初始化 —— 见 M2 的教训） */
  uint32_t calls_grouped = 0u, calls_per_object = 0u;
  double grouped_med = 0.0, per_object_med = 0.0;

  for (int pass = 0; pass < 2; ++pass) {
    const bool recording = (pass == 1);

    for (int mode = 0; mode < 2; ++mode) {
      const uint32_t warmup = 20u, frames = 40u;
      double samples[64]; uint32_t ns = 0u;
      uint32_t last_calls = 0u;

      for (uint32_t f = 0; f < warmup + frames; ++f) {
        const bool measured = (f >= warmup) && recording;
        const double w0 = measured ? wall_ms() : 0.0;

        lum_time_begin_frame();
        lum_window_poll_events();
        lum_input_update();
        lum_time_update();
        if (lum_input_is_key_pressed(Lum_Key_Q) || lum_input_is_key_pressed(Lum_Key_Escape)) goto done;

        /* 每帧同步视口与宽高比（窗口被拉伸时画面不变形） */
        const uint32_t fw = lum_window_get_framebuffer_width();
        const uint32_t fh = lum_window_get_framebuffer_height();
        lum_renderer3d_set_viewport(0, 0, (int)fw, (int)fh);
        lum_camera3d_set_viewport(&cam, (float)fw, (float)fh);

        lum_renderer3d_begin_frame();
        lum_renderer3d_set_camera(&cam);
        lum_renderer3d_clear(bg);

        if (mode == 0) {
          /* ① 按材质分组：每个材质一次实例化 —— draw call 数 = 材质数 */
          for (uint32_t m = 0; m < MAT_COUNT; ++m)
            lum_renderer3d_draw_instanced(&cube, &g_materials[m], groups[m], MAT_INSTANCES);
        } else {
          /* ② 逐物体提交：同样 4096 个物体，但每个都单独一次绘制 */
          for (uint32_t m = 0; m < MAT_COUNT; ++m)
            for (uint32_t i = 0; i < MAT_INSTANCES; ++i)
              lum_renderer3d_draw_mesh(&cube, &g_materials[m], groups[m][i]);
        }

        last_calls = lum_renderer3d_get_draw_call_count();
        lum_renderer3d_end_frame();
        lum_window_swap_buffers();
        lum_time_end_frame();

        if (measured) {
          const double dt = wall_ms() - w0;
          if (ns < 64u) samples[ns++] = dt;
        }
      }

      const double med = median_of(samples, ns);
      if (recording) {
        if (mode == 0) { calls_grouped = last_calls; grouped_med = med; }
        else           { calls_per_object = last_calls; per_object_med = med; }
        printf("%-14s：draw_calls=%-6u 提交 %7.3f ms/帧\n",
               mode == 0 ? "按材质分组" : "逐物体提交", (unsigned)last_calls, med);
        fflush(stdout);
      }
    }
  }

  /* 自检：分组绘制的 draw call 必须等于材质数（而不是物体数） */
  {
    const uint32_t total = MAT_COUNT * MAT_INSTANCES;
    const int ok = (calls_grouped == MAT_COUNT) && (calls_per_object == total);
    printf("自检：按材质分组 draw_calls=%u（期望 %u = 材质数），逐物体 draw_calls=%u（期望 %u = 物体数）→ %s\n",
           (unsigned)calls_grouped, MAT_COUNT, (unsigned)calls_per_object, total, ok ? "符合" : "★不符★");
    fflush(stdout);
    if (!ok) { for (uint32_t m = 0; m < MAT_COUNT; ++m) free(groups[m]); return 1; }

    const double speedup = (grouped_med > 0.0) ? per_object_med / grouped_med : 0.0;
    printf("小结：%u 个物体、%u 个材质 —— 提交耗时 %.3f ms vs %.3f ms（快 %.1f×），draw call %u vs %u\n",
           total, MAT_COUNT, grouped_med, per_object_med, speedup, (unsigned)calls_grouped,
           (unsigned)calls_per_object);
    fflush(stdout);
  }

  /* 交互一段：按 [S] 切换 sRGB，看画面明暗变化（这是 M3 那条 A/B 的肉眼版） */
  for (uint32_t f = 0; f < 240u; ++f) {
    lum_time_begin_frame();
    lum_window_poll_events();
    lum_input_update();
    lum_time_update();
    if (lum_input_is_key_pressed(Lum_Key_Q) || lum_input_is_key_pressed(Lum_Key_Escape)) break;
    if (lum_input_is_key_pressed(Lum_Key_S)) {
      g_srgb = !g_srgb;
      destroy_textures();
      if (!create_textures()) break;
      for (uint32_t m = 0; m < MAT_COUNT; ++m) lum_material3d_set_albedo(&g_materials[m], &g_textures[m]);
      Lum_Log_Info("glimmer_material3d: sRGB %s（颜色贴图必须标；且光照算完还要把输出编码回 sRGB，那一半在 M7）",
                   g_srgb ? "已开启" : "已关闭");
    }
    lum_renderer3d_begin_frame();
    lum_renderer3d_set_camera(&cam);
    lum_renderer3d_clear(bg);
    for (uint32_t m = 0; m < MAT_COUNT; ++m)
      lum_renderer3d_draw_instanced(&cube, &g_materials[m], groups[m], MAT_INSTANCES);
    lum_renderer3d_end_frame();
    lum_window_swap_buffers();
    lum_time_end_frame();
  }

done:
  Lum_Log_Info("glimmer_material3d: done, closing");
  destroy_textures();
  for (uint32_t m = 0; m < MAT_COUNT; ++m) free(groups[m]);
  lum_mesh_destroy(&cube);
  lum_renderer3d_shutdown();
  lum_time_shutdown();
  lum_input_shutdown();
  lum_window_shutdown();
  lum_log_shutdown();
  return 0;
}

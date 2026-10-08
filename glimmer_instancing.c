/* glimmer_instancing —— M2 的验收示例（见 docs/design/glimmer3d.md）。
 *
 * 一件事：**同一个 mesh 画 N 个实例**，对照两种提交方式，给出 draw call 数与耗时表。
 *
 *     ① 实例化      ：一次 lum_renderer3d_draw_instanced()  → 1 次 draw call
 *     ② 逐实例提交  ：循环 N 次 lum_renderer3d_draw_mesh()  → N 次 draw call
 *
 * 两者画的是**同一个场景**（同样 N 个立方体、同样的矩阵），所以表格里的差异只来自提交方式。
 * 测量取 N = 1000 与 10000（重的那一组少跑几帧，免得冒烟等太久）。
 *
 * 画面上是一面由 N 个立方体组成的墙：改动的是间距而不是相机，所以取景稳定、
 * 切换 N 不会让"看起来东西变多了"混进耗时里。
 *
 * 操作：[Q] / [ESC] 提前退出。四组测量跑完自动退出（便于自动化冒烟）。
 */

#include "glimmer/glimmer.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define INST_WIN_W 960
#define INST_WIN_H 600
#define INST_HALF_CUBE 0.25f        /* 立方体 mesh 的半长（之后由实例矩阵缩放） */
#define INST_HALF_WORLD 15.0        /* 那面墙的半宽（世界单位），取景固定 */

/* 墙上第 i 个实例的模型矩阵：先按网格缩放到 spacing 的比例，再平移到网格位置 */
static lum_Mat4 instance_model(uint32_t i, uint32_t cols, float spacing) {
  const uint32_t rows = cols;
  const float x = ((float)(i % cols) - (float)(cols - 1) * 0.5f) * spacing;
  const float y = ((float)(i / cols) - (float)(rows - 1) * 0.5f) * spacing;
  const float s = (spacing * 0.4f) / INST_HALF_CUBE;      /* 立方体占格子的 40% */
  /* 列向量约定：mul(A, B) 先作用 B —— 所以是"先缩放，再平移" */
  return lum_mat4_mul(lum_mat4_translate(x, y, -2.0f), lum_mat4_scale(s, s, s));
}

static uint32_t grid_cols(uint32_t n) {
  uint32_t c = 1u;
  while (c * c < n) ++c;
  return c;
}

/* 墙钟毫秒（C11） */
static double wall_ms(void) {
  struct timespec ts;
  timespec_get(&ts, TIME_UTC);
  return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

/* 提交耗时用【墙钟】而不是 clock()：
   Windows 上 clock() 的粒度约 15 ms（CLOCKS_PER_SEC = 1000 且按调度器时基跳动），
   亚毫秒级的提交差根本量不出来 —— 第一版就是这么把"1000 个实例化"量成 3.9 ms 的。
   墙钟量的是"发完这批命令花了多久"（含驱动的同步部分），这才是提交开销。 */

typedef struct {
  uint32_t n;
  bool     instanced;
  uint32_t frames;
  double   submit_median_ms; /* 每帧"提交"这一段（墙钟）的中位数 */
  double   submit_mean_ms;   /* 均值：与中位数一起打印，一次性尖峰会自己现形 */
  double   frame_wall_ms;    /* 每帧整帧（含交换） */
  uint32_t draw_calls;       /* 最后一帧的统计 */
  uint32_t triangles;
} Inst_measure;

#define INST_MAX_SAMPLES 128u

static double median_of(double* v, uint32_t n) {
  for (uint32_t i = 1; i < n; ++i) {          /* 样本很少，插入排序足够 */
    const double key = v[i];
    uint32_t j = i;
    while (j > 0u && v[j - 1u] > key) { v[j] = v[j - 1u]; --j; }
    v[j] = key;
  }
  if (n == 0u) return 0.0;
  return (n & 1u) ? v[n / 2u] : 0.5 * (v[n / 2u - 1u] + v[n / 2u]);
}

int main(void) {
  if (!lum_log_init()) { fprintf(stderr, "glimmer_instancing: log init failed\n"); return 1; }
  lum_log_set_min_level(Lum_Log_Level_Info);

  if (!lum_window_init("glimmer instancing (M2)", INST_WIN_W, INST_WIN_H)) {
    fprintf(stderr, "glimmer_instancing: window init failed\n");
    return 1;
  }
  if (!lum_input_init()) { fprintf(stderr, "glimmer_instancing: input init failed\n"); return 1; }
  if (!lum_time_init())  { fprintf(stderr, "glimmer_instancing: time init failed\n");  return 1; }
  if (!lum_renderer3d_init()) { fprintf(stderr, "glimmer_instancing: renderer3d init failed\n"); return 1; }

  const uint32_t fb_w = lum_window_get_framebuffer_width();
  const uint32_t fb_h = lum_window_get_framebuffer_height();
  /* 测量前必须关掉垂直同步：否则帧被卡在刷新率上（本机 240 Hz = 4.167 ms/帧），
     "提交耗时"会被 present 的等待淹没 —— 第一版就是被这个骗了：
     12000 个三角形的实例化画得比 1000 个还"快"，因为等待落到哪一组完全是偶然。 */
  lum_window_set_vsync(false);

  lum_renderer3d_set_viewport(0, 0, (int)fb_w, (int)fb_h);

  /* 相机固定（不绕轨道）：像素负载稳定，耗时差异才只反映提交方式 */
  lum_Camera3d cam;
  lum_camera3d_init(&cam, 1.0f);
  lum_camera3d_set_viewport(&cam, (float)fb_w, (float)fb_h);
  cam.position = lum_vec3_create(0.0f, 0.0f, 30.0f);
  cam.target   = lum_vec3_create(0.0f, 0.0f, -2.0f);

  lum_Mesh cube;
  if (!lum_mesh_create_box(&cube, lum_vec3_create(INST_HALF_CUBE, INST_HALF_CUBE, INST_HALF_CUBE))) {
    fprintf(stderr, "glimmer_instancing: mesh creation failed\n");
    return 1;
  }

  lum_Material3d cube_mat = lum_material3d_create(lum_vec4_create(0.55f, 0.78f, 0.95f, 1.0f));
  const lum_Vec4 bg_color   = lum_vec4_create(0.05f, 0.06f, 0.09f, 1.0f);

  Lum_Log_Info("glimmer_instancing: up (%ux%u), mesh %u verts / %u indices",
               fb_w, fb_h, lum_mesh_vertex_count(&cube), lum_mesh_index_count(&cube));

  const uint32_t counts[2] = { 1000u, 10000u };
  Inst_measure results[4];
  int result_count = 0;
  bool quit = false;

  printf("glimmer_instancing: 同一个 mesh，两种提交方式（画面完全相同）\n");

  for (int pass = 0; pass < 2 && !quit; ++pass) {
  const bool recording = (pass == 1);   /* 第一遍整轮丢弃：见下方注释 */
  if(!recording) printf("（预热：第一遍整轮不计入表格 —— 进程刚起来时 CPU 还在升频、驱动也在初始化）\n");
  for (int ci = 0; ci < 2 && !quit; ++ci) {
    const uint32_t n = counts[ci];
    const uint32_t cols = grid_cols(n);
    const float spacing = (float)(2.0 * INST_HALF_WORLD) / (float)cols;

    lum_Mat4* models = (lum_Mat4*)malloc((size_t)n * sizeof(lum_Mat4));
    if (!models) { fprintf(stderr, "glimmer_instancing: out of memory for %u instances\n", n); return 1; }
    for (uint32_t i = 0; i < n; ++i) models[i] = instance_model(i, cols, spacing);

    for (int mode = 0; mode < 2 && !quit; ++mode) {
      const bool instanced = (mode == 0);
      /* 逐实例提交在 10000 时是每帧一万次提交：少跑几帧，别让冒烟等太久 */
      const uint32_t frames = (n >= 10000u && !instanced) ? 10u : 60u;

      double submit_wall = 0.0, frame_wall = 0.0;
      double samples[INST_MAX_SAMPLES]; uint32_t sample_count = 0u;
      uint32_t last_calls = 0u, last_tris = 0u;

      /* 预热帧：不计量。首次用某个网格做实例化时才会配置实例属性、首次大数量才分配缓冲，
         驱动的管线状态也是第一次遇到才验证 —— 这些一次性成本不能算进稳态耗时。 */
      const uint32_t warmup = 60u;
      for (uint32_t f = 0; f < warmup + frames && !quit; ++f) {
        const bool measured = (f >= warmup);
        const double w0 = measured ? wall_ms() : 0.0;
        lum_time_begin_frame();
        lum_window_poll_events();
        lum_input_update();
        lum_time_update();

        if (lum_input_is_key_pressed(Lum_Key_Q) || lum_input_is_key_pressed(Lum_Key_Escape)) {
          quit = true;
          break;
        }

        /* 每帧同步视口与宽高比（同 glimmer_hello3d）：窗口被拉伸时画面不变形 */
        const uint32_t fw = lum_window_get_framebuffer_width();
        const uint32_t fh = lum_window_get_framebuffer_height();
        lum_renderer3d_set_viewport(0, 0, (int)fw, (int)fh);
        lum_camera3d_set_viewport(&cam, (float)fw, (float)fh);

        lum_renderer3d_begin_frame();
        lum_renderer3d_set_camera(&cam);
        lum_renderer3d_clear(bg_color);

        const double c0 = measured ? wall_ms() : 0.0;
        if (instanced) {
          lum_renderer3d_draw_instanced(&cube, &cube_mat, models, n);
        } else {
          for (uint32_t i = 0; i < n; ++i) lum_renderer3d_draw_mesh(&cube, &cube_mat, models[i]);
        }
        if (measured) {
          const double dt_submit = wall_ms() - c0;
          submit_wall += dt_submit;
          if (sample_count < INST_MAX_SAMPLES) samples[sample_count++] = dt_submit;
        }

        lum_renderer3d_end_frame();
        last_calls = lum_renderer3d_get_draw_call_count();
        last_tris  = lum_renderer3d_get_triangle_count();

        lum_window_swap_buffers();
        lum_time_end_frame();
        if (measured) frame_wall += (wall_ms() - w0);
      }

      if (quit) break;

      Inst_measure m;
      m.n = n;
      m.instanced = instanced;
      m.frames = frames;
      m.submit_mean_ms = submit_wall / (double)frames;
      m.submit_median_ms = median_of(samples, sample_count);
      m.frame_wall_ms = frame_wall / (double)frames;
      m.draw_calls = last_calls;
      m.triangles = last_tris;
      if (recording) results[result_count++] = m;

      if (recording) printf("N=%-5u %-12s : draw_calls=%-6u triangles=%-7u 提交中位 %7.3f ms  均值 %7.3f ms  整帧 %7.3f ms （%u 帧，预热 60 帧不计）\n",
             (unsigned)m.n, instanced ? "实例化" : "逐实例提交",
             (unsigned)m.draw_calls, (unsigned)m.triangles,
             m.submit_median_ms, m.submit_mean_ms, m.frame_wall_ms, (unsigned)m.frames);
      fflush(stdout);
    }
    free(models);
  }
  }   /* pass 循环结束 */

  /* 自检：实例化必须恒为 1 次 draw call，逐实例提交必须是 N 次 */
  int bad = 0;
  for (int i = 0; i < result_count; ++i) {
    const uint32_t want = results[i].instanced ? 1u : results[i].n;
    if (results[i].draw_calls != want) {
      printf("  ★FAIL★ N=%u %s draw_calls=%u，期望 %u\n", (unsigned)results[i].n,
             results[i].instanced ? "实例化" : "逐实例提交", (unsigned)results[i].draw_calls, (unsigned)want);
      ++bad;
    }
  }
  printf("自检：draw_calls 期望值 %s（实例化 = 1，逐实例提交 = N）\n", bad == 0 ? "全部符合" : "有不符");
  Lum_Log_Info("glimmer_instancing: done, closing");

  lum_mesh_destroy(&cube);
  lum_renderer3d_shutdown();
  lum_time_shutdown();
  lum_input_shutdown();
  lum_window_shutdown();
  lum_log_shutdown();
  return bad == 0 ? 0 : 1;
}

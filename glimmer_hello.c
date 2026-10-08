/* glimmer_hello —— glimmer 基础平台的【独立运行验证】。
 *
 * 这个文件的唯一目的：证明 glimmer **不依赖 Luminous 也能跑起来**。
 * 它只使用 `glimmer/glimmer.h` 的公开 API，并且构建时**只链接 libglimmer.a**，
 * 完全不链接 libLuminous.a（见 CMakeLists.txt 的 glimmer_hello 目标）。
 * 因此它是 glimmer 边界的机器判据：谁把引擎高层依赖引进 glimmer，这里会直接链接失败。
 *
 * 与 Luminous 的 `lum_application` 的关系：那边有 17 阶段管线 + ECS + 视图 + 场景系统；
 * 这里是同一套底层能力的**最小手工帧循环** —— 窗口 / 输入 / 时间 / 渲染器自己按顺序起停，
 * 每帧 begin_frame → 清屏 → 画 → end_frame → swap。高层引擎就是在这套东西之上搭的。
 *
 * 画的东西：一块背景 + 旋转方块 + 圆盘 + 文字（ASCII）+ 一根线，
 * 用来覆盖"数学（Transform2d/矩阵）→ 几何 → 纹理/文字 → 批处理提交"整条链路。
 */

#include "glimmer/glimmer.h"

#include <math.h>
#include <stdio.h>

/* 这些正是 Luminous 的 lum_application 内部按 Init 级别依序做的事（这里手写一遍） */
#define HELLO_WIN_W 800
#define HELLO_WIN_H 600
#define HELLO_FRAMES 240        /* 跑够帧数自动退出，便于自动化验证 */

int main(void) {
  /* ── 启动：依赖顺序与 lum_application 的 g_init_level 一致 ── */
  if (!lum_log_init()) { fprintf(stderr, "glimmer_hello: log init failed\n"); return 1; }
  /* Debug 级别的通道噪音大：这里只留 Info 以上，输出干净 */
  lum_log_set_min_level(Lum_Log_Level_Info);

  if (!lum_window_init("glimmer hello (standalone: links glimmer only)", HELLO_WIN_W, HELLO_WIN_H)) {
    fprintf(stderr, "glimmer_hello: window init failed\n");
    return 1;
  }
  if (!lum_input_init()) { fprintf(stderr, "glimmer_hello: input init failed\n"); return 1; }
  if (!lum_time_init())  { fprintf(stderr, "glimmer_hello: time init failed\n");  return 1; }

  const uint32_t fb_w = lum_window_get_framebuffer_width();
  const uint32_t fb_h = lum_window_get_framebuffer_height();
  if (!lum_renderer2d_init(fb_w, fb_h)) {
    fprintf(stderr, "glimmer_hello: renderer init failed\n");
    return 1;
  }
  lum_renderer2d_set_clear_color(lum_vec4_create(0.06f, 0.07f, 0.10f, 1.0f));
  lum_renderer2d_set_blend_mode(Lum_Blend_Alpha);

  lum_Camera2d cam;
  lum_camera2d_init(&cam, (float)fb_w, (float)fb_h);
  cam.position = lum_vec2_0();
  cam.zoom = 1.0f;

  /* 字体与纹理都走 glimmer 自己的能力（lum_assets 是引擎高层的资源缓存，这里不能用，
     否则就不是"只链接 glimmer"了）*/
  static lum_Font font;
  const bool have_font = lum_tr_load_font(&font, "resources/FiraCode-Regular.ttf", 16) != 0;
  if (!have_font) Lum_Log_Warn("glimmer_hello: font load failed, text disabled");

  static lum_Texture2d white;
  if (!lum_texture2d_create_white(&white)) {
    fprintf(stderr, "glimmer_hello: white texture creation failed\n");
    return 1;
  }

  Lum_Log_Info("glimmer_hello: standalone platform up (%ux%u, %d frames)", fb_w, fb_h, HELLO_FRAMES);

  /* ── 帧循环 ── */
  int frame = 0;
  while (lum_window_is_open() && frame < HELLO_FRAMES) {
    lum_time_begin_frame();
    lum_window_poll_events();
    lum_input_update();
    lum_time_update();
    const float dt = lum_time_get_delta();
    (void)dt;

    lum_renderer2d_set_camera(&cam);
    lum_renderer2d_begin_frame();
    lum_renderer2d_clear();

    /* 旋转方块：走 Transform2d → 矩阵，覆盖数学层 */
    const float angle = (float)frame * 0.02f;
    lum_Transform2d t = lum_transform2d_create(lum_vec2_create(-160.0f, 0.0f),
                                               lum_vec2_create(90.0f, 90.0f),
                                               lum_vec2_1(), angle, lum_vec2_0());
    lum_renderer2d_draw_sprite_matrix_uv(&white, lum_transform2d_get_matrix(&t),
                                         lum_vec4_create(0.0f, 0.0f, 1.0f, 1.0f), 1,
                                         lum_vec4_create(0.55f, 0.80f, 1.00f, 1.0f));

    /* 圆盘 + 空心环：覆盖圆盘着色器与逐图元半径 */
    lum_renderer2d_draw_circle(lum_vec2_create(0.0f, 40.0f), 46.0f + 10.0f * sinf(angle * 3.0f), 2,
                               lum_vec4_create(1.00f, 0.80f, 0.35f, 1.0f));
    lum_renderer2d_draw_circle(lum_vec2_create(0.0f, 40.0f), 20.0f, 3,
                               lum_vec4_create(0.10f, 0.12f, 0.16f, 1.0f));

    /* 线 + 矩形：覆盖最基础的图元路径 */
    lum_renderer2d_draw_line(lum_vec2_create(-300.0f, -200.0f), lum_vec2_create(300.0f, -200.0f),
                             3.0f, 4, lum_vec4_create(0.45f, 1.00f, 0.60f, 0.9f));
    for (int i = 0; i < 8; ++i) {
      const float x = -280.0f + (float)i * 80.0f;
      lum_renderer2d_draw_rect(lum_vec2_create(x, 160.0f), lum_vec2_create(56.0f, 24.0f),
                               0.0f, lum_vec2_0(), 5,
                               lum_vec4_create(0.2f + 0.1f * (float)i, 0.7f, 0.9f - 0.1f * (float)i, 1.0f));
    }

    /* 文字：屏幕空间（像素坐标、左上原点），走 glimmer 的字体图集 */
    if (have_font) {
      lum_tr_draw_screen(&font, "glimmer standalone: links libglimmer.a only",
                         lum_vec2_create(20.0f, 20.0f), 6, lum_vec4_create(0.92f, 0.94f, 0.98f, 1.0f));
      char line[96];
      snprintf(line, sizeof(line), "frame %d   dt %.4f   draw_calls %u", frame, (double)dt,
               (unsigned)lum_renderer2d_get_draw_call_count());
      lum_tr_draw_screen(&font, line, lum_vec2_create(20.0f, 44.0f), 6,
                         lum_vec4_create(0.62f, 0.80f, 1.00f, 1.0f));
    }

    lum_renderer2d_end_frame();
    lum_window_swap_buffers();

    lum_time_end_frame();
    ++frame;
  }

  /* ── 退出：严格逆序 ── */
  Lum_Log_Info("glimmer_hello: %d frames rendered, shutting down", frame);
  lum_texture2d_destroy(&white);
  if (have_font) lum_tr_destroy_font(&font);
  lum_renderer2d_shutdown();
  lum_time_shutdown();
  lum_input_shutdown();
  lum_window_shutdown();
  lum_log_shutdown();
  return 0;
}

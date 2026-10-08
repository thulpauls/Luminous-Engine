/* glimmer_hello3d —— glimmer 3D 渲染器的最小示例（M1）。
 *
 * 与 glimmer_hello 一样**只链接 libglimmer.a**（不碰 Luminous），
 * 但这次画的是 3D：两个【前后重叠】的立方体 + 轨道相机 + 深度测试开关。
 *
 * 为什么是"两个重叠 + 开关"而不是一个方块：
 *   只画一个方块，无论深度测试有没有生效，画面都一样 —— 那种示例证明不了任何事。
 *   两个方块叠在同一片像素上、并且【先画近的、后画远的】，于是：
 *     · 深度测试开 → 重叠区显示近方块的颜色（近的先画，远的被深度挡掉）
 *     · 深度测试关 → 重叠区显示后方块的颜色（后画的覆盖上去）
 *   [D] 就在这两个状态之间切。这正是 docs/design/glimmer3d.md 里 M1 的验收判据。
 *
 * 操作：[D] 深度测试开关   [空格] 暂停/继续轨道   [Q]/[ESC] 退出
 * 跑满 240 帧自动退出（便于自动化冒烟）。
 */

#include "glimmer/glimmer.h"

#include <math.h>
#include <stdio.h>

#define HELLO3D_WIN_W 960
#define HELLO3D_WIN_H 600
#define HELLO3D_FRAMES 240

int main(void) {
  if (!lum_log_init()) { fprintf(stderr, "glimmer_hello3d: log init failed\n"); return 1; }
  lum_log_set_min_level(Lum_Log_Level_Info);

  if (!lum_window_init("glimmer hello3d (depth-tested cubes, glimmer only)", HELLO3D_WIN_W, HELLO3D_WIN_H)) {
    fprintf(stderr, "glimmer_hello3d: window init failed\n");
    return 1;
  }
  if (!lum_input_init()) { fprintf(stderr, "glimmer_hello3d: input init failed\n"); return 1; }
  if (!lum_time_init())  { fprintf(stderr, "glimmer_hello3d: time init failed\n");  return 1; }
  if (!lum_renderer3d_init()) {
    fprintf(stderr, "glimmer_hello3d: 3D renderer init failed\n");
    return 1;
  }

  const uint32_t fb_w = lum_window_get_framebuffer_width();
  const uint32_t fb_h = lum_window_get_framebuffer_height();
  lum_renderer3d_set_viewport(0, 0, (int)fb_w, (int)fb_h);

  lum_Camera3d cam;
  lum_camera3d_init(&cam, 1.0f);
  lum_camera3d_set_viewport(&cam, (float)fb_w, (float)fb_h);
  cam.target = lum_vec3_0();

  lum_Mesh near_box, far_box;
  if (!lum_mesh_create_box(&near_box, lum_vec3_create(0.62f, 0.62f, 0.62f)) ||
      !lum_mesh_create_box(&far_box,  lum_vec3_create(0.95f, 0.95f, 0.95f))) {
    fprintf(stderr, "glimmer_hello3d: mesh creation failed\n");
    return 1;
  }

  Lum_Log_Info("glimmer_hello3d: up (%ux%u), 2 depth-tested cubes, [D] toggles depth test",
               fb_w, fb_h);

  /* M3 起绘制要传材质而不是颜色。这里两个方块各一个纯色材质（没有贴图 → 渲染器用内置白纹理） */
  lum_Material3d near_mat = lum_material3d_create(lum_vec4_create(0.95f, 0.42f, 0.28f, 1.0f));
  lum_Material3d far_mat  = lum_material3d_create(lum_vec4_create(0.28f, 0.58f, 0.95f, 1.0f));

  int frame = 0;
  float yaw = 0.6f;
  bool paused = false;

  while (lum_window_is_open() && frame < HELLO3D_FRAMES) {
    lum_time_begin_frame();
    lum_window_poll_events();
    lum_input_update();
    lum_time_update();
    const float dt = lum_time_get_delta();

    if (lum_input_is_key_pressed(Lum_Key_D))      lum_renderer3d_set_depth_test(!lum_renderer3d_depth_test_enabled());
    if (lum_input_is_key_pressed(Lum_Key_Space))  paused = !paused;
    if (lum_input_is_key_pressed(Lum_Key_Q) || lum_input_is_key_pressed(Lum_Key_Escape)) break;

    if (!paused) yaw += dt * 0.6f;
    lum_camera3d_orbit(&cam, yaw, 0.32f, 5.2f);

    /* 每帧同步视口与宽高比：窗口一旦被拉伸，视口和 aspect 都得跟上，
       否则画面缩在一角（只改了视口而忘了 aspect 就会拉伸变形）。宽高比是投影矩阵的输入，
       所以它一变，set_camera 必须跟着重算 VP —— 顺序不能颠倒。 */
    const uint32_t fw = lum_window_get_framebuffer_width();
    const uint32_t fh = lum_window_get_framebuffer_height();
    lum_renderer3d_set_viewport(0, 0, (int)fw, (int)fh);
    lum_camera3d_set_viewport(&cam, (float)fw, (float)fh);

    lum_renderer3d_begin_frame();
    lum_renderer3d_set_camera(&cam);
    lum_renderer3d_clear(lum_vec4_create(0.05f, 0.06f, 0.09f, 1.0f));

    /* 先近后远：这样"关掉深度测试"时重叠区会被远处的方块盖住，A/B 才有对比 */
    lum_renderer3d_draw_mesh(&near_box, &near_mat, lum_mat4_translate(0.0f, 0.0f, 0.6f));
    lum_renderer3d_draw_mesh(&far_box,  &far_mat,  lum_mat4_translate(0.0f, 0.0f, -2.2f));

    lum_renderer3d_end_frame();
    lum_window_swap_buffers();
    lum_time_end_frame();
    ++frame;
  }

  lum_Renderer3d_stats stats = *lum_renderer3d_get_stats();
  printf("glimmer_hello3d: %d frames, last frame: draw_calls=%u triangles=%u depth_test=%d\n",
         frame, (unsigned)stats.draw_calls, (unsigned)stats.triangles,
         (int)lum_renderer3d_depth_test_enabled());
  Lum_Log_Info("glimmer_hello3d: done, shutting down");

  lum_mesh_destroy(&far_box);
  lum_mesh_destroy(&near_box);
  lum_renderer3d_shutdown();
  lum_time_shutdown();
  lum_input_shutdown();
  lum_window_shutdown();
  lum_log_shutdown();
  return 0;
}

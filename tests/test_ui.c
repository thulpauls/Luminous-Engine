/* 保留式 UI 纯逻辑测试：布局数学 / 命中路由 / 焦点链 / 属性语义。
   无 GL 运行（渲染器未初始化时绘制调用安全空转）。由 CMake 目标 test_ui 运行。 */
#include "lum_ui.h"
#include <stdio.h>
#include <string.h>

static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { \
  if (cond) { ++g_pass; } \
  else { ++g_fail; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

int main(void) {
  CHECK(lum_ui_init(NULL));                      /* 无字体：布局按默认行高 */

  lum_Widget* root = lum_ui_root();
  lum_ui_set_fixed_size(root, lum_vec2_create(400, 300));

  /* 面板 + 两个固定尺寸按钮（纵向堆叠） */
  lum_Widget* panel = lum_ui_box(root, false);
  lum_ui_set_position(panel, lum_vec2_create(10, 10));
  lum_ui_set_fixed_size(panel, lum_vec2_create(200, 150));

  lum_Widget* b0 = lum_ui_button(panel, "b0");
  lum_ui_set_fixed_size(b0, lum_vec2_create(100, 24));
  lum_Widget* b1 = lum_ui_button(panel, "b1");
  lum_ui_set_fixed_size(b1, lum_vec2_create(100, 24));

  lum_ui_frame();   /* 布局 + 绘制（渲染器未初始化 → 绘制安全空转） */

  /* 布局数学：panel 在 (10,10)；按钮纵向堆叠，padding 8 / spacing 4 / fixed_size 覆盖宽度 */
  CHECK(panel->rect.position.x == 10 && panel->rect.position.y == 10);
  CHECK(b0->rect.position.x == 18 && b0->rect.position.y == 18);
  CHECK(b0->rect.size.x == 100 && b0->rect.size.y == 24);          /* fixed_size 覆盖填满 */
  CHECK(b1->rect.position.y == 18 + 24 + 4);                        /* spacing 4 */

  /* 命中路由：按钮上 → 按钮；panel 边缘 → panel；树外 → NULL */
  CHECK(lum_ui_widget_at(lum_vec2_create(50, 30)) == b0);
  CHECK(lum_ui_widget_at(lum_vec2_create(15, 155)) == panel);
  CHECK(lum_ui_widget_at(lum_vec2_create(500, 500)) == NULL);

  /* set_text 深拷贝：源缓冲变更不影响控件 */
  char buf[32] = "hello";
  lum_ui_set_text(b0, buf);
  buf[0] = 'X';
  CHECK(strcmp(lum_ui_get_text(b0), "hello") == 0);

  /* visible=false：退出布局与命中；b1 上移补位 */
  lum_ui_set_visible(b0, false);
  lum_ui_frame();
  CHECK(b1->rect.position.y == 18);                /* b0 隐藏后 b1 上移补位 */
  lum_ui_set_visible(b0, true);
  lum_ui_frame();

  /* 值语义：slider 钳制 / checkbox 翻转 */
  lum_Widget* s = lum_ui_slider(panel, "s", 50.0f, 0.0f, 100.0f);
  lum_ui_set_value(s, 250.0f);
  CHECK(lum_ui_get_value(s) == 100.0f);            /* 超上界钳制 */
  lum_ui_set_value(s, -5.0f);
  CHECK(lum_ui_get_value(s) == 0.0f);              /* 超下界钳制 */

  lum_Widget* c = lum_ui_checkbox(panel, "c", false);
  lum_ui_set_checked(c, true);
  CHECK(lum_ui_get_checked(c) == true);

  /* 输入吞噬：hover/capture/focus 三源（鼠标停在 (0,0)，用控件挪位模拟悬停）。
     路由先于布局：set_position 后需两帧，命中测试才见到新矩形 */
  lum_ui_frame();
  CHECK(lum_ui_hovered() == NULL);              /* 根表面不算悬停，空白处为 NULL */
  CHECK(lum_ui_input_captured() == false);      /* 无悬停 / 捕获 / 焦点 */

  lum_ui_set_position(panel, lum_vec2_create(0, 0));   /* 面板挪到光标下 */
  lum_ui_frame();                               /* 帧1：布局应用新矩形（路由读到的还是旧矩形） */
  lum_ui_frame();                               /* 帧2：路由命中面板 */
  CHECK(lum_ui_hovered() == panel);
  CHECK(lum_ui_input_captured() == true);       /* 悬停控件 → 输入被 UI 占用 */
  lum_ui_set_position(panel, lum_vec2_create(10, 10));   /* 挪走 → 释放 */
  lum_ui_frame();
  lum_ui_frame();
  CHECK(lum_ui_hovered() == NULL);
  CHECK(lum_ui_input_captured() == false);

  /* 滚动容器：内容测量 / 程序化滚动钳制 / 子项随滚动偏移 / 滚出视口不可命中 */
  lum_Widget* scroll = lum_ui_scroll(root);
  lum_ui_set_position(scroll, lum_vec2_create(300, 10));
  lum_ui_set_fixed_size(scroll, lum_vec2_create(80, 80));
  lum_Widget* r0 = lum_ui_button(scroll, "r0");
  lum_ui_set_fixed_size(r0, lum_vec2_create(60, 30));
  lum_Widget* r1 = lum_ui_button(scroll, "r1");
  lum_ui_set_fixed_size(r1, lum_vec2_create(60, 30));
  lum_Widget* r2 = lum_ui_button(scroll, "r2");
  lum_ui_set_fixed_size(r2, lum_vec2_create(60, 30));

  lum_ui_frame();
  /* 内容高 = padding16 + 30×3 + spacing4×2 = 114；视口内高 64 → 最大滚动 50 */
  CHECK(lum_ui_content_height(scroll) == 114.0f);
  CHECK(lum_ui_get_scroll(scroll) == 0.0f);
  CHECK(r0->rect.position.y == 18.0f);                       /* 未滚动：容器顶 + padding */

  lum_ui_set_scroll(scroll, 99.0f);                          /* 越界：钳到 50 */
  lum_ui_frame();
  CHECK(lum_ui_get_scroll(scroll) == 50.0f);
  CHECK(r0->rect.position.y == 10.0f + 8.0f - 50.0f);        /* 子项随滚动上移（滚出视口为负） */
  CHECK(r2->rect.position.y == 10.0f + 8.0f - 50.0f + 30.0f + 4.0f + 30.0f + 4.0f);

  lum_ui_set_scroll(scroll, -5.0f);                          /* 负值：钳到 0 */
  lum_ui_frame();
  CHECK(lum_ui_get_scroll(scroll) == 0.0f);

  /* 滚动到底后：r0 完全滚出视口，其 rect 落在容器矩形外 → 命中不返回 r0 */
  lum_ui_set_scroll(scroll, 50.0f);
  lum_ui_frame();
  lum_Widget* hit_top = lum_ui_widget_at(lum_vec2_create(340, 25));   /* 容器顶部带：r0 已滚出 */
  CHECK(hit_top != r0);
  lum_Widget* hit_visible = lum_ui_widget_at(lum_vec2_create(340, 45)); /* r2 的可见区 */
  CHECK(hit_visible == r2);

  /* 焦点链：focus_next 在可聚焦控件间循环（label/panel 不可聚焦） */
  lum_ui_frame();
  CHECK(lum_ui_focused() == NULL);
  lum_ui_focus_next();
  lum_Widget* f1 = lum_ui_focused();
  CHECK(f1 != NULL);
  CHECK(f1->type == Lum_Widget_Button || f1->type == Lum_Widget_Checkbox ||
        f1->type == Lum_Widget_Slider || f1->type == Lum_Widget_Textbox);   /* 命中的是可聚焦控件 */
  lum_ui_focus_next();
  CHECK(lum_ui_focused() != NULL && lum_ui_focused() != f1);

  lum_ui_shutdown();
  CHECK(lum_ui_root() == NULL);

  printf("ui tests: %d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}

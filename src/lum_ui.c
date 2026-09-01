#include "lum_ui.h"
#include "lum_renderer2d.h"
#include "lum_input.h"
#include "lum_input_codes.h"
#include "lum_window.h"
#include "lum_time.h"
#include "containers/lum_vector.h"
#include "lum_assert.h"
#include "lum_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* ---------- 上下文 ---------- */

typedef struct lum_Ui_context {
  bool initialized;
  lum_Font* font;            /* 可为 NULL：不渲染文字，布局按 text_height 估算 */
  float text_height;
  float window_w, window_h;  /* 公共坐标（左上原点 Y 向下）基准 */

  lum_Widget* root;
  lum_Widget* hover;      /* 本帧悬停 */
  lum_Widget* capture;    /* 按下锁定（保持到释放） */
  lum_Widget* focus;      /* 键盘焦点（Tab 循环 / 点击获得） */

  lum_Vec2 mouse;
  bool mouse_down, mouse_pressed, mouse_released;
  lum_Vec2 scroll;   /* 本帧滚轮增量（滚动容器路由用） */

  bool vp_saved;
  lum_Mat4 saved_vp;
} lum_Ui_context;

static lum_Ui_context g_ui;
lum_Theme lum_default_theme;

/* ---------- 工具 ---------- */

static char* lum__ui_strdup(const char* s) {
  size_t len = strlen(s) + 1;
  char* copy = (char*)malloc(len);
  if (copy) memcpy(copy, s, len);
  return copy;
}

bool lum_ui_point_in_rect(lum_Rect rect, lum_Vec2 point) {
  return point.x >= rect.position.x && point.x < rect.position.x + rect.size.x &&
         point.y >= rect.position.y && point.y < rect.position.y + rect.size.y;
}

float lum_ui_text_height(void) {
  return g_ui.text_height;
}

/* 文本尺寸：有字体用 measure；无字体（测试）按等宽估算 */
static lum_Vec2 lum__ui_text_size(const char* text) {
  if (g_ui.font) return lum_tr_measure_text(g_ui.font, text);
  return lum_vec2_create((float)strlen(text) * g_ui.text_height * 0.6f, g_ui.text_height);
}

/* ---------- 控件创建 ---------- */

static lum_Widget* lum__ui_create(lum_Widget* parent, lum_Widget_type type) {
  lum_Widget* w = (lum_Widget*)calloc(1, sizeof(*w));
  if (!w) return NULL;
  w->type = type;
  w->visible = true;
  w->enabled = true;
  w->color_override = lum_vec4_create(0, 0, 0, -1.0f);   /* a<0 = 用主题 */
  return w;
}

static void lum__ui_attach(lum_Widget* parent, lum_Widget* w) {
  if (parent->child_count == parent->child_capacity) {
    uint32_t cap = parent->child_capacity ? parent->child_capacity * 2u : 4u;
    lum_Widget** children = (lum_Widget**)realloc(parent->children, (size_t)cap * sizeof(*children));
    if (!children) return;
    parent->children = children;
    parent->child_capacity = cap;
  }
  parent->children[parent->child_count++] = w;
  w->parent = parent;
}

lum_Widget* lum_ui_root(void) {
  return g_ui.root;
}

lum_Widget* lum_ui_box(lum_Widget* parent, bool horizontal) {
  if (!parent) return NULL;
  lum_Widget* w = lum__ui_create(parent, Lum_Widget_Box);
  if (!w) return NULL;
  w->horizontal = horizontal;
  lum__ui_attach(parent, w);
  return w;
}

lum_Widget* lum_ui_label(lum_Widget* parent, const char* text) {
  if (!parent) return NULL;
  lum_Widget* w = lum__ui_create(parent, Lum_Widget_Label);
  if (!w) return NULL;
  w->text = lum__ui_strdup(text ? text : "");
  lum__ui_attach(parent, w);
  return w;
}

lum_Widget* lum_ui_button(lum_Widget* parent, const char* label) {
  if (!parent) return NULL;
  lum_Widget* w = lum__ui_create(parent, Lum_Widget_Button);
  if (!w) return NULL;
  w->text = lum__ui_strdup(label ? label : "");
  lum__ui_attach(parent, w);
  return w;
}

lum_Widget* lum_ui_checkbox(lum_Widget* parent, const char* label, bool checked) {
  if (!parent) return NULL;
  lum_Widget* w = lum__ui_create(parent, Lum_Widget_Checkbox);
  if (!w) return NULL;
  w->text = lum__ui_strdup(label ? label : "");
  w->checked = checked;
  lum__ui_attach(parent, w);
  return w;
}

lum_Widget* lum_ui_slider(lum_Widget* parent, const char* label,
                             float value, float min, float max) {
  if (!parent) return NULL;
  lum_Widget* w = lum__ui_create(parent, Lum_Widget_Slider);
  if (!w) return NULL;
  w->text = lum__ui_strdup(label ? label : "");
  w->value_min = min;
  w->value_max = (max > min) ? max : min + 1.0f;
  w->value = (value < min) ? min : (value > w->value_max ? w->value_max : value);
  lum__ui_attach(parent, w);
  return w;
}

lum_Widget* lum_ui_progress(lum_Widget* parent, float value) {
  if (!parent) return NULL;
  lum_Widget* w = lum__ui_create(parent, Lum_Widget_Progress);
  if (!w) return NULL;
  w->value_min = 0.0f;
  w->value_max = 1.0f;
  lum_ui_set_value(w, value);   /* set_value 按 min/max 钳制，对 Progress 同样适用 */
  lum__ui_attach(parent, w);
  return w;
}

lum_Widget* lum_ui_textbox(lum_Widget* parent, const char* initial_text) {
  if (!parent) return NULL;
  lum_Widget* w = lum__ui_create(parent, Lum_Widget_Textbox);
  if (!w) return NULL;
  w->text = lum__ui_strdup(initial_text ? initial_text : "");
  lum__ui_attach(parent, w);
  return w;
}

/* ---------- 属性 ---------- */

void lum_ui_set_text(lum_Widget* w, const char* text) {
  Lum_Assert(w);
  free(w->text);
  w->text = lum__ui_strdup(text ? text : "");
}

const char* lum_ui_get_text(const lum_Widget* w) {
  Lum_Assert(w);
  return w->text;
}

void lum_ui_set_visible(lum_Widget* w, bool visible) { Lum_Assert(w); w->visible = visible; }
void lum_ui_set_enabled(lum_Widget* w, bool enabled) { Lum_Assert(w); w->enabled = enabled; }
void lum_ui_set_position(lum_Widget* w, lum_Vec2 position) { Lum_Assert(w); w->position = position; }
void lum_ui_set_fixed_size(lum_Widget* w, lum_Vec2 size) { Lum_Assert(w); w->fixed_size = size; }

void lum_ui_set_checked(lum_Widget* w, bool checked) {
  Lum_Assert(w && w->type == Lum_Widget_Checkbox);
  w->checked = checked;
}

bool lum_ui_get_checked(const lum_Widget* w) {
  Lum_Assert(w && w->type == Lum_Widget_Checkbox);
  return w->checked;
}

void lum_ui_set_value(lum_Widget* w, float value) {
  Lum_Assert(w && (w->type == Lum_Widget_Slider || w->type == Lum_Widget_Progress));
  if (value < w->value_min) value = w->value_min;
  if (value > w->value_max) value = w->value_max;
  w->value = value;   /* 程序化设值不触发回调 */
}

float lum_ui_get_value(const lum_Widget* w) {
  Lum_Assert(w && (w->type == Lum_Widget_Slider || w->type == Lum_Widget_Progress));
  return w->value;
}

lum_Widget* lum_ui_scroll(lum_Widget* parent) {
  if (!parent) return NULL;
  lum_Widget* w = lum__ui_create(parent, Lum_Widget_Scroll);
  if (!w) return NULL;
  lum__ui_attach(parent, w);
  return w;
}

void lum_ui_set_scroll(lum_Widget* w, float y) {
  Lum_Assert(w && w->type == Lum_Widget_Scroll);
  w->scroll_y = y;   /* 滚动范围在布局期钳制 */
}

float lum_ui_get_scroll(const lum_Widget* w) {
  Lum_Assert(w && w->type == Lum_Widget_Scroll);
  return w->scroll_y;
}

float lum_ui_content_height(const lum_Widget* w) {
  Lum_Assert(w && w->type == Lum_Widget_Scroll);
  return w->content_height;
}

void lum_ui_on_click(lum_Widget* w, lum_Ui_click_fn cb, void* user) {
  Lum_Assert(w);
  w->on_click = cb;
  w->user = user;
}

void lum_ui_on_value_changed(lum_Widget* w, lum_Ui_value_fn cb, void* user) {
  Lum_Assert(w);
  w->on_value_changed = cb;
  w->user = user;
}

/* ---------- 生命周期 ---------- */

int lum_ui_init(lum_Font* font) {
  memset(&g_ui, 0, sizeof(g_ui));
  g_ui.font = font;

  if (font) {
    lum_Vec2 metrics = lum_tr_measure_text(font, "Ag");
    g_ui.text_height = metrics.y;
  }
  if (g_ui.text_height <= 0.0f) g_ui.text_height = 18.0f;

  lum_default_theme = (lum_Theme){
    .panel_bg       = lum_vec4_create(0.08f, 0.09f, 0.12f, 0.92f),
    .panel_border   = lum_vec4_create(0.30f, 0.34f, 0.42f, 1.0f),
    .button_bg      = lum_vec4_create(0.16f, 0.18f, 0.24f, 1.0f),
    .button_hover   = lum_vec4_create(0.22f, 0.26f, 0.34f, 1.0f),
    .button_pressed = lum_vec4_create(0.12f, 0.30f, 0.52f, 1.0f),
    .button_disabled= lum_vec4_create(0.12f, 0.13f, 0.16f, 1.0f),
    .text           = lum_vec4_create(0.88f, 0.90f, 0.94f, 1.0f),
    .text_disabled  = lum_vec4_create(0.45f, 0.47f, 0.52f, 1.0f),
    .accent         = lum_vec4_create(0.25f, 0.55f, 0.95f, 1.0f),
    .slider_track   = lum_vec4_create(0.16f, 0.18f, 0.24f, 1.0f),
    .padding        = 8.0f,
    .spacing        = 4.0f,
  };

  g_ui.root = lum__ui_create(NULL, Lum_Widget_Box);   /* 根：绝对定位容器 */
  if (!g_ui.root) return 0;
  g_ui.initialized = true;
  return 1;
}

static void lum__ui_free_tree(lum_Widget* w) {
  if (!w) return;
  for (uint32_t i = 0; i < w->child_count; ++i) {
    lum__ui_free_tree(w->children[i]);
  }
  free(w->text);
  free(w->children);
  free(w);
}

void lum_ui_shutdown(void) {
  if (!g_ui.initialized) return;
  lum__ui_free_tree(g_ui.root);
  memset(&g_ui, 0, sizeof(g_ui));
}

/* ---------- 布局（堆叠单遍：子项期望尺寸随排布即时计算） ---------- */

static lum_Vec2 lum__ui_measure(lum_Widget* w) {
  if (w->type == Lum_Widget_Scroll) {
    /* 内容高 = 纵向堆叠式（同 Box 主轴）；自身尺寸 fixed 优先，未定视口高则退化为内容高（不滚动）。
       content_height 是布局输出：measure 会先于 arrange 被父级与自身各调一次，重复写幂等 */
    float content = lum_default_theme.padding * 2.0f;
    float cross_max = 0.0f;
    uint32_t visible_count = 0;
    for (uint32_t i = 0; i < w->child_count; ++i) {
      if (!w->children[i]->visible) continue;
      lum_Vec2 cs = lum__ui_measure(w->children[i]);
      if (visible_count) content += lum_default_theme.spacing;
      content += cs.y;
      if (cs.x > cross_max) cross_max = cs.x;
      visible_count++;
    }
    w->content_height = content;
    float viewport_x = (w->fixed_size.x > 0) ? w->fixed_size.x : cross_max + lum_default_theme.padding * 2.0f;
    float viewport_y = (w->fixed_size.y > 0) ? w->fixed_size.y : content;
    return lum_vec2_create(viewport_x, viewport_y);
  }
  if (w->fixed_size.x > 0.0f || w->fixed_size.y > 0.0f) {
    return lum_vec2_create(w->fixed_size.x > 0 ? w->fixed_size.x : 0.0f, w->fixed_size.y > 0 ? w->fixed_size.y : 0.0f);
  }
  switch (w->type) {
    case Lum_Widget_Box: {
      /* 主轴 = padding×2 + Σ子项 + spacing×(n-1)；交叉轴 = padding×2 + 最大子项。
         两个轴都必须计入 padding——否则水平盒测得的高度会缺 padding，压扁子项 */
      float main_total = lum_default_theme.padding * 2.0f;
      float cross_max = 0.0f;
      uint32_t visible_count = 0;
      for (uint32_t i = 0; i < w->child_count; ++i) {
        if (!w->children[i]->visible) continue;
        lum_Vec2 cs = lum__ui_measure(w->children[i]);
        if (visible_count) {
          main_total += lum_default_theme.spacing;
        }
        if (w->horizontal) {
          main_total += cs.x;
          if (cs.y > cross_max) cross_max = cs.y;
        } else {
          main_total += cs.y;
          if (cs.x > cross_max) cross_max = cs.x;
        }
        visible_count++;
      }
      if (w->horizontal)
        return lum_vec2_create(main_total, cross_max + lum_default_theme.padding * 2.0f);
      return lum_vec2_create(cross_max + lum_default_theme.padding * 2.0f, main_total);
    }
    case Lum_Widget_Label:
      return lum__ui_text_size(w->text);
    case Lum_Widget_Button: {
      lum_Vec2 ts = lum__ui_text_size(w->text);
      return lum_vec2_create(ts.x + lum_default_theme.padding * 2.0f + 12.0f,
                             ts.y + lum_default_theme.padding);
    }
    case Lum_Widget_Checkbox: {
      lum_Vec2 ts = lum__ui_text_size(w->text);
      return lum_vec2_create(g_ui.text_height + 8.0f + ts.x, g_ui.text_height);
    }
    case Lum_Widget_Slider: {
      lum_Vec2 ts = lum__ui_text_size(w->text);
      float w = (ts.x > 120.0f) ? ts.x : 120.0f;
      return lum_vec2_create(w, g_ui.text_height + 12.0f);
    }
    case Lum_Widget_Textbox: {
      lum_Vec2 ts = lum__ui_text_size(w->text);
      float w = (ts.x > 120.0f) ? ts.x : 120.0f;
      return lum_vec2_create(w + lum_default_theme.padding * 2.0f, g_ui.text_height + 12.0f);
    }
    case Lum_Widget_Progress:
      return lum_vec2_create(120.0f, g_ui.text_height + 4.0f);
  }
  return lum_vec2_create(0.0f, 0.0f);
}

/* 排布单控件及其子树（内部坐标尚未翻转，翻转发生在绘制转换） */
static void lum__ui_arrange(lum_Widget* w, lum_Rect assigned) {
  w->rect = assigned;
  if (w->type != Lum_Widget_Box && w->type != Lum_Widget_Scroll) return;

  /* Scroll：滚动范围在此钳制（滚轮/程序化统一收口），子项整体上移 scroll_y，溢出交给绘制剪裁 */
  float scroll = 0.0f;
  float inner_h = assigned.size.y - lum_default_theme.padding * 2.0f;
  if (w->type == Lum_Widget_Scroll) {
    float max_scroll = (w->content_height > inner_h) ? w->content_height - inner_h : 0.0f;
    scroll = w->scroll_y;
    if (scroll > max_scroll) scroll = max_scroll;
    if (scroll < 0.0f) scroll = 0.0f;
    w->scroll_y = scroll;
  }

  float cursor = assigned.position.y + lum_default_theme.padding - scroll;
  float cross_x = assigned.position.x + lum_default_theme.padding;
  float inner_w = assigned.size.x - lum_default_theme.padding * 2.0f;

  for (uint32_t i = 0; i < w->child_count; ++i) {
    lum_Widget* c = w->children[i];
    if (!c->visible) continue;
    lum_Vec2 cs = lum__ui_measure(c);

    lum_Rect child_rect;
    if (w->horizontal) {
      child_rect = (lum_Rect){ {cross_x, assigned.position.y + lum_default_theme.padding}, {cs.x, inner_h} };
      if (c->fixed_size.y > 0) child_rect.size.y = c->fixed_size.y;
      cross_x += cs.x + lum_default_theme.spacing;
    } else {
      child_rect = (lum_Rect){ {assigned.position.x + lum_default_theme.padding, cursor}, {inner_w, cs.y} };
      if (c->fixed_size.x > 0) child_rect.size.x = c->fixed_size.x;
      cursor += cs.y + lum_default_theme.spacing;
    }
    lum__ui_arrange(c, child_rect);
  }
}

/* 绝对容器（根）：子项按 position + fixed_size/测量 摆放 */
static void lum__ui_arrange_root(lum_Widget* root, float window_w, float window_h) {
  root->rect = (root->fixed_size.x > 0 || root->fixed_size.y > 0)
      ? (lum_Rect){ {0, 0}, {root->fixed_size.x > 0 ? root->fixed_size.x : window_w,
                             root->fixed_size.y > 0 ? root->fixed_size.y : window_h} }
      : (lum_Rect){ {0, 0}, {window_w, window_h} };

  for (uint32_t i = 0; i < root->child_count; ++i) {
    lum_Widget* c = root->children[i];
    if (!c->visible) continue;
    lum_Vec2 cs = lum__ui_measure(c);
    lum_Rect r = { c->position,
                   {c->fixed_size.x > 0 ? c->fixed_size.x : cs.x,
                    c->fixed_size.y > 0 ? c->fixed_size.y : cs.y} };
    lum__ui_arrange(c, r);
  }
}

/* ---------- 输入路由 ---------- */

static lum_Widget* lum__ui_hit(lum_Widget* w, lum_Vec2 p) {
  if (!w->visible) return NULL;
  if (!lum_ui_point_in_rect(w->rect, p)) return NULL;
  for (int i = (int)w->child_count - 1; i >= 0; --i) {
    lum_Widget* hit = lum__ui_hit(w->children[i], p);
    if (hit) return hit;
  }
  return w;
}

lum_Widget* lum_ui_widget_at(lum_Vec2 point) {
  if (!g_ui.initialized || !g_ui.root) return NULL;
  return lum__ui_hit(g_ui.root, point);
}

/* 绘制转换：公共矩形（左上原点）→ 内部 Y 向上绘制矩形 */
static void lum__ui_draw_rect(lum_Rect r, lum_Vec4 color) {
  lum_renderer2d_draw_rect(lum_vec2_create(r.position.x, g_ui.window_h - r.position.y - r.size.y),
                           r.size, 0.0f, lum_vec2_0(), Lum_Ui_Layer, color);
}

static void lum__ui_frame_rect(lum_Rect r, lum_Vec4 bg, lum_Vec4 border) {
  lum__ui_draw_rect(r, bg);
  const float t = 1.0f;
  lum__ui_draw_rect((lum_Rect){ {r.position.x, r.position.y}, {r.size.x, t} }, border);
  lum__ui_draw_rect((lum_Rect){ {r.position.x, r.position.y + r.size.y - t}, {r.size.x, t} }, border);
  lum__ui_draw_rect((lum_Rect){ {r.position.x, r.position.y}, {t, r.size.y} }, border);
  lum__ui_draw_rect((lum_Rect){ {r.position.x + r.size.x - t, r.position.y}, {t, r.size.y} }, border);
}

static void lum__ui_draw_text(lum_Vec2 top_left, const char* text, lum_Vec4 color) {
  if (!g_ui.font) return;
  float baseline = g_ui.window_h - top_left.y - lum_tr_ascent(g_ui.font);
  lum_tr_draw_layer(g_ui.font, text, lum_vec2_create(top_left.x, baseline), Lum_Ui_Layer, color);
}

/* 剪裁换算：UI 公共矩形（逻辑像素、左上原点）→ 渲染目标像素（左下原点、framebuffer 尺寸）。
   无头环境（测试/无窗口）静默不裁 */
static void lum__ui_push_scissor(lum_Rect r) {
  float fb_w = (float)lum_window_get_framebuffer_width();
  float fb_h = (float)lum_window_get_framebuffer_height();
  if (fb_w <= 0.0f || fb_h <= 0.0f || g_ui.window_w <= 0.0f) return;
  float scale = fb_w / g_ui.window_w;
  lum_renderer2d_set_scissor((int)(r.position.x * scale),
                             (int)(fb_h - (r.position.y + r.size.y) * scale),
                             (int)(r.size.x * scale),
                             (int)(r.size.y * scale));
}

static void lum__ui_pop_scissor(void) {
  lum_renderer2d_clear_scissor();
}

/* ---------- 绘制 ---------- */

static void lum__ui_draw(lum_Widget* w) {
  if (!w->visible) return;

  switch (w->type) {
    case Lum_Widget_Box:
      if (w->parent)   /* 根不画背景 */
        lum__ui_frame_rect(w->rect, lum_default_theme.panel_bg, lum_default_theme.panel_border);
      break;

    case Lum_Widget_Scroll: {
      if (w->parent)
        lum__ui_frame_rect(w->rect, lum_default_theme.panel_bg, lum_default_theme.panel_border);

      /* 子项画进内容区剪裁（右侧留出滚动条轨道）；本 case 自行递归后返回 */
      const float scrollbar_w = 5.0f;
      lum_Rect clip = { {w->rect.position.x + lum_default_theme.padding,
                         w->rect.position.y + lum_default_theme.padding},
                        {w->rect.size.x - lum_default_theme.padding * 2.0f - scrollbar_w,
                         w->rect.size.y - lum_default_theme.padding * 2.0f} };
      lum__ui_push_scissor(clip);
      for (uint32_t i = 0; i < w->child_count; ++i) {
        lum__ui_draw(w->children[i]);
      }
      lum__ui_pop_scissor();

      /* 滚动条指示器（v1 纯指示）：右侧轨道 + 视口占比滑块 */
      float viewport_h = w->rect.size.y - lum_default_theme.padding * 2.0f;
      if (w->content_height > viewport_h && w->content_height > 0.0f) {
        float track_x = w->rect.position.x + w->rect.size.x - lum_default_theme.padding - scrollbar_w;
        float track_y = w->rect.position.y + lum_default_theme.padding;
        float thumb_h = viewport_h * viewport_h / w->content_height;
        if (thumb_h < 8.0f) thumb_h = 8.0f;
        float max_scroll = w->content_height - viewport_h;
        float thumb_y = track_y + (w->scroll_y / max_scroll) * (viewport_h - thumb_h);
        lum__ui_draw_rect((lum_Rect){ {track_x, track_y}, {scrollbar_w, viewport_h} },
                          lum_default_theme.slider_track);
        lum__ui_draw_rect((lum_Rect){ {track_x, thumb_y}, {scrollbar_w, thumb_h} },
                          lum_default_theme.accent);
      }
      return;   /* 子项已绘制：跳过函数尾部通用子项递归 */
    }

    case Lum_Widget_Label: {
      lum_Vec4 color = (w->color_override.w >= 0.0f) ? w->color_override : lum_default_theme.text;
      lum__ui_draw_text(w->rect.position, w->text, color);
      break;
    }

    case Lum_Widget_Button: {
      lum_Vec4 bg = w->enabled
          ? (w->pressed ? lum_default_theme.button_pressed
                        : (w->hovered ? lum_default_theme.button_hover : lum_default_theme.button_bg))
          : lum_default_theme.button_disabled;
      lum__ui_frame_rect(w->rect, bg, lum_default_theme.panel_border);
      lum_Vec2 ts = lum__ui_text_size(w->text);
      lum_Vec4 tc = w->enabled ? lum_default_theme.text : lum_default_theme.text_disabled;
      if (g_ui.font) {
        lum_Vec2 center = lum_vec2_create(w->rect.position.x + w->rect.size.x * 0.5f,
                                          w->rect.position.y + w->rect.size.y * 0.5f);
        lum__ui_draw_text(lum_vec2_create(center.x - ts.x * 0.5f, center.y - g_ui.text_height * 0.5f),
                          w->text, tc);
      }
      break;
    }

    case Lum_Widget_Checkbox: {
      float box = g_ui.text_height;
      lum_Rect box_rect = { w->rect.position, {box, box} };
      lum__ui_frame_rect(box_rect, w->checked ? lum_default_theme.accent : lum_default_theme.slider_track,
                         w->checked ? lum_default_theme.text : lum_default_theme.accent);
      if (w->checked) {
        lum__ui_draw_rect((lum_Rect){ lum_vec2_add(box_rect.position,
                                      lum_vec2_create(box * 0.25f, box * 0.25f)),
                                      {box * 0.5f, box * 0.5f} }, lum_default_theme.text);
      }
      lum__ui_draw_text(lum_vec2_create(w->rect.position.x + box + 8.0f, w->rect.position.y),
                        w->text, lum_default_theme.text);
      break;
    }

    case Lum_Widget_Slider: {
      char value_text[64];
      snprintf(value_text, sizeof(value_text), "%s  %.3g", w->text ? w->text : "", w->value);
      lum__ui_draw_text(w->rect.position, value_text, lum_default_theme.text);

      float track_h = 4.0f;
      lum_Rect track = { lum_vec2_create(w->rect.position.x,
                                         w->rect.position.y + w->rect.size.y - track_h),
                         {w->rect.size.x, track_h} };
      lum__ui_draw_rect(track, lum_default_theme.slider_track);

      float t_norm = (w->value_max > w->value_min)
          ? (w->value - w->value_min) / (w->value_max - w->value_min) : 0.0f;
      if (t_norm < 0.0f) t_norm = 0.0f;
      if (t_norm > 1.0f) t_norm = 1.0f;
      lum_Rect fill = { track.position, {track.size.x * t_norm, track_h} };
      lum__ui_draw_rect(fill, lum_default_theme.accent);

      float knob = 12.0f;
      lum_Rect knob_rect = { lum_vec2_create(track.position.x + track.size.x * t_norm - knob * 0.5f,
                                             track.position.y + track_h * 0.5f - knob * 0.5f),
                             {knob, knob} };
      lum__ui_frame_rect(knob_rect, w->pressed ? lum_default_theme.button_pressed : lum_default_theme.text,
                         lum_default_theme.panel_border);
      break;
    }

    case Lum_Widget_Progress: {
      lum__ui_draw_rect(w->rect, lum_default_theme.slider_track);
      float t_norm = (w->value_min < w->value_max)
          ? (w->value - w->value_min) / (w->value_max - w->value_min) : 0.0f;
      if (t_norm < 0.0f) t_norm = 0.0f;
      if (t_norm > 1.0f) t_norm = 1.0f;
      lum__ui_draw_rect((lum_Rect){ w->rect.position,
                                    {w->rect.size.x * t_norm, w->rect.size.y} },
                        lum_default_theme.accent);
      break;
    }

    case Lum_Widget_Textbox: {
      lum__ui_frame_rect(w->rect, lum_default_theme.button_bg, lum_default_theme.panel_border);
      lum_Vec2 pad = lum_vec2_create(lum_default_theme.padding,
                                     (w->rect.size.y - g_ui.text_height) * 0.5f);
      lum__ui_draw_text(lum_vec2_add(w->rect.position, pad), w->text, lum_default_theme.text);

      /* 焦点光标：文本末尾的 2px 竖线（1.6s 周期闪烁） */
      if (w->focused) {
        float phase = fmodf(lum_time_get_total(), 1.6f);
        if (phase < 0.8f) {
          lum_Vec2 ts = lum__ui_text_size(w->text);
          lum__ui_draw_rect((lum_Rect){ lum_vec2_add(w->rect.position,
                                          lum_vec2_create(pad.x + ts.x + 2.0f, pad.y)),
                                          {2.0f, g_ui.text_height} }, lum_default_theme.text);
        }
      }
      break;
    }
  }

  /* 键盘焦点环：聚焦控件内缘 2px accent 描边 */
  if (w->focused && w->enabled) {
    const float t = 2.0f;
    lum__ui_draw_rect((lum_Rect){ {w->rect.position.x, w->rect.position.y}, {w->rect.size.x, t} },
                      lum_default_theme.accent);
    lum__ui_draw_rect((lum_Rect){ {w->rect.position.x, w->rect.position.y + w->rect.size.y - t},
                                  {w->rect.size.x, t} }, lum_default_theme.accent);
    lum__ui_draw_rect((lum_Rect){ {w->rect.position.x, w->rect.position.y}, {t, w->rect.size.y} },
                      lum_default_theme.accent);
    lum__ui_draw_rect((lum_Rect){ {w->rect.position.x + w->rect.size.x - t, w->rect.position.y},
                                  {t, w->rect.size.y} }, lum_default_theme.accent);
  }

  for (uint32_t i = 0; i < w->child_count; ++i) {
    lum__ui_draw(w->children[i]);
  }
}

/* ---------- 键盘焦点链 ---------- */

static bool lum__ui_focusable(const lum_Widget* w) {
  return w->visible && w->enabled &&
         (w->type == Lum_Widget_Button || w->type == Lum_Widget_Checkbox ||
          w->type == Lum_Widget_Slider || w->type == Lum_Widget_Textbox);
}

/* 深度优先收集可聚焦控件（树序即 Tab 序） */
static void lum__ui_collect_focusable(lum_Widget* w, lum_Widget** list, uint32_t cap, uint32_t* count) {
  if (!w || !w->visible || *count >= cap) return;
  if (lum__ui_focusable(w)) list[(*count)++] = w;
  for (uint32_t i = 0; i < w->child_count; ++i) {
    lum__ui_collect_focusable(w->children[i], list, cap, count);
  }
}

lum_Widget* lum_ui_focused(void) {
  return g_ui.focus;
}

lum_Widget* lum_ui_hovered(void) {
  return g_ui.hover;   /* 上一帧路由结果；根表面不算，空白处为 NULL */
}

bool lum_ui_input_captured(void) {
  return g_ui.hover != NULL || g_ui.capture != NULL || g_ui.focus != NULL;
}

void lum_ui_focus_next(void) {
  if (!g_ui.root) return;
  lum_Widget* list[64];
  uint32_t count = 0;
  lum__ui_collect_focusable(g_ui.root, list, 64, &count);
  if (count == 0) { g_ui.focus = NULL; return; }

  uint32_t idx = 0;
  if (g_ui.focus) {
    for (uint32_t i = 0; i < count; ++i) {
      if (list[i] == g_ui.focus) { idx = (i + 1) % count; break; }
    }
  }
  g_ui.focus = list[idx];
}

static void lum__ui_set_state(lum_Widget* w, const lum_Widget* hover,
                              const lum_Widget* capture, bool mouse_down) {
  if (!w) return;
  w->hovered = (w == hover);
  w->pressed = (w == capture && mouse_down);
  w->focused = (w == g_ui.focus);
  for (uint32_t i = 0; i < w->child_count; ++i) {
    lum__ui_set_state(w->children[i], hover, capture, mouse_down);
  }
}

void lum_ui_frame(void) {
  if (!g_ui.initialized) return;

  /* 输入快照 */
  g_ui.mouse = lum_input_get_mouse_position();
  g_ui.mouse_down = lum_input_is_mouse_down(Lum_Mouse_Button_Left);
  g_ui.mouse_pressed = lum_input_is_mouse_pressed(Lum_Mouse_Button_Left);
  g_ui.mouse_released = lum_input_is_mouse_released(Lum_Mouse_Button_Left);
  g_ui.scroll = lum_input_get_scroll();

  /* 键盘：Tab 焦点循环；聚焦控件的按键响应（方向键调滑块 / Enter·Space 激活 / textbox 输入） */
  if (lum_input_is_key_pressed(Lum_Key_Tab)) lum_ui_focus_next();

  lum_Widget* focused = g_ui.focus;
  if (focused && (!focused->visible || !focused->enabled)) focused = NULL;

  if (focused && lum_input_is_key_pressed(Lum_Key_Tab) == false) {
    if (focused->type == Lum_Widget_Slider &&
        (lum_input_is_key_pressed(Lum_Key_Left) || lum_input_is_key_pressed(Lum_Key_Right))) {
      float step = (focused->value_max - focused->value_min) * 0.05f;
      float v = focused->value + (lum_input_is_key_pressed(Lum_Key_Right) ? step : -step);
      if (v < focused->value_min) v = focused->value_min;
      if (v > focused->value_max) v = focused->value_max;
      if (v != focused->value) {
        focused->value = v;
        if (focused->on_value_changed) focused->on_value_changed(focused, focused->user);
      }
    }
    if (focused->type != Lum_Widget_Textbox &&
        (lum_input_is_key_pressed(Lum_Key_Enter) || lum_input_is_key_pressed(Lum_Key_Space))) {
      if (focused->type == Lum_Widget_Button && focused->on_click) {
        focused->on_click(focused, focused->user);
      }
      if (focused->type == Lum_Widget_Checkbox) {
        focused->checked = !focused->checked;
        if (focused->on_value_changed) focused->on_value_changed(focused, focused->user);
      }
    }
    if (focused->type == Lum_Widget_Textbox) {
      if (lum_input_is_key_pressed(Lum_Key_Backspace)) {
        size_t len = strlen(focused->text);
        if (len > 0) {
          focused->text[len - 1] = '\0';
          if (focused->on_value_changed) focused->on_value_changed(focused, focused->user);
        }
      }
      for (const char* p = lum_input_text(); *p; ++p) {
        size_t len = strlen(focused->text);
        char* nt = (char*)realloc(focused->text, len + 2);
        if (!nt) break;
        nt[len] = *p;
        nt[len + 1] = '\0';
        focused->text = nt;
        if (focused->on_value_changed) focused->on_value_changed(focused, focused->user);
      }
    }
  }

  float w = (float)lum_window_get_width();
  float h = (float)lum_window_get_height();
  g_ui.window_w = w;
  g_ui.window_h = h;

  /* 交互路由：根是桌面表面不算悬停；按下可聚焦控件获得焦点，点按空白处清除焦点 */
  g_ui.hover = lum__ui_hit(g_ui.root, g_ui.mouse);
  if (g_ui.hover == g_ui.root) g_ui.hover = NULL;

  /* 滚轮路由：hover 链向上找最近的滚动容器，滚动其内容（滚动范围在布局期钳制）。
     悬停在滚动容器上即 input_captured：世界侧相机/热键本就让路，无双重消费 */
  if (g_ui.hover && g_ui.scroll.y != 0.0f) {
    for (lum_Widget* a = g_ui.hover; a != NULL; a = a->parent) {
      if (a->type == Lum_Widget_Scroll) {
        a->scroll_y -= g_ui.scroll.y * 40.0f;   /* 每格 40px：上滚回看顶部 */
        break;
      }
    }
  }

  if (g_ui.mouse_pressed) {
    if (g_ui.hover && g_ui.hover->enabled) {
      g_ui.capture = g_ui.hover;
      g_ui.focus = lum__ui_focusable(g_ui.hover) ? g_ui.hover : NULL;
    } else {
      g_ui.focus = NULL;   /* 空白处 / 禁用控件：焦点释放，键盘归还世界 */
    }
  }
  if (g_ui.capture && !g_ui.mouse_down) {
    bool over = lum_ui_point_in_rect(g_ui.capture->rect, g_ui.mouse);
    if (g_ui.capture->type == Lum_Widget_Button && over && g_ui.capture->on_click) {
      g_ui.capture->on_click(g_ui.capture, g_ui.capture->user);
    }
    if (g_ui.capture->type == Lum_Widget_Checkbox && over && g_ui.capture->on_value_changed) {
      g_ui.capture->checked = !g_ui.capture->checked;
      g_ui.capture->on_value_changed(g_ui.capture, g_ui.capture->user);
    }
    g_ui.capture = NULL;
  }

  /* 滑块拖动（capture 期间持续生效） */
  if (g_ui.capture && g_ui.capture->type == Lum_Widget_Slider && g_ui.mouse_down) {
    lum_Widget* s = g_ui.capture;
    float t = (g_ui.mouse.x - s->rect.position.x) / (s->rect.size.x > 0 ? s->rect.size.x : 1.0f);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float value = s->value_min + (s->value_max - s->value_min) * t;
    if (value != s->value) {
      s->value = value;
      if (s->on_value_changed) s->on_value_changed(s, s->user);
    }
  }

  /* hover/pressed 帧内状态（递归整树） */
  lum__ui_set_state(g_ui.root, g_ui.hover, g_ui.capture, g_ui.mouse_down);

  /* 布局 + 绘制（UI 投影内；命令级 VP 与世界渲染互不干扰） */
  g_ui.vp_saved = true;
  g_ui.saved_vp = lum_renderer2d_view_projection();
  lum_renderer2d_set_view_projection(lum_mat4_ortho(0.0f, w, 0.0f, h, -1.0f, 1.0f));
  lum_renderer2d_set_blend_mode(Lum_Blend_Alpha);

  lum__ui_arrange_root(g_ui.root, w, h);
  lum__ui_draw(g_ui.root);

  lum_renderer2d_set_view_projection(g_ui.saved_vp);
  g_ui.vp_saved = false;
}

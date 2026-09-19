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
  lum_Widget* dropdown;   /* 展开中的 Dropdown 覆盖层（同一时刻至多一个） */

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
  (void)parent;   /* 挂接由 lum__ui_attach 负责，创建本身不需要父指针 */
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

lum_Widget* lum_ui_dropdown(lum_Widget* parent, const char* const* options, int count, int selected) {
  if (!parent) return NULL;
  lum_Widget* w = lum__ui_create(parent, Lum_Widget_Dropdown);
  if (!w) return NULL;
  w->selected = -1;
  lum_ui_dropdown_set_options(w, options, count);   /* 内部按新数量钳制 selected */
  lum_ui_dropdown_set_selected(w, selected);
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

void lum_ui_set_align(lum_Widget* w, lum_Ui_Align align) {
  Lum_Assert(w);
  w->main_align = (uint8_t)align;   /* 每帧全量重排，改完下一帧自动生效 */
}

lum_Ui_Align lum_ui_get_align(const lum_Widget* w) {
  Lum_Assert(w);
  return (lum_Ui_Align)w->main_align;
}

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

void lum_ui_dropdown_set_options(lum_Widget* w, const char* const* options, int count) {
  Lum_Assert(w && w->type == Lum_Widget_Dropdown);
  for (uint32_t i = 0; i < w->option_count; ++i) free(w->options[i]);
  w->option_count = 0;
  if (count > 0) {
    if ((uint32_t)count > w->option_capacity) {
      uint32_t cap = w->option_capacity ? w->option_capacity : 4u;
      while (cap < (uint32_t)count) cap *= 2u;
      char** opts = (char**)realloc(w->options, (size_t)cap * sizeof(*opts));
      if (opts) { w->options = opts; w->option_capacity = cap; }
    }
    if ((uint32_t)count <= w->option_capacity) {
      for (int i = 0; i < count; ++i) {
        w->options[w->option_count] = lum__ui_strdup(options && options[i] ? options[i] : "");
        if (!w->options[w->option_count]) break;
        w->option_count++;
      }
    }
  }
  lum_ui_dropdown_set_selected(w, w->selected);   /* 按新数量重新钳制 */
}

void lum_ui_dropdown_set_selected(lum_Widget* w, int index) {
  Lum_Assert(w && w->type == Lum_Widget_Dropdown);
  if (index >= (int)w->option_count) index = (int)w->option_count - 1;
  if (index < -1) index = -1;
  w->selected = index;   /* 程序化设值不触发 on_value_changed（与 set_value 同约定） */
}

int lum_ui_dropdown_selected(const lum_Widget* w) {
  Lum_Assert(w && w->type == Lum_Widget_Dropdown);
  return w->selected;
}

int lum_ui_dropdown_count(const lum_Widget* w) {
  Lum_Assert(w && w->type == Lum_Widget_Dropdown);
  return (int)w->option_count;
}

const char* lum_ui_dropdown_option(const lum_Widget* w, int index) {
  Lum_Assert(w && w->type == Lum_Widget_Dropdown);
  if (index < 0 || index >= (int)w->option_count) return NULL;
  return w->options[index];
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
    /* 参考 GIMP 深色主题的灰黑色调：中性灰、无彩色偏移。
       层次关系：面板比按钮面亮，按钮边框=面×0.65，顶边高光=面向白提亮 25% */
    .panel_bg       = lum_vec4_create(0.24f, 0.24f, 0.24f, 0.92f),
    .panel_border   = lum_vec4_create(0.13f, 0.13f, 0.13f, 1.0f),
    .button_bg      = lum_vec4_create(0.17f, 0.17f, 0.17f, 1.0f),
    .button_hover   = lum_vec4_create(0.22f, 0.22f, 0.22f, 1.0f),
    .button_pressed = lum_vec4_create(0.11f, 0.11f, 0.11f, 1.0f),
    .button_disabled= lum_vec4_create(0.12f, 0.12f, 0.12f, 1.0f),
    .text           = lum_vec4_create(0.84f, 0.84f, 0.84f, 1.0f),
    .text_disabled  = lum_vec4_create(0.45f, 0.45f, 0.45f, 1.0f),
    .accent         = lum_vec4_create(0.36f, 0.46f, 0.56f, 1.0f),   /* 低饱和蓝灰：选中/滑块/焦点 */
    .slider_track   = lum_vec4_create(0.15f, 0.15f, 0.15f, 1.0f),
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
  for (uint32_t i = 0; i < w->option_count; ++i) free(w->options[i]);
  free(w->options);
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
      float slider_w = (ts.x > 120.0f) ? ts.x : 120.0f;
      return lum_vec2_create(slider_w, g_ui.text_height + 12.0f);
    }
    case Lum_Widget_Textbox: {
      lum_Vec2 ts = lum__ui_text_size(w->text);
      float textbox_w = (ts.x > 120.0f) ? ts.x : 120.0f;
      return lum_vec2_create(textbox_w + lum_default_theme.padding * 2.0f, g_ui.text_height + 12.0f);
    }
    case Lum_Widget_Progress:
      return lum_vec2_create(120.0f, g_ui.text_height + 4.0f);
    case Lum_Widget_Dropdown: {
      float max_w = 0.0f;
      for (uint32_t i = 0; i < w->option_count; ++i) {
        lum_Vec2 ts = lum__ui_text_size(w->options[i]);
        if (ts.x > max_w) max_w = ts.x;
      }
      float bw = max_w + lum_default_theme.padding * 2.0f + 20.0f;   /* 右侧留箭头区 */
      if (bw < 120.0f) bw = 120.0f;
      return lum_vec2_create(bw, g_ui.text_height + 12.0f);
    }
    case Lum_Widget_Scroll:
      /* Scroll 已在函数开头提前返回；此处仅补全枚举，避免 -Wswitch 告警 */
      break;
  }
  return lum_vec2_create(0.0f, 0.0f);
}

/* 排布单控件及其子树（内部坐标尚未翻转，翻转发生在绘制转换）。
   主轴双向：Start 子项自起点向下/向右排，End 子项停靠终点向上/向左，
   Fill 子项吃掉两组之间的剩余空隙。碰撞时 Start/Fill 组优先（End 游标钳制）；
   Scroll 内容单向生长，非 Start 对齐一律退化为 Start */
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

  bool horizontal = w->horizontal;
  bool is_box = (w->type == Lum_Widget_Box);
  float pad = lum_default_theme.padding;
  float spacing = lum_default_theme.spacing;
  float main_start = (horizontal ? assigned.position.x : assigned.position.y) + pad - scroll;
  float main_end   = (horizontal ? assigned.position.x + assigned.size.x
                                 : assigned.position.y + assigned.size.y) - pad;
  float cross = (horizontal ? assigned.position.y : assigned.position.x) + pad;
  float inner_cross = horizontal ? inner_h : assigned.size.x - pad * 2.0f;

  /* 第一遍：Start 组（含 Scroll 内退化子项与多余 Fill）按声明顺序推进起点游标；
     首个可见 Fill 只记下不占位——它的空隙要等 End 组预留量算出后统一结算 */
  lum_Widget* fill_child = NULL;
  float fwd = main_start;
  for (uint32_t i = 0; i < w->child_count; ++i) {
    lum_Widget* c = w->children[i];
    if (!c->visible) continue;
    lum_Ui_Align align = is_box ? (lum_Ui_Align)c->main_align : Lum_Ui_Align_Start;
    if (align == Lum_Ui_Align_Fill) {
      if (fill_child) align = Lum_Ui_Align_Start;   /* 只有首个可见 Fill 扩展，其余按 Start */
      else { fill_child = c; continue; }
    }
    if (align != Lum_Ui_Align_Start) continue;   /* End：本遍不占位，留给第二遍 */

    lum_Vec2 cs = lum__ui_measure(c);
    lum_Rect child_rect;
    if (horizontal) {
      child_rect = (lum_Rect){ {fwd, cross}, {cs.x, inner_cross} };
      if (c->fixed_size.y > 0) child_rect.size.y = c->fixed_size.y;
      fwd += cs.x + spacing;
    } else {
      child_rect = (lum_Rect){ {cross, fwd}, {inner_cross, cs.y} };
      if (c->fixed_size.x > 0) child_rect.size.x = c->fixed_size.x;
      fwd += cs.y + spacing;
    }
    lum__ui_arrange(c, child_rect);
  }

  /* Fill 结算：空隙 = 终点边 − End 组预留量 − Start 游标；不足则为 0（Start 组优先）。
     主轴尺寸取整个空隙（fixed_size 主轴分量被忽略），交叉轴规则不变。
     Fill 子项仍须 measure 一次：带 fixed_size 的容器会短路子项测量，
     而 Scroll 的 content_height 是 measure 的输出，漏测则滚动范围恒为 0 */
  if (fill_child) {
    lum__ui_measure(fill_child);
    float reserved = main_end;
    uint32_t end_count = 0;
    for (uint32_t i = 0; i < w->child_count; ++i) {
      lum_Widget* c = w->children[i];
      if (!c->visible || (lum_Ui_Align)c->main_align != Lum_Ui_Align_End) continue;
      lum_Vec2 cs = lum__ui_measure(c);
      reserved -= (horizontal ? cs.x : cs.y) + (end_count ? spacing : 0.0f);
      end_count++;
    }
    float gap = reserved - fwd;
    if (gap < 0.0f) gap = 0.0f;
    lum_Rect child_rect;
    if (horizontal) {
      child_rect = (lum_Rect){ {fwd, cross}, {gap, inner_cross} };
      if (fill_child->fixed_size.y > 0) child_rect.size.y = fill_child->fixed_size.y;
    } else {
      child_rect = (lum_Rect){ {cross, fwd}, {inner_cross, gap} };
      if (fill_child->fixed_size.x > 0) child_rect.size.x = fill_child->fixed_size.x;
    }
    lum__ui_arrange(fill_child, child_rect);
    fwd += gap;
  }

  /* 第二遍：End 组按声明逆序从终点回退放置——组内视觉顺序仍与声明顺序一致 */
  if (is_box) {
    float bwd = main_end;
    for (uint32_t i = w->child_count; i-- > 0; ) {
      lum_Widget* c = w->children[i];
      if (!c->visible || (lum_Ui_Align)c->main_align != Lum_Ui_Align_End) continue;
      lum_Vec2 cs = lum__ui_measure(c);
      lum_Rect child_rect;
      if (horizontal) {
        bwd -= cs.x;
        if (bwd < fwd) bwd = fwd;                /* 碰撞钳制：Start/Fill 组优先 */
        child_rect = (lum_Rect){ {bwd, cross}, {cs.x, inner_cross} };
        if (c->fixed_size.y > 0) child_rect.size.y = c->fixed_size.y;
        bwd -= spacing;
      } else {
        bwd -= cs.y;
        if (bwd < fwd) bwd = fwd;
        child_rect = (lum_Rect){ {cross, bwd}, {inner_cross, cs.y} };
        if (c->fixed_size.x > 0) child_rect.size.x = c->fixed_size.x;
        bwd -= spacing;
      }
      lum__ui_arrange(c, child_rect);
    }
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

/* ---------- Dropdown 展开覆盖层 ---------- */

#define Lum__Ui_Dropdown_Row_Pad 4.0f   /* 选项行高 = 文本行高 + 上下各 4px */

/* 展开列表矩形：贴控件下缘同宽，底部放不下且上方够用则向上翻开 */
static lum_Rect lum__ui_dropdown_popup_rect(const lum_Widget* w) {
  float row_h = g_ui.text_height + Lum__Ui_Dropdown_Row_Pad * 2.0f;
  float h = (float)w->option_count * row_h + lum_default_theme.padding * 2.0f;
  lum_Rect r = { {w->rect.position.x, w->rect.position.y + w->rect.size.y}, {w->rect.size.x, h} };
  if (r.position.y + h > g_ui.window_h && w->rect.position.y - h >= 0.0f)
    r.position.y = w->rect.position.y - h;
  return r;
}

/* 点所在选项索引；不在列表内容区内返回 -1 */
static int lum__ui_dropdown_option_at(const lum_Widget* w, lum_Rect popup, lum_Vec2 p) {
  float row_h = g_ui.text_height + Lum__Ui_Dropdown_Row_Pad * 2.0f;
  float y = p.y - popup.position.y - lum_default_theme.padding;
  if (y < 0.0f) return -1;
  int idx = (int)(y / row_h);
  return (idx < (int)w->option_count) ? idx : -1;
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

/* 按钮顶边框内侧 1px 高光线：颜色 = 按钮底色向白提亮 25%（各状态底色自动适配），
   左右缩进 1px 不压侧边框，模拟微弱立体效果。按钮内部细节，无外部接口 */
static void lum__ui_button_top_highlight(lum_Rect r, lum_Vec4 bg) {
  if (r.size.x < 3.0f || r.size.y < 3.0f) return;
  lum_Vec4 hi = lum_vec4_create(bg.x + (1.0f - bg.x) * 0.25f,
                                bg.y + (1.0f - bg.y) * 0.25f,
                                bg.z + (1.0f - bg.z) * 0.25f,
                                bg.w);
  lum__ui_draw_rect((lum_Rect){ {r.position.x + 1.0f, r.position.y + 1.0f},
                                {r.size.x - 2.0f, 1.0f} }, hi);
}

static void lum__ui_draw_text(lum_Vec2 top_left, const char* text, lum_Vec4 color) {
  if (!g_ui.font) return;
  /* 交给 lum_tr_draw_screen：它会在绘制期间换成 Y 向上的像素 VP。
     直接调 lum_tr_draw_layer 会让字形在 Screen 阶段的 Y 向下 VP 里上下颠倒
     —— 原先这里手写的 baseline 换算只挪了位置，并没有解决朝向。 */
  lum_tr_draw_screen(g_ui.font, text, top_left, Lum_Ui_Layer, color);
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
      /* 边框 = 底色压暗 35%（与顶边高光的提亮相对，随状态色自动适配） */
      lum_Vec4 border = lum_vec4_create(bg.x * 0.65f, bg.y * 0.65f, bg.z * 0.65f, bg.w);
      lum__ui_frame_rect(w->rect, bg, border);
      lum__ui_button_top_highlight(w->rect, bg);
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

    case Lum_Widget_Dropdown: {
      lum_Vec4 bg = w->enabled
          ? (w->hovered ? lum_default_theme.button_hover : lum_default_theme.button_bg)
          : lum_default_theme.button_disabled;
      lum__ui_frame_rect(w->rect, bg, lum_default_theme.panel_border);

      const char* label = "";
      if (w->selected >= 0) label = lum_ui_dropdown_option(w, w->selected);
      else if (w->text) label = w->text;   /* 未选中：text 字段作占位文本（菜单语义用） */
      lum_Vec4 tc = w->enabled ? lum_default_theme.text : lum_default_theme.text_disabled;
      lum_Vec2 pos = lum_vec2_create(w->rect.position.x + lum_default_theme.padding,
                                     w->rect.position.y + (w->rect.size.y - g_ui.text_height) * 0.5f);
      lum__ui_draw_text(pos, label ? label : "", tc);

      /* 箭头：右侧居中的三行递减像素三角 */
      float ax = w->rect.position.x + w->rect.size.x - lum_default_theme.padding - 10.0f;
      float ay = w->rect.position.y + w->rect.size.y * 0.5f - 3.0f;
      for (int i = 0; i < 3; ++i) {
        lum__ui_draw_rect((lum_Rect){ {ax + i * 2.0f, ay + i * 2.0f}, {10.0f - i * 4.0f, 2.0f} }, tc);
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

/* Popup 覆盖层绘制（坐标换算与树内 helper 相同，仅层不同） */
static void lum__ui_popup_draw_rect(lum_Rect r, lum_Vec4 color) {
  lum_renderer2d_draw_rect(lum_vec2_create(r.position.x, g_ui.window_h - r.position.y - r.size.y),
                           r.size, 0.0f, lum_vec2_0(), Lum_Ui_Popup_Layer, color);
}

static void lum__ui_popup_frame_rect(lum_Rect r, lum_Vec4 bg, lum_Vec4 border) {
  lum__ui_popup_draw_rect(r, bg);
  const float t = 1.0f;
  lum__ui_popup_draw_rect((lum_Rect){ {r.position.x, r.position.y}, {r.size.x, t} }, border);
  lum__ui_popup_draw_rect((lum_Rect){ {r.position.x, r.position.y + r.size.y - t}, {r.size.x, t} }, border);
  lum__ui_popup_draw_rect((lum_Rect){ {r.position.x, r.position.y}, {t, r.size.y} }, border);
  lum__ui_popup_draw_rect((lum_Rect){ {r.position.x + r.size.x - t, r.position.y}, {t, r.size.y} }, border);
}

static void lum__ui_popup_draw_text(lum_Vec2 top_left, const char* text, lum_Vec4 color) {
  if (!g_ui.font) return;
  float baseline = g_ui.window_h - top_left.y - lum_tr_ascent(g_ui.font);
  lum_tr_draw_layer(g_ui.font, text, lum_vec2_create(top_left.x, baseline), Lum_Ui_Popup_Layer, color);
}

/* Dropdown 展开列表：树外覆盖层，整树绘制完后调用（Lum_Ui_Popup_Layer 置顶、不受滚动剪裁影响） */
static void lum__ui_draw_dropdown_popup(lum_Widget* w) {
  lum_Rect popup = lum__ui_dropdown_popup_rect(w);
  lum__ui_popup_frame_rect(popup, lum_default_theme.panel_bg, lum_default_theme.panel_border);
  float row_h = g_ui.text_height + Lum__Ui_Dropdown_Row_Pad * 2.0f;
  for (int i = 0; i < (int)w->option_count; ++i) {
    lum_Rect row = { {popup.position.x + lum_default_theme.padding,
                      popup.position.y + lum_default_theme.padding + i * row_h},
                     {popup.size.x - lum_default_theme.padding * 2.0f, row_h} };
    if (i == w->selected)
      lum__ui_popup_draw_rect(row, lum_default_theme.accent);          /* 选中项常亮 */
    else if (w->enabled && lum_ui_point_in_rect(row, g_ui.mouse))
      lum__ui_popup_draw_rect(row, lum_default_theme.button_hover);    /* 悬停行高亮 */
    lum_Vec2 pos = lum_vec2_create(row.position.x,
                                   row.position.y + (row_h - g_ui.text_height) * 0.5f);
    lum__ui_popup_draw_text(pos, w->options[i], lum_default_theme.text);
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

  /* Dropdown 展开路由：列表是树外覆盖层，其矩形优先接管悬停（按下会捕获到本控件）；
     点击列表和控件框之外 → 立即收起并吞掉本次点击（不穿透到下层控件） */
  if (g_ui.dropdown && (!g_ui.dropdown->visible || !g_ui.dropdown->enabled)) g_ui.dropdown = NULL;
  if (g_ui.dropdown) {
    lum_Rect popup = lum__ui_dropdown_popup_rect(g_ui.dropdown);
    if (lum_ui_point_in_rect(popup, g_ui.mouse)) {
      g_ui.hover = g_ui.dropdown;
    } else if (!lum_ui_point_in_rect(g_ui.dropdown->rect, g_ui.mouse) && g_ui.mouse_pressed) {
      g_ui.dropdown = NULL;
      g_ui.hover = NULL;
    }
  }

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
    if (g_ui.capture->type == Lum_Widget_Dropdown && g_ui.capture->enabled) {
      lum_Widget* d = g_ui.capture;
      if (g_ui.dropdown == d) {
        /* 展开中释放：落在选项上 → 选中并收起；落在框上 → 收起；拖出 → 收起 */
        int idx = lum__ui_dropdown_option_at(d, lum__ui_dropdown_popup_rect(d), g_ui.mouse);
        if (idx >= 0 && idx != d->selected) {
          d->selected = idx;
          if (d->on_value_changed) d->on_value_changed(d, d->user);
        }
        g_ui.dropdown = NULL;
      } else if (over) {
        g_ui.dropdown = d;               /* 闭合框上释放：展开 */
      }
    }
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
  if (g_ui.dropdown && g_ui.dropdown->visible) lum__ui_draw_dropdown_popup(g_ui.dropdown);

  lum_renderer2d_set_view_projection(g_ui.saved_vp);
  g_ui.vp_saved = false;
}

#ifndef Lum_Ui_H
#define Lum_Ui_H

#include "lum_math2d.h"
#include "lum_text_renderer.h"

/*
* 保留模式界面：控件是持久对象，状态长在控件里，布局每帧自动计算，交互走事件回调。
*
* 生命周期：Lum_Stage_Start 构建控件树（一次），Lum_Stage_Screen 每帧调一次 lum_ui_frame()
*   （输入路由 → 回调 → 布局 → 绘制）。控件指针稳定，可长期持有并随时 set_*。
*
* 坐标系：像素、左上原点、Y 向下（与鼠标输入同向）。独立像素正交投影
*   （渲染器命令级 VP），相机移动缩放不影响 UI。绘制层级 Lum_Ui_Layer。
*
* 交互模型：
*   hover   —— 鼠标悬停的最上层可交互控件（逆序深度优先，先绘制者在上）
*   capture —— 按下后锁定，保持到释放（滑块拖动不脱手）
*   focus   —— v3（键盘焦点链）
*
* 布局：根容器为绝对定位容器（子项用 set_position 摆放）；
*   lum_ui_box 创建堆叠容器（纵向/横向），子项沿主轴排列、交叉轴填满。
*   子项可用 set_align 声明主轴对齐：Start 自起点向下/向右（默认）、
*   End 停靠终点向上/向左、Fill 吃掉两者间剩余空隙——同一容器可混用。
*   每帧全量重算（无脏标记）。
*
* 值与文本：checkbox/slider 的值存控件内（getter 读取），文本深拷贝（set_text 安全）；
*   dropdown 选项同样深拷贝，selected 存索引（-1 = 未选中）。
* 事件：on_click / on_value_changed 回调 + user 指针；程序化 set_value 不触发回调。
*/

#define Lum_Ui_Layer 1000
/* Dropdown 展开列表等覆盖层：渲染器同层内按材质分批（矩形批先于文字批），
   树内文字会盖住同层矩形——覆盖层必须整体抬一层才能同时压住树内矩形与文字 */
#define Lum_Ui_Popup_Layer (Lum_Ui_Layer + 1)

typedef struct lum_Widget lum_Widget;

typedef enum lum_Widget_type {
  Lum_Widget_Box = 0,      /* 布局容器（纵向/横向） */
  Lum_Widget_Scroll,       /* 纵向滚动容器：子项照纵向 Box 堆叠，超出视口剪裁 + 滚轮 */
  Lum_Widget_Label,
  Lum_Widget_Button,
  Lum_Widget_Checkbox,
  Lum_Widget_Slider,
  Lum_Widget_Progress,
  Lum_Widget_Textbox,
  Lum_Widget_Dropdown,     /* 下拉选择：闭合框显示选中项，点击展开覆盖层列表（树外绘制） */
} lum_Widget_type;

typedef void (*lum_Ui_click_fn)(lum_Widget* widget, void* user);
typedef void (*lum_Ui_value_fn)(lum_Widget* widget, void* user);

/* Box/Scroll 子项的主轴对齐（由父容器在排布时解释；Scroll 内非 Start 退化为 Start） */
typedef enum lum_Ui_Align {
  Lum_Ui_Align_Start = 0,   /* 自主轴起点向下/向右排（默认，与旧行为一致） */
  Lum_Ui_Align_End,         /* 停靠主轴终点（纵向=向上、横向=向左），组内视觉序=声明序 */
  Lum_Ui_Align_Fill,        /* 吃掉 Start 组与 End 组之间的剩余空隙（首个可见者生效） */
} lum_Ui_Align;

struct lum_Widget {
  lum_Widget_type type;
  bool visible;
  bool enabled;
  bool hovered;              /* 本帧悬停（帧内计算，只读） */
  bool pressed;              /* 鼠标按下中（capture == 本控件） */
  bool horizontal;           /* Box：横向堆叠 */
  bool checked;              /* Checkbox 值 */
  bool focused;              /* 键盘焦点（Tab 循环 / 点击获得；帧内计算，只读） */
  uint8_t main_align;        /* 本控件在父容器中的主轴对齐（lum_Ui_Align；默认 Start） */

  lum_Rect rect;             /* 布局输出（父坐标系像素） */
  lum_Vec2 position;         /* 绝对容器（根）子项的位置 */
  lum_Vec2 fixed_size;       /* 用户覆盖尺寸；0 分量 = 使用测量值 */

  lum_Widget* parent;
  lum_Widget** children;  /* 拥有的指针数组 */
  uint32_t child_count, child_capacity;

  char* text;                /* 深拷贝（label/button/slider 的标签） */
  lum_Vec4 color_override;   /* 文字颜色覆写；a < 0 = 使用主题 */

  float value, value_min, value_max;   /* Slider */

  float scroll_y;                      /* Scroll：内容滚动偏移（布局期钳制到内容范围） */
  float content_height;                /* Scroll：布局时测得的内容总高 */

  char** options;                      /* Dropdown：深拷贝的选项文本数组 */
  uint32_t option_count, option_capacity;
  int selected;                        /* Dropdown 当前选中索引；-1 = 未选中 */

  lum_Ui_click_fn on_click;
  lum_Ui_value_fn on_value_changed;
  void* user;
};

/* 主题（可运行时修改；v1 无持久化） */
typedef struct lum_Theme {
  lum_Vec4 panel_bg;
  lum_Vec4 panel_border;
  lum_Vec4 button_bg;
  lum_Vec4 button_hover;
  lum_Vec4 button_pressed;
  lum_Vec4 button_disabled;
  lum_Vec4 text;
  lum_Vec4 text_disabled;
  lum_Vec4 accent;           /* 选中/滑块填充 */
  lum_Vec4 slider_track;
  float    padding;          /* 容器内边距 */
  float    spacing;          /* 堆叠子项间距 */
} lum_Theme;

extern lum_Theme lum_default_theme;   /* 变量名与 typedef 区分 */

/* font 可为 NULL：不渲染文字、布局按 text_height 行高估算（纯逻辑测试用） */
int  lum_ui_init(lum_Font* font);
void lum_ui_shutdown(void);
void lum_ui_frame(void);     /* 每帧一次：输入路由 → 回调 → 布局 → 绘制 */

lum_Widget* lum_ui_root(void);

lum_Widget* lum_ui_box    (lum_Widget* parent, bool horizontal);
lum_Widget* lum_ui_scroll (lum_Widget* parent);   /* 纵向滚动容器：需 set_fixed_size 定视口高 */
lum_Widget* lum_ui_label  (lum_Widget* parent, const char* text);
lum_Widget* lum_ui_button (lum_Widget* parent, const char* label);
lum_Widget* lum_ui_checkbox(lum_Widget* parent, const char* label, bool checked);
lum_Widget* lum_ui_slider (lum_Widget* parent, const char* label,
                              float value, float min, float max);
lum_Widget* lum_ui_progress(lum_Widget* parent, float value);   /* value ∈ 0..1 */
lum_Widget* lum_ui_textbox(lum_Widget* parent, const char* initial_text);
lum_Widget* lum_ui_dropdown(lum_Widget* parent, const char* const* options, int count, int selected);

/* 键盘焦点：Tab 循环可聚焦控件；textbox 聚焦时接收字符流，slider 响应左右方向键 */
lum_Widget* lum_ui_focused(void);
void lum_ui_focus_next(void);

/* 输入吞噬：世界交互（拾取/相机/热键）应在 UI 占用输入期间跳过。
   hovered —— 鼠标下的最上层控件（根表面不算，空白处 NULL；帧内读取的是上一帧路由结果）
   input_captured —— 悬停任意控件 / 拖动捕获中 / 存在键盘焦点（点按空白处释放） */
lum_Widget* lum_ui_hovered(void);
bool lum_ui_input_captured(void);

void lum_ui_set_text(lum_Widget* w, const char* text);
void lum_ui_set_visible(lum_Widget* w, bool visible);
void lum_ui_set_enabled(lum_Widget* w, bool enabled);
void lum_ui_set_position(lum_Widget* w, lum_Vec2 position);   /* 绝对容器（根）子项用 */
void lum_ui_set_fixed_size(lum_Widget* w, lum_Vec2 size);
void lum_ui_set_align(lum_Widget* w, lum_Ui_Align align);     /* 父容器排布本控件时的主轴对齐 */
lum_Ui_Align lum_ui_get_align(const lum_Widget* w);
void lum_ui_set_checked(lum_Widget* w, bool checked);
void lum_ui_set_value(lum_Widget* w, float value);            /* 不触发 on_value_changed */

const char* lum_ui_get_text(const lum_Widget* w);
bool  lum_ui_get_checked(const lum_Widget* w);
float lum_ui_get_value(const lum_Widget* w);

/* Dropdown：set 为程序化修改（不触发 on_value_changed）；selected ∈ -1..count-1，
   越界自动钳制；选项文本深拷贝（调用方缓冲可立即释放） */
void lum_ui_dropdown_set_options(lum_Widget* w, const char* const* options, int count);
void lum_ui_dropdown_set_selected(lum_Widget* w, int index);
int  lum_ui_dropdown_selected(const lum_Widget* w);
int  lum_ui_dropdown_count(const lum_Widget* w);
const char* lum_ui_dropdown_option(const lum_Widget* w, int index);   /* 越界返回 NULL */

/* 滚动容器：set 为程序化滚动（布局期钳制到 [0, 内容−视口]）；content_height 布局后有效 */
void  lum_ui_set_scroll(lum_Widget* w, float y);
float lum_ui_get_scroll(const lum_Widget* w);
float lum_ui_content_height(const lum_Widget* w);

void lum_ui_on_click(lum_Widget* w, lum_Ui_click_fn cb, void* user);
void lum_ui_on_value_changed(lum_Widget* w, lum_Ui_value_fn cb, void* user);

float lum_ui_text_height(void);
lum_Widget* lum_ui_widget_at(lum_Vec2 point);                 /* 命中测试（测试/工具用） */
bool lum_ui_point_in_rect(lum_Rect rect, lum_Vec2 point);

#endif //Lum_Ui_H

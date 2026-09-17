# 子系统

| 模块 | 头文件 | 职责 |
|---|---|---|
| `lum_text_renderer` | `lum_text_renderer.h` | 位图字体渲染（**仅 ASCII**） |
| `lum_ui` | `lum_ui.h` | 保留模式界面（控件树 + 自动布局） |

两者自成体系：`lum_ui` 依赖 `lum_text_renderer` 画文字，其余只依赖渲染层。

---

## `lum_text_renderer`

**职责**：用 stb_truetype 做位图字体（图集 + 懒加载字形），并绘制文字。

**关键契约**

| 函数 | 说明 |
|---|---|
| `lum_tr_load_font(font, path, height_pixel)` | 加载字体文件。**1 成功 / 0 失败** |
| `lum_tr_destroy_font(font)` | 释放（含内部图集与 GL 纹理） |
| `lum_tr_preload_ascii(font)` | 预光栅化 `c ∈ [32, 127)`，消除首帧的批量上传 |
| `lum_tr_draw_layer(font, text, position, layer, color)` | 在世界空间绘制，**`position` 是文本左下角** |
| `lum_tr_draw(font, text, position, color)` | 同上，`layer = 0` |
| `lum_tr_measure_text(font, text)` | 返回 `(宽, 高)`；**多行取最大宽 × 行数**；**会触发懒加载** |
| `lum_tr_ascent(font)` | 基线到行顶的上升高度（像素），UI 顶边对齐用 |

- 数据结构：`lum_Font{initialized, void* renderer}`（**不透明内部**）
- 图集：**单张 512×512 R8**（`Lum_Text_Atlas_Size`），padding 1；
  用 `Swizzle_R_To_RGBA` 让单通道直接进 sprite 染色路径
- `lum_Font` 里持有一个 `malloc` 的 renderer + 字体文件缓冲，
  **必须 `lum_tr_destroy_font`**（或交给 `lum_assets` 统一管理）

**⚠ 关键约束：只支持 ASCII**

- 字形表是 `lum_Glyph glyphs[128]`，单页图集，`preload_ascii` 也只预载 `[32,127)`
- 代码中对每个字符只有一句 `Lum_Assert(c < 128)`
  （`src/lum_text_renderer.c:193` 与 `:257`），**后面紧跟 `&tr->glyphs[c]`**

**【违反后果】** 传入非 ASCII 字节（例如任何中文字符串）：

- **debug（定义了 `Lum_Debug`）**：断言失败 → abort
- **release（未定义 `Lum_Debug`）**：断言消失，`c` 可达 255，
  **越界读 `glyphs[128]`** → 未定义行为（[CONTRACT.md](../CONTRACT.md) §7）

也就是说，**这里的安全完全依赖 `Lum_Debug`**。冻结前至少要把断言换成
运行期 `continue`（跳过非 ASCII），否则这是一个可被输入触发的内存错误。

**易错点**

- 传中文 / 任意 UTF-8 → 见上（最严重的一条）
- `lum_tr_measure_text` 会**改变内部状态**（懒加载字形），不是纯查询
- 文本位置是**左下角**（基线相关），不是左上角 —— 与 UI 的左上原点约定不同
- 忘了 `lum_tr_destroy_font` → 泄漏字体缓冲与图集纹理
- HUD/调试文本里写中文会直接崩（本项目早期就踩过这个）

---

## `lum_ui`

**职责**：**保留模式**界面 —— 控件是持久对象，状态长在控件里，
布局每帧自动计算，交互走事件回调。头文件里有较完整的说明块。

**生命周期**

- `Lum_Stage_Start`：`lum_ui_init(font)` + 构建控件树（**一次**）
- `Lum_Stage_Screen`：**每帧**调一次 `lum_ui_frame()`
- `lum_ui_shutdown()`
- `font` **可为 `NULL`**：不渲染文字、布局按 `text_height` 行高估算（纯逻辑测试用）

**坐标系**：像素、**左上原点、Y 向下**（与鼠标输入同向）；
独立像素正交投影（渲染器命令级 VP），**相机移动缩放不影响 UI**；
绘制层级 `Lum_Ui_Layer`(1000)，Dropdown 覆盖层用 `Lum_Ui_Popup_Layer`(1001)

**控件与模型**

| 类别 | 函数 |
|---|---|
| 容器 | `lum_ui_box(parent, horizontal)`、`lum_ui_scroll(parent)` |
| 叶子 | `lum_ui_label` / `button` / `checkbox` / `slider` / `progress` / `textbox` / `dropdown` |

- 交互三态：**hover**（悬停最上层）、**capture**（按下锁定到释放）、**focus**（键盘焦点链）
- 布局：根容器绝对定位（子项用 `lum_ui_set_position`）；
  `box` 是堆叠容器，子项主轴用 `lum_ui_set_align` 声明
  （`Start` 默认 / `End` 停靠终点 / `Fill` 吃掉空隙）；**每帧全量重算，无脏标记**
- 值与文本：checkbox/slider 的值存控件内（`lum_ui_get_*` 读）；
  **文本深拷贝**（`set_text` 安全，调用方缓冲可立即释放）；dropdown 选项同样深拷贝，
  `selected` 存索引（`-1` = 未选中）
- 事件：`lum_ui_on_click` / `lum_ui_on_value_changed` + `user` 指针；
  **程序化 `set_value` 不触发回调**

**输入吞噬（世界交互必须先查）**

```c
if (!lum_ui_input_captured()) {
  /* 才处理拾取 / 相机 / 热键 */
}
```

- `lum_ui_hovered()`：鼠标下的最上层控件（空白处 `NULL`）
- `lum_ui_input_captured()`：悬停任意控件 / 拖动捕获中 / 存在键盘焦点
- 二者读的是**上一帧**的路由结果

**主题**

- `extern lum_Theme lum_default_theme;` —— **可写的全局变量**
  （[CONTRACT.md](../CONTRACT.md) §1 的单例之一）。运行时改它即全局生效

**易错点**

- **世界交互没先查 `lum_ui_input_captured()`** → 点 UI 会穿透到世界（拾取、相机拖动）
- `lum_ui_hovered()` / `lum_ui_input_captured()` 是**上一帧**的结果，别当成实时
- `lum_ui_set_value` **不触发** `on_value_changed`（这是刻意的，但容易忘）
- `lum_Widget` 是**全字段公开**的结构体（约 30 个字段）—— 它的布局属于 ABI
- `Scroll` 需要先 `lum_ui_set_fixed_size` 定视口高，否则退化为"不滚动"

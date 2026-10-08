# 平台层（glimmer 的一部分）

与操作系统、窗口、输入、时钟打交道的一层。**这些模块属于 glimmer 基础平台**
（`glimmer/lum_*.h`），不在引擎高层 —— 它们加上数学层与渲染器，构成 glimmer
"能脱离引擎独立运行"的底气（见 [`glimmer.md`](glimmer.md)）。

| 模块 | 头文件 | 职责 |
|---|---|---|
| `lum_filesystem` | `glimmer/lum_filesystem.h` | 整文件读写、存在性、大小与修改时间 |
| `lum_window` | `glimmer/lum_window.h` | GLFW 窗口 + OpenGL 上下文（单例） |
| `lum_input_codes` | `glimmer/lum_input_codes.h` | 键码 / 鼠标键枚举（**纯头**） |
| `lum_input` | `glimmer/lum_input.h` | 输入状态查询（单例） |
| `lum_time` | `glimmer/lum_time.h` | 帧计时、FPS、帧率上限（单例） |

---

## `lum_filesystem`

**职责**：整文件读写（不做流式、不做目录遍历）。

**关键契约**

| 函数 | 返回值 | 所有权 |
|---|---|---|
| `lum_fs_read` | `char*` | **malloc，调用方 `free`** ⚠ 头文件未写 |
| `lum_fs_read_binary` | `lum_Binary_file{data,size}` | `data` 是 **malloc**，调用方 `free` ⚠ 未写 |
| `lum_fs_write` / `lum_fs_write_binary` | `1`/`0` | — |

- `lum_fs_read` 会**多分配 1 字节并写 NUL**，可以直接当 C 字符串用；
  `lum_fs_read_binary` **不**加终止符
- 失败路径统一 `Lum_Log_Error` 并返回 `NULL` / `(lum_Binary_file){NULL,0}` / `0`
- `lum_fs_exists` / `is_file` / `is_directory`、`lum_fs_file_size` / `file_mtime`

**易错点**

- **所有权**（[CONTRACT.md](../CONTRACT.md) §4）：这两个函数是引擎里少见的
  "返回需要你自己 `free` 的内存"。忘了就泄漏
- 把 `lum_fs_read_binary` 的结果当字符串用 → 没有终止符，会越界

---

## `lum_window`

**职责**：GLFW 窗口与 OpenGL 上下文（单例）。名字叫"窗口"，
但它同时是**整个引擎的 GL 上下文持有者**。

**关键契约**

- `lum_window_init(title, w, h)` 请求 **OpenGL 3.3 core profile**
  （`GLFW_CONTEXT_VERSION_MAJOR/MINOR = 3` + `GLFW_OPENGL_CORE_PROFILE`），
  然后加载 glad
- `lum_window_set_vsync(bool)`、`lum_window_is_open()`、`lum_window_set_should_close(bool)`
- `lum_window_poll_events()` / `lum_window_swap_buffers()`
- `lum_window_get_native_handle()` → `void*`（实为 `GLFWwindow*`，**借用**）
- `lum_window_get_time()` → `double`，GLFW 单调时钟（`lum_time` 的时钟来源）
- `lum_window_sleep(float sec)`、`lum_window_set_title`

**易错点**

- **窗口尺寸 ≠ framebuffer 尺寸**。`lum_window_get_width/height` 是窗口逻辑尺寸；
  `lum_window_get_framebuffer_width/height` 是像素尺寸。高 DPI 下两者不同 ——
  **渲染与视口必须用 framebuffer 尺寸**
- 它持有 GL 上下文，所以关闭顺序里 `lum_assets` / `lum_renderer2d` **必须先于它**拆
  （见 [architecture.md](../architecture.md) 第 5 节）
- 尚无跨平台验证，仅 Windows 实测

---

## `lum_input_codes`

**职责**：键码与鼠标键枚举。纯头，无 `.c`。

**关键契约**

- 数值**逐条等于 GLFW 常量**：`Lum_Key_Space = 32`、`Lum_Key_Escape = 256`、
  `Lum_Key_Enter = 257`、`Lum_Key_Left_Shift = 340` …
- `Lum_Key_Unknown = -1`；`Lum_Key_Last = Lum_Key_Menu`(348)
- `Lum_Mouse_Button_Left/Right/Middle` 是 `Button_1/2/3` 的别名；
  `Lum_Mouse_Button_Last = 8`

**易错点**

- 这是**有意的后端耦合**（[CONTRACT.md](../CONTRACT.md) §10）：把数值当"引擎自有编码"
  存档或跨进程传递，换掉 GLFW 后端即失效。冻结前需正式表态

---

## `lum_input`

**职责**：输入状态查询（单例）。状态由 GLFW 回调与逐帧轮询共同维护。

**关键契约**

- `lum_input_update()` **每帧调一次**：把"当前帧"滚入"上一帧"，再重新采样
- `lum_input_is_key_down` = 持续按住；`is_key_pressed` / `is_key_released` = **边沿**
  （鼠标键同理）
- `lum_input_get_mouse_position()` / `get_mouse_position_delta()`（像素）
- `lum_input_get_scroll()`：本帧滚轮累计
- `lum_input_text()`：本帧字符流，NUL 结尾、**帧内恒定**、重复调用返回**同一缓冲**
  （头文件已写明）
- 字符流**只收 ASCII 可打印字符**：`src/lum_input.c` 过滤 `codepoint >= 32 && < 127`，
  且受 `Lum_Input_Text_Max`(32) 截断

**易错点**

- 忘了每帧 `lum_input_update` → **边沿检测永远不触发**（pressed/released 恒为 false）
- 键码/鼠标键超范围会触发 `Lum_Assert`（debug 下直接 abort）
- **中文输入拿不到** —— 字符回调只保留 ASCII（与文本渲染器同为 ASCII-only，见下方子系统）

---

## `lum_time`

**职责**：帧计时与帧率控制（单例）。

**关键契约**

- `lum_time_begin_frame()` / `lum_time_end_frame()` 成对使用；`get_delta()` = 本帧秒数
- `lum_time_get_total()` 自引擎启动累计；`get_frame_count()`；`get_fps()`
- `lum_time_set_max_fps(n)`：`n > 0` 时限帧
- `lum_time_set_max_delta(s)`：钳制单帧 dt，避免调试断点后数值跳变
- `lum_time_set_fps_update_interval(s)`：FPS 的平滑窗口
- 时钟来源是 `lum_window_get_time()`

**易错点**

- 未 `begin_frame` 就读 `get_delta()` → 值无意义
- `Lum_Sleep_Margin`(0.030f) 是**公开、可被外部 `#define` 覆盖**的编译期常量
  （头文件用 `#ifndef` 包着）。改它会改变睡眠补偿行为 —— 属于版本契约的一部分，
  冻结时要么收进正式契约，要么去掉这个口子

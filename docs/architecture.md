# Luminous 架构总览

## 这份文档的范围

- **面向**：要读引擎源码、写扩展组件、或判断"某个功能该放哪"的人
- **不含**：逐符号 API 参考 —— 那属于头文件注释，二期由 Doxygen 呈现
- **配套**：`CONTRACT.md`（跨模块语义契约）、`versioning.md`（编号策略）

## 一句话

Luminous 是一个**分层单向依赖**的 2D 渲染框架：所有模块只准依赖它下面的层，
引擎的全部编排状态收敛到一个**阶段状态机**（`lum_application`），
消费者通过 35 个公共头接入，`src/` 只放实现、不对外可见。

---

## 1. 分层

层次顺序不是新造的，而是直接取自 `include/luminous.h` 的注释分组。

分层自 2026-09 第二次重组后是**两段式**：glimmer 是引擎脚下的基础平台，
Luminous 的高层模块全部建在它上面。

| 层 | 职责 | 模块 | 依赖方向 |
|---|---|---|---|
| **glimmer ① 数学** | 无副作用的数学基底 | `glimmer/lum_math2d` `glimmer/lum_transform` | 只准层内向下（`math2d` 的引擎头闭包为**空**） |
| **glimmer ② 平台** | 与操作系统/窗口/磁盘打交道 | `glimmer/lum_log` `glimmer/lum_assert` `glimmer/lum_filesystem` `glimmer/lum_window` `glimmer/lum_input_codes` `glimmer/lum_input` `glimmer/lum_time` | → ① |
| **glimmer ③ 渲染器** | GPU 资源与 2D 绘制 | `glimmer/lum_shader` `glimmer/lum_texture` `glimmer/lum_atlas` `glimmer/lum_camera` `glimmer/lum_material` `glimmer/lum_render_target` `glimmer/lum_renderer2d` `glimmer/lum_text_renderer` | → ①② |
| **Luminous ④ 基础** | 引擎自己的地基（碰撞、容器、版本） | `lum_collision` `lum_vector` `lum_list` `lum_version` | → glimmer |
| **Luminous ⑤ 资源** | 磁盘资源的缓存与复用 | `lum_assets` | → ④ + glimmer |
| **Luminous ⑥ 序列化** | 与格式无关的读写抽象 | `lum_serial` | → glimmer |
| **Luminous ⑦ 场景** | 实体/组件/视图与存档 | `lum_ecs` `lum_scene` `lum_scene_io` | → ④⑤⑥ + glimmer |
| **Luminous ⑧ 扩展组件** | 挂在实体上的现成能力 | `lum_components` `lum_component_glow` `lum_component_beam` | → ⑥⑦ + glimmer |
| **Luminous ⑨ 特效** | 光晕/光束的绘制函数 | `lum_effects` `lum_glow` `lum_beam` | → glimmer |
| **Luminous ⑩ UI** | 控件与主题 | `lum_ui` | → glimmer |
| **Luminous ⑪ 编排** | 引擎唯一的状态机（17 阶段管线） | `lum_application` | → 除自身外全部 |

**glimmer 不许反向依赖 ④～⑪ 中的任何模块** —— 它自带窗口、输入、时间、文件系统、
数学与渲染器，因此可以脱离引擎单独运行（见下）。

### 构建目标（三个，依赖是一条单向链）

```
glad ──> glimmer ──> Luminous
         （基础平台）  （引擎高层）
```

拆开的目的：
1. **边界可机械校验** —— `tools/check-glimmer-boundary.ps1` 查包含关系与链接行，
   构建目标查链接依赖（三条规则见 [`modules/glimmer.md`](modules/glimmer.md)）；
2. **平台可独立运行** —— `glimmer_hello`（`glimmer_hello.c`）只链接 `libglimmer.a`，
   自己开窗口、自己跑帧循环、自己画，是"glimmer 不依赖引擎"的机器判据；
3. **将来换 3D 渲染器只动 `glimmer` 这一个目标**。

### 真正的叶子（不含任何引擎头）

消费者可以只包含它们而不拉进任何其它模块：

```
glimmer/glimmer_version.h   glimmer/lum_math2d.h   glimmer/lum_log.h
glimmer/lum_filesystem.h   glimmer/lum_window.h   glimmer/lum_input_codes.h
glimmer/lum_time.h   glimmer/lum_atlas.h   lum_ecs.h
containers/lum_list.h  containers/lum_vector.h
```

（`lum_version.h` **不在**这个列表里：它包含 `glimmer/glimmer_version.h` —— 引擎要声明
"兼容哪个 glimmer 版本"，这条依赖是有意的，见下方第二条层内依赖。）

**层内依赖只有两条**：`lum_assert.h → lum_log.h`（`Lum_Assert` 失败要通过日志报错），
以及 `lum_version.h → glimmer/glimmer_version.h`（引擎要声明它兼容哪个 glimmer 版本）。
两条依赖都指向同层或下层，方向没有反转。

---

## 2. 公共头依赖图（消费者视角）

`A → B` 表示包含 `A.h` 会连带包含 `B.h`。

```
glimmer 数学 / 平台（路径带 glimmer/ 前缀）
  glimmer/lum_assert        → glimmer/lum_log
  glimmer/glimmer_version   → （无依赖：纯宏，CMake 也按文本解析它）
  lum_version（引擎）        → glimmer/glimmer_version（仅此一条跨线依赖：声明兼容的 glimmer 版本）
  glimmer/lum_input         → glimmer/lum_input_codes, glimmer/lum_math2d
  （math2d / transform / log / filesystem / window / time / input_codes：无引擎依赖）

glimmer 渲染器（`glimmer/glimmer.h` 是平台统一入口）
  glimmer/lum_shader        → glimmer/lum_math2d
  glimmer/lum_texture       → glimmer/lum_math2d
  glimmer/lum_camera        → glimmer/lum_math2d
  glimmer/lum_atlas         → （无引擎依赖）
  glimmer/lum_material      → glimmer/lum_math2d, glimmer/lum_shader, glimmer/lum_texture
  glimmer/lum_render_target → glimmer/lum_math2d, glimmer/lum_texture
  glimmer/lum_renderer2d    → glimmer/lum_math2d, glimmer/lum_transform, glimmer/lum_camera,
                              glimmer/lum_texture, glimmer/lum_material

Luminous 高层（路径**不带** glimmer/ 前缀）
  lum_assets        → glimmer/lum_texture, glimmer/lum_text_renderer
  lum_serial        → glimmer/lum_math2d, lum_ecs, containers/lum_vector

Luminous 场景
  lum_scene         → glimmer/lum_math2d, lum_ecs, glimmer/lum_camera, glimmer/lum_texture,
                      glimmer/lum_render_target
  lum_scene_io      → lum_ecs

Luminous 扩展组件 / 特效
  lum_components    → components/lum_component_glow, components/lum_component_beam
  lum_component_glow→ lum_scene
  lum_component_beam→ lum_scene
  lum_effects       → effects/lum_glow, effects/lum_beam
  lum_glow          → glimmer/lum_math2d
  lum_beam          → glimmer/lum_math2d

Luminous UI / 编排
  lum_ui            → glimmer/lum_math2d, glimmer/lum_text_renderer
  lum_application   → glimmer/lum_math2d, glimmer/lum_log, glimmer/lum_camera, lum_ecs
```

**方向约束（可机械校验）**：
1. 所有 `glimmer/*` 只能出现在**右边**（被 Luminous 依赖），反向一次都不许；
2. glimmer 源码/公共头的包含只能是 `glimmer/*`、third_party（glad/stb/GLFW，且仅实现可见）与标准库；
3. `glimmer_hello` 的链接行里不许出现 `Luminous`。
三条都由 `tools/check-glimmer-boundary.ps1` 在每次门禁里强制。

`include/luminous.h` 现在是两段：一段 `glimmer/glimmer.h`（平台全量），
一段引擎高层（`lum_version` / `lum_collision` / `containers` / `lum_assets` / `lum_serial` /
`lum_ecs` / `lum_scene` / `lum_scene_io` / `components` / `effects` / `lum_ui`）；`lum_beam`、
`lum_glow`、`lum_component_beam`、`lum_component_glow` 仍由 `lum_effects.h` /
`lum_components.h` 间接拉入。

---

## 3. 两个环，以及它们是怎么被打断的

架构上存在一个天然的双向关系：

- `lum_scene.c` 需要 `lum_Context`（系统回调签名）
- `lum_application.h` 需要 `lum_View`（上下文里透传当前视图）
- 而 **`lum_View` 定义在 `lum_scene.h`**

打断方式（**公开头依赖图因此无环**）：

| 位置 | 做法 |
|---|---|
| `include/lum_application.h` | 只做 `struct lum_View;` 前置声明，不包含 `lum_scene.h` |
| `include/lum_scene.h` | 只做 `struct lum_Context;` 前置声明 + 同名 typedef，不包含 `lum_application.h` |
| `src/lum_scene.c` | 实现里才真正 `#include "lum_application.h"` |

**后果（对使用者有意义）**：只包含 `lum_scene.h` 不会把整个 application 层拉进来；
只用 ECS + 场景的代码不必依赖状态机。环只存在于实现层，不污染公开面。

---

## 4. 名义依赖 vs 实际依赖（三处噪音）

下面三行 `#include` 存在，但对应符号**一次都没用到**：

| 文件 | 多余包含 | 证据 |
|---|---|---|
| `src/lum_math2d.c` | `lum_log.h` | 使用 0 次；第 412 行留有 `//TODO: 接入lum_log` |
| `src/lum_window.c` | `lum_math2d.h` | 使用 0 次（无任何 `lum_Vec` / `lum_Mat` / `lum_Rect`） |
| `src/lum_time.c` | `lum_math2d.h` | 使用 0 次 |

所以**代码里的实际耦合比依赖图看起来更松**。清掉这三行可以消除虚假依赖，
也让"基础层/平台层互不牵连"这个卖点站得住。（`lum_time → lum_window` 是**真实**依赖：
时间模块用 `lum_window_get_time()` 取时钟。）

---

## 5. 启动与关闭顺序

引擎把顺序写死在 `src/lum_application.c` 的 `lum__stage_init()` / `lum__stage_shutdown()`，
不注册、不可配。`g_init_level` 记录进度，关闭时**只拆已经起来的**，
因此任意一步失败都能安全回滚。

| 级别 | 启动动作 | 为什么在这个位置 |
|---|---|---|
| 1 | `lum_log_init`（glimmer） | 必须最先 —— 后面任何一步失败都要能记录 |
| 2 | `lum_window_init` + vsync（glimmer） | `input` 和 `time` 都要 GLFW 窗口 |
| 3 | `lum_input_init`（glimmer） | 需要窗口句柄装回调 |
| 4 | `lum_time_init`（glimmer） | 依赖窗口时钟；`max_fps` 在这里下发 |
| 5 | `lum_renderer2d_init`（glimmer） | 需要已创建的 GL 上下文；同时设清屏色与默认混合模式（Alpha） |
| 6 | `lum_effects_init`（Luminous） | 编译特效着色器，要渲染器就绪 |
| 7 | `lum_ecs_create` + `lum_scene_init`（Luminous） | 世界与内置组件注册 |
| 8 | `lum_assets_init`（Luminous） | 资源缓存；它持有 GL 对象，所以**最后起、最先拆** |

前 5 级（1~5）全部属于 glimmer —— 也就是说 `glimmer_hello.c` 手写的那段启动顺序与
引擎的 Init 前 5 级**逐条一致**；Luminous 只是在这之上再加资源、世界与阶段管线。

关闭严格逆序（8 → 1），理由最典型的是 `assets`：它持有的纹理是 GL 对象，
必须早于 `renderer2d` 拆除。

---

## 6. 帧流程

引擎每帧只推进一个状态机，阶段顺序写死在 `lum_flow_run_frame()`：

```
Init
Start
┌─ 每帧 ───────────────────────────────────────────────────────┐
│  Frame_Begin                                                 │
│  Input → Pre → Transform → Post → Late → Commit   （每帧 1 步）│
│  ┌─ 每个视图 ───────────────────────────────┐                 │
│  │  View_Begin → Scene → Scene_Overlay      │                 │
│  └──────────────────────────────────────────┘                 │
│  Post_Process → Screen → Frame_End                            │
└──────────────────────────────────────────────────────────────┘
Stop
Shutdown（逆序拆除）
```

其中 `Init / Frame_Begin / Transform / Commit / View_Begin / Frame_End / Shutdown`
是**引擎独占**阶段，用户注册会被拒绝并告警。

数据契约（单向）：**Pre 及之前只能写 `local`；`Transform` 由引擎把 `local → world`；
Post 及之后读 `world` 才是本帧最新的**。详见 `include/lum_application.h` 的注释块。

未注册任何视图时退化为"沿用调用方已设好的 VP"，`Scene`/`Scene_Overlay` 每帧各跑一次，
保持单相机行为。

---

## 7. 规模

（行数按**物理行、含空行**；"声明"按声明行统计，含少量宏与误匹配，
全库唯一公开函数名为 443 个）

| 层 | 模块 | .c | .h | 声明 |
|---|---|---:|---:|---:|
| **glimmer 基础平台** | （独立构建目标，可单独运行） | **3969** | **1272** | |
| 数学 | `glimmer/lum_math2d` | 583 | 148 | 78 |
| 数学 | `glimmer/lum_transform` | 69 | 26 | 11 |
| 平台 | `glimmer/lum_window` | 167 | 31 | 16 |
| 平台 | `glimmer/lum_input` | 159 | 33 | 14 |
| 平台 | `glimmer/lum_time` | 152 | 33 | 16 |
| 平台 | `glimmer/lum_log` | 131 | 39 | 7 |
| 平台 | `glimmer/lum_filesystem` | 118 | 25 | 9 |
| 平台 | `glimmer/lum_assert` | — | 28 | 1（宏） |
| 平台 | `glimmer/lum_input_codes` | — | 141 | 枚举 |
| 渲染 | `glimmer/lum_renderer2d` | 838 | 89 | 29 |
| 渲染 | `glimmer/lum_text_renderer` | 305 | 47 | 7 |
| 渲染 | `glimmer/lum_material` | 292 | 114 | 28 |
| 渲染 | `glimmer/lum_texture` | 195 | 75 | 8 |
| 渲染 | `glimmer/lum_shader` | 169 | 30 | 14 |
| 渲染 | `glimmer/lum_render_target` | 127 | 28 | 6 |
| 渲染 | `glimmer/lum_camera` | 172 | 85 | 20 |
| 渲染 | `glimmer/lum_atlas` | 54 | 26 | 4 |
| **3D（M1~M3）** | `glimmer/lum_renderer3d` | 274 | 102 | 17 |
| 3D（M1~M3） | `glimmer/lum_mesh` | 164 | 58 | 9 |

| 渲染 | `glimmer/glimmer.h`（伞头） | — | 60 | — |
| 版本 | `glimmer/glimmer_version.h` | — | 54 | 0（宏） |
| **Luminous 引擎高层** | （依赖 glimmer） | **4822** | **1302** | |
| 基础 | `lum_collision` | 735 | 133 | 28 |
| 基础 | `lum_vector` | 145 | 39 | 15 |
| 基础 | `lum_version` | — | 78 | 0（宏） |
| 基础 | `lum_list` | — | 84 | 内联 |
| 资源 | `lum_assets` | 166 | 18 | 6 |
| 序列化 | `lum_serial` | 414 | 149 | 29 |
| 场景 | `lum_ecs` | 492 | 77 | 28 |
| 场景 | `lum_scene` | 477 | 169 | 23 |
| 场景 | `lum_scene_io` | 338 | 11 | 3 |
| 扩展组件 | `lum_component_glow` | 79 | 36 | 4 |
| 扩展组件 | `lum_component_beam` | 66 | 32 | 4 |
| 扩展组件 | `lum_components` | 8 | 14 | 1 |
| 特效 | `lum_glow` | 173 | 64 | 7 |
| 特效 | `lum_beam` | 120 | 45 | 5 |
| 特效 | `lum_effects` | 39 | 11 | 3 |
| UI | `lum_ui` | 1104 | 177 | 42 |
| 编排 | `lum_application` | 466 | 137 | 17 |
| 编排 | `luminous.h`（伞头） | — | 30 | — |
| | **合计** | **8791**（32 个 .c） | **2574**（39 个头） | |
| 第三方 | `glad.c` | 2532 | — | — |

**平台占 45.1% 的实现行数**（glimmer 3969 / 8791）：基础平台不是一个薄壳，
它自带窗口、输入、时间、文件系统、数学与整套渲染器 —— 这正是它能"独立运行"的原因。
高层里最大的三块是 `lum_ui`（1104）、`lum_collision`（735）、`lum_ecs`（492）。
`lum_scene_io` 值得单独注意：**338 行实现，但头文件只有 11 行、3 个函数** ——
它的全部契约（存档格式、版本、兼容性）目前都对消费者不可见。

> **计数方式提醒**：不要用 `Get-Content | Measure-Object -Line` 数行 ——
> 它只统计**非空行**（`lum_assert.h` 会得到 21 而不是 28）。用
> `[System.IO.File]::ReadAllLines($path, [Text.UTF8Encoding]::new($false)).Count`。

---

## 8. 已知架构债

按影响排序，细节见对应条目：

1. **ECS 指针失效规则未成文** —— 组件池用 `realloc` 增长，`lum_ecs_add/get` 返回的裸指针
   会被后续 `add` 失效，而头文件零注释。→ `CONTRACT.md`
2. **线程与单例模型未成文** —— 全库 0 处提及线程，却有一堆全局单例。→ `CONTRACT.md`
3. **`Lum_Debug` 改变公开面** —— `Lum_Assert`、`Lum_Log_Trace/Debug`
   随编译定义变化，导致"同版本号、不同 API 面"。
   （`lum_vec2/3/4_print`、`lum_mat4_print` 已修：改为始终声明，输出走日志模块的
   Debug 级别、由运行时 `min_level` 过滤 —— 见 `CONTRACT.md` §7。）
4. **存档格式契约不可见** —— `Lum_Scene_Io_Version` 定义在 `src/lum_scene_io.c`，
   且 `version/100` 的 major-only 门当前接受 0–99。
5. **返回值约定三套并存** —— `int` 1/0、`bool`、哨兵（`lum_shader_get_uniform_location` 返回 -1）。
6. **`lum_math2d` 的 11 个谓词返回 `int`** 而非 `bool`，与全库其它谓词不一致。
7. **三处未使用 include**（见第 4 节）。
8. **无测试** —— `tests/` 当前为空，架构上的回归网不存在。

---

## 9. 相关文档

| 文档 | 内容 | 状态 |
|---|---|---|
| [`architecture.md`](architecture.md) | 本文 | ✅ |
| [`CONTRACT.md`](CONTRACT.md) | 跨模块语义契约（线程/失败/所有权/坐标系/枚举稳定性） | ✅ |
| [`versioning.md`](versioning.md) | 两条版本线（引擎 / glimmer 分开计数）、兼容性矩阵、四个版本轴 | ✅ |
| [`modules/*.md`](modules/base.md) | 逐层参考（base / platform / render / assets / serial / scene / components / effects / subsystem） | ✅ |
| `include/*.h` | 逐符号契约（Doxygen 语法，二期渲染） | 进行中（35 个头中 4 个已有标记） |
| `tools/api-coverage.ps1` | 列出缺注释的公开符号 | 待写 |

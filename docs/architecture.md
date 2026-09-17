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

| 层 | 职责 | 模块 | 依赖方向 |
|---|---|---|---|
| ① 基础 | 无副作用的地基 | `lum_version` `lum_log` `lum_assert` `lum_math2d` `lum_collision` `lum_vector` `lum_list` | 只准层内向下 |
| ② 平台 | 与操作系统/窗口打交道 | `lum_filesystem` `lum_window` `lum_input_codes` `lum_input` `lum_time` | → ① |
| ③ 渲染 | GPU 资源与 2D 绘制 | `lum_shader` `lum_texture` `lum_atlas` `lum_camera` `lum_transform` `lum_renderer2d` `lum_material` `lum_render_target` | → ①② |
| ④ 资源 | 磁盘资源的缓存与复用 | `lum_assets` | → ①③ |
| ⑤ 序列化 | 与格式无关的读写抽象 | `lum_serial` | → ① |
| ⑥ 场景 | 实体/组件/视图与存档 | `lum_ecs` `lum_scene` `lum_scene_io` | → ①③④⑤ |
| ⑦ 扩展组件 | 挂在实体上的现成能力 | `lum_components` `lum_component_glow` `lum_component_beam` | → ③⑤⑥ |
| ⑧ 特效 | 光晕/光束的绘制函数 | `lum_effects` `lum_glow` `lum_beam` | → ①③ |
| ⑨ 子系统 | 自成体系的垂直功能 | `lum_text_renderer` `lum_ui` | → ①③（`ui` 另需 ②） |
| ⑩ 编排 | 引擎唯一的状态机 | `lum_application` | → 除自身外全部 |

### 真正的叶子（不含任何引擎头）

消费者可以只包含它们而不拉进任何其它模块：

```
lum_version.h   lum_log.h   lum_math2d.h   lum_ecs.h
lum_filesystem.h   lum_window.h   lum_input_codes.h   lum_time.h
lum_atlas.h   containers/lum_list.h   containers/lum_vector.h
```

**唯一的层内依赖是 `lum_assert.h → lum_log.h`** —— `Lum_Assert` 失败时要通过日志报错，
所以 `assert` 并不是独立的叶子。其余模块都严格向下。

---

## 2. 公共头依赖图（消费者视角）

`A → B` 表示包含 `A.h` 会连带包含 `B.h`。

```
基础
  lum_assert        → lum_log
  （其余基础模块：无引擎依赖）

平台
  lum_input         → lum_input_codes, lum_math2d
  （filesystem / window / time / input_codes：无引擎依赖）

渲染
  lum_shader        → lum_math2d
  lum_texture       → lum_math2d
  lum_camera        → lum_math2d
  lum_transform     → lum_math2d
  lum_material      → lum_math2d, lum_shader, lum_texture
  lum_render_target → lum_math2d, lum_texture
  lum_renderer2d    → lum_math2d, lum_camera, lum_transform, lum_texture, lum_material

资源 / 序列化
  lum_assets        → lum_texture, lum_text_renderer
  lum_serial        → lum_math2d, lum_ecs, containers/lum_vector

场景
  lum_scene         → lum_math2d, lum_ecs, lum_camera, lum_texture, lum_render_target
  lum_scene_io      → lum_ecs

扩展组件 / 特效
  lum_components    → components/lum_component_glow, components/lum_component_beam
  lum_component_glow→ lum_scene
  lum_component_beam→ lum_scene
  lum_effects       → effects/lum_glow, effects/lum_beam
  lum_glow          → lum_math2d
  lum_beam          → lum_math2d

子系统 / 编排
  lum_text_renderer → lum_math2d
  lum_ui            → lum_math2d, lum_text_renderer
  lum_application   → lum_math2d, lum_log, lum_camera, lum_ecs
```

`include/luminous.h` 直接包含其中 30 个；剩下 4 个（`lum_beam` / `lum_glow` /
`lum_component_beam` / `lum_component_glow`）由 `lum_effects.h` 和 `lum_components.h`
间接拉入。

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
| 1 | `lum_log_init` | 必须最先 —— 后面任何一步失败都要能记录 |
| 2 | `lum_window_init` + vsync | `input` 和 `time` 都要 GLFW 窗口 |
| 3 | `lum_input_init` | 需要窗口句柄装回调 |
| 4 | `lum_time_init` | 依赖窗口时钟；`max_fps` 在这里下发 |
| 5 | `lum_renderer2d_init` | 需要已创建的 GL 上下文；同时设清屏色与默认混合模式（Alpha） |
| 6 | `lum_effects_init` | 编译特效着色器，要渲染器就绪 |
| 7 | `lum_ecs_create` + `lum_scene_init` | 世界与内置组件注册 |
| 8 | `lum_assets_init` | 资源缓存；它持有 GL 对象，所以**最后起、最先拆** |

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
| ① | `lum_math2d` | 449 | 116 | 72 |
| ① | `lum_collision` | 735 | 133 | 28 |
| ① | `lum_log` | 131 | 39 | 7 |
| ① | `lum_vector` | 145 | 39 | 15 |
| ① | `lum_assert` | — | 28 | 1（宏） |
| ① | `lum_version` | — | 63 | 0（宏） |
| ① | `lum_list` | — | 84 | 内联 |
| ② | `lum_window` | 167 | 31 | 16 |
| ② | `lum_input` | 159 | 33 | 14 |
| ② | `lum_time` | 152 | 33 | 16 |
| ② | `lum_filesystem` | 118 | 25 | 9 |
| ② | `lum_input_codes` | — | 141 | 枚举 |
| ③ | `lum_renderer2d` | 839 | 89 | 29 |
| ③ | `lum_material` | 253 | 80 | 21 |
| ③ | `lum_texture` | 184 | 60 | 8 |
| ③ | `lum_shader` | 170 | 30 | 14 |
| ③ | `lum_render_target` | 127 | 28 | 6 |
| ③ | `lum_camera` | 100 | 35 | 13 |
| ③ | `lum_transform` | 69 | 26 | 11 |
| ③ | `lum_atlas` | 54 | 26 | 4 |
| ④ | `lum_assets` | 166 | 18 | 6 |
| ⑤ | `lum_serial` | 414 | 149 | 29 |
| ⑥ | `lum_ecs` | 492 | 77 | 28 |
| ⑥ | `lum_scene` | 473 | 167 | 23 |
| ⑥ | `lum_scene_io` | 338 | 11 | 3 |
| ⑦ | `lum_component_glow` | 79 | 36 | 4 |
| ⑦ | `lum_component_beam` | 66 | 32 | 4 |
| ⑦ | `lum_components` | 8 | 14 | 1 |
| ⑧ | `lum_glow` | 173 | 64 | 7 |
| ⑧ | `lum_beam` | 120 | 45 | 5 |
| ⑧ | `lum_effects` | 39 | 11 | 3 |
| ⑨ | `lum_ui` | 1102 | 177 | 42 |
| ⑨ | `lum_text_renderer` | 284 | 34 | 7 |
| ⑩ | `lum_application` | 463 | 137 | 17 |
| ⑩ | `luminous.h`（伞头） | — | 53 | — |
| | **合计（引擎）** | **8069**（30 个 .c） | **2164**（35 个头） | |
| 第三方 | `glad.c` | 2532 | — | — |

最大的三块是 `lum_ui`（1102）、`lum_renderer2d`（839）、`lum_collision`（735）。
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
3. **`Lum_Debug` 改变公开面** —— `Lum_Assert`、`lum_vec2_print` 系列、`Lum_Log_Trace/Debug`
   随编译定义变化，导致"同版本号、不同 API 面"。
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
| `docs/architecture.md` | 本文 | ✅ |
| `docs/CONTRACT.md` | 跨模块语义契约（线程/失败/所有权/坐标系/枚举稳定性） | 待写 |
| `docs/versioning.md` | 编号策略（冻结时定稿） | 待写 |
| `docs/modules/*.md` | 逐层参考 | 待写 |
| `include/*.h` | 逐符号契约（Doxygen 语法，二期渲染） | 进行中（35 个头中 4 个已有标记） |

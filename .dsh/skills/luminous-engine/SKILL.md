---
name: luminous-engine
description: Work inside the Luminous 2D OpenGL engine repository (C99/C11, CMake + MinGW, Windows). Use when reading, modifying, extending, debugging, or documenting Luminous source; when adding or changing engine features; when touching its ECS / scene / renderer2d / effects / UI / collision modules or its include/ public headers; and when preparing or reviewing its API freeze. Carries the verified build commands, semantic contracts, naming conventions, and the traps that silently corrupt behavior.
whenToUse: The task names Luminous, its docs/, its include/ public headers, its modules (lum_ecs, lum_scene, lum_renderer2d, lum_glow, lum_beam, lum_ui, lum_collision, ...), or asks to build, verify, restructure, version, or freeze this engine.
---

# Luminous 引擎

本技能是 `docs/` 的**可执行提炼**，不是替代品。需要细节时按下表回查，不要凭印象写。

| 需要什么 | 去哪里 |
|---|---|
| 分层、依赖图、启动顺序、帧流程、规模 | `docs/architecture.md` |
| 跨模块语义契约（所有权 / 失效 / 线程 / 坐标系 / 枚举稳定性） | `docs/CONTRACT.md` |
| 版本号、依赖矩阵、tag 约定 | `docs/versioning.md` |
| 某个模块的契约与易错点 | `docs/modules/{base,platform,render,assets,serial,scene,components,effects,subsystem}.md` |
| 逐符号参考 | `include/*.h` 的注释（Doxygen 语法；工具链二期才接） |

---

## 1. 构建与验证（每次改完都跑）

```powershell
cmake -B build -G "MinGW Makefiles"     # 生成器必须显式指定
cmake --build build
```

- **必须带 `-G "MinGW Makefiles"`**。不指定会回落 NMake（本机没装）→ configure 失败，
  且会留下错误的 `CMakeCache.txt`，之后必须删掉 `build/` 才能换生成器
- 全量重编译：`cmake --build build --target clean` 后再 build
- 警告必须保持 **0**：`-Wall -Wextra`，严格集再查
  `-Wshadow -Wmissing-prototypes -Wstrict-prototypes -Wold-style-definition`
- 公共头自包含：每个 `include/**/*.h` **单独**用 `-Iinclude` 编译，必须 0 失败
- 冒烟：跑 `build/Luminous_collision_demo.exe`（当前唯一可运行目标）

---

## 2. 结构边界（已确立，勿破坏）

- `include/` = **唯一对外包含根**，只放引擎公共头；`src/` = 实现，**不在包含路径上**
- `third_party/` = 第三方头，**PRIVATE**，消费方看不到
- `CMakeLists.txt` 用 `target_include_directories(Luminous PUBLIC include)`
  + `target_include_directories(Luminous PRIVATE third_party)`
- 新增公共头 → 放 `include/`，并按层加进 `include/luminous.h` 伞头
- 子目录头（`components/` `containers/` `effects/`）在 `.c` 里**用根相对路径**包含
  （`#include "effects/lum_beam.h"`），**不要写裸名** —— 裸名依赖同目录查找，
  在 `src/` 与 `include/` 分离后会失效

---

## 3. 命名与风格

三套前缀，规则**各不相同**，别混用：

| 类别 | 形式 | 例子 |
|---|---|---|
| 函数 | `lum_` + 全小写 snake_case | `lum_ecs_create`、`lum_scene_pick` |
| **类型**（typedef / struct / enum 标签） | `lum_` + **首词首字母大写、后续词全小写** | `lum_Component_transform`、`lum_Ecs_view`、`lum_Texture_flag`、`lum_Render_target` |
| **宏 与 枚举常量** | `Lum_` + **每个词首字母大写** | `Lum_Ecs_Max_Entities`、`Lum_Stage_Init`、`Lum_Texture_Flag_Mipmaps` |

- 数字段算一个词：`Lum_Key_0`、`Lum_Material_Parameter_1i`、`lum_Texture2d` 都合规
- **实测例外**（对公开头全量校验：类型 57/58、宏 84/89、枚举常量 128/134）：
  - `containers/lum_list.h` 与 `containers/lum_vector.h` 的泛型宏用**小写** `lum_`
    （`lum_list_each`、`lum_list_entry`、`lum_vector_get_t`）—— 刻意对齐函数命名
  - **`lum_Ui_Align` 违反类型规则**：后续词应全小写，即 `lum_Ui_align`
  - 内置组件 id 常量用**小写** `lum_` 而非 `Lum_`：
    `lum_Component_transform_id` … `lum_Component_builtin_count`
  - 后两条是**冻结前必须表态**的项 —— 改名属破坏性变更，要么现在改，要么正式写进例外
- **校验这类规则必须区分大小写**：PowerShell 的 `-match` **默认忽略大小写**，
  会把不合规的名字判成合规。用 `[regex]::IsMatch($name, $pattern)`（本仓库踩过）
- 注释用**中文**，Doxygen 子集：`@brief` / `@param` / `@return` / `@note` / `@warning`
- 头文件守卫 `Lum_X_H`，结尾 `#endif //Lum_X_H`
- 2 空格缩进；`Lum_Assert` 只用于**编程错误**，数据相关失败返回 `0` / `false` / `NULL` 并记日志

---

## 4. 会静默出错的陷阱（改代码前必读）

| 陷阱 | 后果 |
|---|---|
| **ECS 指针失效** | `lum_ecs_add` / `get`、`lum_ecs_view_each*`、`lum_scene_add_*`、`lum_component_*_add` 返回的是**指向池内数组的裸指针**；对**同一组件类型**再做一次 `add` 可能使先前全部失效（池走 `realloc`）。要跨多个 add 留引用 → 先存 `lum_Entity` 句柄，事后再 `get` |
| **文本渲染器只支持 ASCII** | 每个字符只有一句 `Lum_Assert(c < 128)` 作防线，其后紧跟 `&glyphs[c]`（`glyphs[128]`）。release 下断言消失 → 传非 ASCII（中文）会**越界读** |
| **`layer` 是第一排序键** | 给每个实体唯一 `layer` 做精确 Y 排序 → 批处理彻底失效 |
| **`layer` vs `category`** | `layer` = 排序键；`category` = 可见性位掩码。**两者正交**，别混用 |
| **`lum_ecs_flush`** | `destroy_entity` 只置死；槽位回收与世代推进发生在 `flush`。自己驱动流程必须手动调，走引擎时 `Lum_Stage_Commit` 会调 |
| **窗口尺寸 ≠ framebuffer 尺寸** | 高 DPI 下不同；渲染与视口**必须用 framebuffer 尺寸** |
| **视口左下原点，屏幕/UI 左上原点** | 混用会上下颠倒 |
| **Screen 阶段的坐标已经是像素** | `Lum_Stage_Screen` 的 VP 是 `ortho(0, sw, sh, 0)`（像素、**左上原点**），文字/图形**直接给像素坐标**。**不要**再套 `lum_app_screen_to_world` —— 那是世界空间换算，会把内容推到画面外。`collision_demo.c` 原来正是这么写的，导致它的 HUD 整段不可见（实测 `screen_to_world(16,24)=(-544,316)`，当像素用则 NDC x=−1.97）。**别照抄旧 demo 的 HUD 写法** |
| **`lum_scene_pick` 依赖 world 矩阵** | 未跑过 `Transform` 阶段 → 用的是上一帧矩阵，命中区域偏移 |
| **无线程安全** | 全库 0 个线程原语，遍地全局单例（log/input/time/window/ui/assets/effects/renderer2d/注册表/`lum_default_theme`）。跨线程即 UB，无诊断 |
| **`Lum_Debug` 改变公开面** | `Lum_Assert` 变 no-op、`lum_vec2_print` 系列消失、`Lum_Log_Trace/Debug` 变 no-op → 同版本号可有不同 API 面 |
| **注册到引擎独占阶段会被静默拒绝** | `lum_app_add_system` 对 `Init / Frame_Begin / Transform / Commit / View_Begin / Frame_End / Shutdown` 返回 0 并告警；而**该告警可能在日志初始化之前发出从而被完全吞掉**（`lum__flow_ensure_init` 早于 `lum_stage_init` 里的 `lum_log_init`）。**引擎内部注册必须用 `lum_flow_add_engine_system`**。历史事故：引擎自己的帧边界系统用了公开入口 → 从未注册 → 整帧不清屏、不 flush、不 swap → **窗口纯白且零报错**。用户侧表现就是"白屏" |
| **不要用 PowerShell 文本 cmdlet 读写这些文件** | `Get-Content` / `Set-Content` 会按 ANSI(CP936) 解码 UTF-8 再写回，**损坏中文注释**（本仓库踩过）。用 `edit` / `write` 工具，或 .NET 字节 API |

---

## 5. 版本与提交

- 三段式 `<开发阶段> <主要版本号>.<patch 版本号>`，如 `prototype 0.1`
- **唯一来源**是 `include/lum_version.h`；CMake 在 `project()` **之前**读它，
  改版本只改这个头文件，CMake 自动跟随
- 阶段词**不参与比较**；编译期比较用 `Lum_Version_Encode(major, patch)`
  （刻意纯整数乘加，`#if` 不认 `(unsigned long)` 转型）
- tag 形如 `prototype-0.1`（**注解** tag）。**已 push 的 tag 不动**
- 公共 API 变更属破坏性变更 → 先查 `docs/CONTRACT.md` §9 的稳定性清单

---

## 6. 文档维护

- 新文档放 `docs/`，并更新 `docs/README.md` 的状态表
- **写作纪律：只写能从代码验证的内容**。凡"代码行为已如此、但头文件未写"的，
  标 **⚠ 未成文** —— 那是冻结前要补注释的清单
- 行数统计用 `[System.IO.File]::ReadAllLines(...)`；
  **`Get-Content | Measure-Object -Line` 只数非空行**，会低估
- 验证方式：编码（严格 UTF-8 解码）、内部 `.md` 链接逐一 `Test-Path`

---

## 7. 当前待办（冻结前）

按优先级，细节见 `docs/architecture.md` 第 8 节：

1. **ECS 指针失效 / 线程模型未成文** → 补进 `lum_ecs.h` 注释
2. **`Lum_Debug` 改变公开面** → 改成 PUBLIC 编译定义或生成配置头
3. **`lum_scene_io.h` 只有 11 行**，存档格式契约（版本、兼容承诺）不可见
4. **返回值约定三套并存**（`int` 1/0、`bool`、哨兵）；
   `lum_math2d` 有 11 个谓词返回 `int` 而非 `bool`
5. **存档版本门形同虚设**：`version/100` 只比 major，常量 `1u` → 实测接受 0–99
6. **三处未使用 include**：`lum_math2d→log`、`lum_window→math2d`、`lum_time→math2d`
7. **`tests/` 为空**，没有回归网
8. 仍未处理：`resources/` 下两个 Monotype 商业字体（未提交）、
   `tools/api-coverage.ps1`（未写）、`CMakePresets.json`（未加）

---

## 8. RTS（本项目）进度与约定

### 位置与命名

- 代码在 **`games/rts/`**；标识符用 **`rts_` / `Rts_`** 前缀，与引擎的 `lum_` / `Lum_` 明确区分
- 目标：`Luminous_rts`（游戏本体）、`Luminous_rts_check`（**无窗口**回归检查）
- 游戏是**纯消费者**：`games/` 下的源码只用 `-Iinclude` 就能编过，
  看不到 `src/`，也看不到 `third_party/`。**这条要一直保持**

### 里程碑

| | 内容 | 状态 |
|---|---|---|
| M1 | 相机（键盘/边缘/中键拖拽 + 滚轮以光标为锚缩放）＋ 框选与单击选中 | ✅ 完成 |
| M2 | 移动与命令（右键下令、命令队列） | 待做 |
| M3 | 寻路（网格 A\*）＋ 单位互斥/建筑阻挡 | 待做 |
| M4 | 战斗（索敌/冷却/伤害/死亡/血条） | 待做 |
| M5 | 经济与建造 | 待做 |
| M6 | HUD ＋ 小地图（用 `lum_View` + `category_mask`） | 待做 |
| M7 | 存档、AI、胜负 | 待做 |

### 无窗口验证路径（重要）

`lum_flow_run_steps(ecs, n, dt)` 不碰 GL，可以直接跑 `Input..Commit`（含 `Transform`），
因此**选择/移动这类逻辑能脱离窗口测试**。`games/rts/rts_check.c` 就是这么做的：
建 ECS → 生成单位 → 跑一步 → 直接调逻辑函数 → 断言。

**推论**：游戏逻辑不要内部去取 `lum_app_get_camera()` / `lum_window_get_*()` 这类全局单例，
而是**显式把 camera 和 window_size 作为参数传进来**，否则这段逻辑就无法 headless 测试。
（M1 已经把 `rts_selection_resolve` 改成这个形状。）

### M1 期间确认的约束

- **`lum_app_screen_to_world` 依赖窗口单例**：无窗口时 `lum_window_get_width()` 返回 0，
  转换会断言失败/除零。所以可测试的代码用 `lum_camera2d_screen_to_world(cam, p, window_size)`
- **框选 AABB 必须与 `lum_scene_pick` 用同一套判定**（列向量长度=层级缩放、平移=m[12..13]、
  忽略旋转、**闭区间**），否则会出现"点得到但框不到"。闭区间意味着**相切算命中**
- **单位必须同 layer 才能合批**：`Rts_Layer_Units` 一个常量给所有单位；
  不要按 Y 排序给每个单位唯一 layer
- **ECS 三组件上限正好用满**：单位 = transform + sprite + unit。
  再加组件就得拆遍历（或改引擎）
- 多单位选择统计、清空都是用**单组件视图**一趟 O(n) 完成，够用；上千单位再考虑空间索引
- **边缘滚动必须判断指针确实在客户区内**：指针在窗口外时 GLFW 返回的客户区坐标会越界
  （负值或大于宽高），不判断就会被当成"一直贴边"→ 视角一路飞走
  （实测启动后 `camera.y` 从 0 掉到 −1228，单位全部飞出画面，看起来像"屏幕是空的"）

### 诊断手法（比截图可靠）

排查"画面不对"时，**不要靠截图**（GL 内容常常截不到，还会误抓桌面）：

1. **帧缓冲读回**：在 `Frame_End` 里、`swap_buffers` 之前用 `glReadPixels` 读回，
   扫描出**所有非清屏像素的包围盒**并打日志。一眼就能区分"没清屏 / 没画 / 画错位置"。
   （需要临时在引擎侧加，因为 `glad` 对游戏不可见）
2. **数据侧探针**：在游戏系统里自己遍历 transform+sprite，把 sprite 系统*应该*看到的数据
   （world 平移、size、layer、texture id）打出来，与相机状态对照
3. **注意探针读的时机**：`lum_renderer2d_get_draw_call_count()` 在 `begin_frame` 就被清零，
   在 `Screen` 阶段读它**必然是 0** —— 我为此白查了一轮。要读它得放在 `Frame_End` 之后

### 施工纪律

1. 发现**引擎层面的新陷阱/新约定** → **先补本 skill**，再继续写游戏代码
2. 里程碑完成 → 更新上面的表 + 把"临时绕法"升级成引擎能力或写进 `docs/CONTRACT.md`
3. 每次改完跑：全量构建（0 warning）＋ `Luminous_rts_check` ＋ `Luminous_rts` 冒烟

### 已修的引擎侧问题（本项目推进中发现）

- `collision_demo.c` 的 HUD 曾整段画在画面外（多套了一层 `screen_to_world`）—— 已修，
  详见 §4 的 "Screen 阶段的坐标已经是像素"

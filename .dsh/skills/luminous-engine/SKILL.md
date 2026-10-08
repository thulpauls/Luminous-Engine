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
| 两条版本线（引擎 / glimmer）、兼容性矩阵、依赖矩阵、tag 约定 | `docs/versioning.md` |
| 某个模块的契约与易错点 | `docs/modules/{base,platform,glimmer,assets,serial,scene,components,effects,subsystem}.md`（渲染器模块 = **glimmer**） |
| **做 3D 渲染器 / 平台扩展的施工路线** | **`docs/design/glimmer3d.md`** —— 分期大纲（M1 最简渲染器 → M9 与 2D 合成），每期代码规模、验收判据、施工流程、进度台账。**动手前先读它，按台账推进并回勾** |
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
- 三个构建目标，依赖是单向链 **glad → glimmer → Luminous**：
  `build/libglimmer.a` 是**基础平台**（数学 math2d/transform + 平台 log/assert/filesystem/
  window/input/time + 渲染器 renderer2d/shader/texture/material/render_target/atlas/camera/
  text_renderer），`build/libLuminous.a` 是引擎高层。
  平台占实现行数的 43.9%（3776 / 8598 行）。换 3D 渲染器时替换的是 `glimmer` 这一个目标
- 警告必须保持 **0**：`-Wall -Wextra`，严格集再查
  `-Wshadow -Wmissing-prototypes -Wstrict-prototypes -Wold-style-definition`
- 公共头自包含：每个 `include/**/*.h` **单独**编译，必须 0 失败。
  其中 `include/glimmer/*.h` 还要额外用**只给 `-Iinclude`** 编一遍 ——
  证明平台的公共头没把 glad/stb/GLFW 泄漏给消费者
- **平台边界**：`& .\tools\check-glimmer-boundary.ps1` 必须通过
  （三条规则：glimmer 越界包含 / glimmer 公共头泄露第三方头 /
  **所有 `glimmer*` 目标的链接行里不许出现 `Luminous`**）。
  这个脚本带 **UTF-8 BOM** —— 它含中文，而无 BOM 时 Windows PowerShell 会按 CP936 解码、
  直接语法报错（我踩过）；改它请用 .NET `UTF8Encoding($true)` 写回
- 冒烟：**八个**可运行目标都要跑，而且**既数 stderr 又断言"进程自行退出"**
  （只数 Warn/Error 会漏掉"循环被改坏、永远不退出"这类改动 —— 我踩过，见诊断手法 18）——
  `build/Luminous_astar_demo.exe`（A* 寻路演示，交互式，不自退）、`build/Luminous_rts.exe`、
  `build/Luminous_collision_demo.exe`（后两个也是交互式）、
  `build/glimmer_hello.exe`（1.7 s 自退，2 条 Info）、
  `build/glimmer_hello3d.exe`（1.7 s 自退，2 条 Info）、
  **`build/glimmer_instancing.exe`**（M2 实例化 A/B，5.5 s 自退，2 条 Info，末行是自检结论）、
  **`build/glimmer_material3d.exe`**（M3 材质：4096 物体 / 4 材质，2.5 s 自退，末两行是 draw call 自检与耗时小结）、
  **`build/glimmer_check.exe`**（无窗口：3D 数学 / 视图矩阵 / camera3d / mesh / 版本，
  当前 **85 条断言 0 失败**，退出码即结论）。
  stderr 里只允许出现 Info。**渲染相关的改动还要加读帧缓冲那一发**（见诊断手法 17）——
  白屏/画不出来时日志是空的；3D 的深度/遮挡类改动必须做**开关 A/B 并读回颜色与深度**
  （只画一个方块证明不了任何事）

---

## 2. 结构边界（已确立，勿破坏）

- `include/` = **唯一对外包含根**；`src/` = 实现，**不在包含路径上**
- `third_party/` = 第三方头，**PRIVATE**，消费方看不到
- **基础平台叫 glimmer**，在 `include/glimmer/` + `src/glimmer/`（20 个头 / 17 个 .c），
  是独立构建目标，**能脱离引擎单独运行**（`glimmer_hello` 就是证明）。
  消费者只 `#include "glimmer/glimmer.h"`；边界与允许依赖见 `docs/modules/glimmer.md`，
  由 `tools/check-glimmer-boundary.ps1` 强制
- **方向永远是 Luminous → glimmer**：往 glimmer 里加东西前先问"引擎高层需要它吗"，
  需要就说明它不该进 glimmer
- `CMakeLists.txt` 用 `target_include_directories(<lib> PUBLIC include)`
  + `target_include_directories(<lib> PRIVATE third_party)`；**源文件清单显式列出**（不用 GLOB），
  避免"忘了加文件却静默通过"，也避免 `src/glimmer/` 被引擎目标重复编译
- 新增公共头 → 放 `include/`（平台相关的一律放 `include/glimmer/`），并加进伞头
  `include/glimmer/glimmer.h`（平台）或 `include/luminous.h`（引擎高层）
- 子目录头（`glimmer/` `components/` `containers/` `effects/`）在 `.c` 里**用根相对路径**包含
  （`#include "glimmer/lum_renderer2d.h"`、`#include "effects/lum_beam.h"`），**不要写裸名**
  —— 裸名依赖同目录查找，在 `src/` 与 `include/` 分离后会失效

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
- **glimmer 的符号前缀（阶段 2，未做）**：渲染器模块已隔离为 `glimmer`（目录/头文件路径/
  构建目标），但**符号仍是 `lum_*` / `Lum_*`**。要改成 `glimmer_*` / `Glimmer_*` 是独立一步：
  触及引擎 + 游戏 + demo 约 170 处调用点，外加类型名（`lum_Texture2d`、`lum_Material`、
  `lum_Camera2d`、`lum_Font`… 还被 scene/assets/ui/effects 用着）。改之前先想清楚
  "glimmer 是否要成为独立命名域" —— 这属于冻结前必须表态的项
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
| **Screen 阶段的图形直接给像素坐标** | `Lum_Stage_Screen` 的 VP 是 `ortho(0, sw, sh, 0)`（像素、**左上原点**）。`draw_rect` / `draw_line` 等**直接给像素坐标**。**不要**套 `lum_app_screen_to_world`（世界换算）—— `collision_demo.c` 曾这样写，HUD 整段被推到画面外（实测 `screen_to_world(16,24)=(-544,316)`，当像素用则 NDC x=−1.97） |
| **Screen 阶段的文字必须换 VP** | 文字渲染器的几何是 **Y 向上**的（字形位图在上传前被翻成 GL 底朝向，见 `lum__tr_load_glyph`），而 Screen 阶段的 VP 是 `ortho(0, sw, sh, 0)`（Y 向下）。**只挪基线位置解决不了朝向**——必须在绘制期间临时换成 Y 向上的像素 VP。用 **`lum_tr_draw_screen(font, text, top_left, layer, color)`**，它内部就是这么做的（靠"每条命令记录 VP 快照"只影响这几条命令）。直接用 `lum_tr_draw_layer` 画 HUD 会得到**上下颠倒**的文字。这个坑咬过四处：引擎 UI 自己手写、`collision_demo`、RTS，以及我第一次的修法（只翻了位置） |
| **同 `layer`、不同 VP 的命令，绘制顺序 ≠ 记录顺序** | 排序键是 `layer → view_projection → has_scissor → scissor → material → texture->id → primitive → blend → sequence`，`sequence`（记录序）是**最后一项**：只要 VP / material / texture / scissor 有一个不同，命令就被这些**状态字节**重排，`sequence` 根本轮不到。Screen 阶段画 HUD 正好撞上：背板用阶段自带的 Y 向下 VP，文字经 `lum_tr_draw_screen` 临时换成 Y 向上 VP → 两者同层时 `memcmp` 把 Y 向上的 VP 排在前面，于是**先记录的背板反而最后画、盖在文字上**（0.88 alpha 把整段 HUD 压成灰影）。实测（`glReadPixels` 数背板矩形内像素）：同层时 93112 像素**全部**判暗、最亮 `(31,33,39)`；背板 9 层 / 文字 10 层后最亮 `(191,196,205)`，文字恢复。**规则：HUD 背板与 HUD 文字必须各占一层，背板层 < 文字层**；同层只对"状态完全一致"的命令保证记录顺序 |
| **指针可能在窗口外，客户区坐标会越界** | GLFW 的 `glfwGetCursorPos` 在指针离开窗口时返回**越界坐标**（实测拿到 `(1996,1218)`，而窗口只有 1280×720）。不判断的后果：① 被当成"一直贴边"→ 相机一路飞走；② 用越界坐标当框选端点 → 框被画到画面外，看起来像"框瞬间消失"，且松手后什么都选不中。**用窗口尺寸做 inside 判断，并把拖动端点夹到窗口边界** |
| **`lum_scene_pick` 依赖 world 矩阵** | 未跑过 `Transform` 阶段 → 用的是上一帧矩阵，命中区域偏移 |
| **sprite 的锚点必须与 pick 一致** | `lum__sys_sprite` 曾传 `origin = (0,0)`（**左下角**锚点），而 `lum_scene_pick` 与框选 AABB 按"以实体局部原点为**中心**"算 → **绘制与命中差半个 `size`**。表现：选中角标相对单位偏下偏左、点选要多点半个身位。绘制侧现已统一为 `origin = size/2`。**改任何一处的锚点/尺寸语义都要同时改三处**：`lum__sys_sprite` 的绘制、`lum_scene_pick`、`lum_component_glow`（它的渲染中心也取 `m[12..13]`）。依据见 `docs/CONTRACT.md` §8 |
| **无线程安全** | 全库 0 个线程原语，遍地全局单例（log/input/time/window/ui/assets/effects/renderer2d/注册表/`lum_default_theme`）。跨线程即 UB，无诊断 |
| **`Lum_Debug` 改变公开面** | `Lum_Assert` 变 no-op、`Lum_Log_Trace/Debug` 变 no-op → 同版本号可有不同 API 面。**`lum_vec2/3/4_print`、`lum_mat4_print` 已不在此列**：现在始终声明、始终编译，输出走 `lum_log_message(Lum_Log_Level_Debug, ...)`，由运行时 `min_level` 过滤。**注意 `Lum_Log_Debug` 是编译期宏**（未定义 `Lum_Debug` 时展开成 `((void)0)`）—— 要"连 Release 也能开"的调试输出必须直接调 `lum_log_message`，用那个宏写等于在 Release 里永远打不出来 |
| **调试输出别用裸 `printf`** | 裸 `printf` 绕开日志模块：没有时间戳/级别/位置前缀、不受 `min_level` 控制、永远进不了 `lum_log_set_file()` 的文件；还自带 ANSI 转义，重定向进文件就是乱码（`lum_log_enable_color(false)` 关不掉裸转义）。`lum_math2d.c` 那四个打印原来就是这样（声明还套着 `#ifdef Lum_Debug`）。而且它用 `log10(floor(\|v\|))` 估数值宽度：`\|v\| < 1` 时 `floor` 得 **0** → `log10(0) = -inf` → 转 `int` 是 **UB**（本机实测宽度 `-2147483642`，printf 域宽彻底失控）；就算不 UB 也整体偏 ±1（`v = 1.0` 估 6、实际 5）。**按格式化后的字符串实测宽度**才对 |
| **注册到引擎独占阶段会被静默拒绝** | `lum_app_add_system` 对 `Init / Frame_Begin / Transform / Commit / View_Begin / Frame_End / Shutdown` 返回 0 并告警；而**该告警可能在日志初始化之前发出从而被完全吞掉**（`lum__flow_ensure_init` 早于 `lum_stage_init` 里的 `lum_log_init`）。**引擎内部注册必须用 `lum_flow_add_engine_system`**。历史事故：引擎自己的帧边界系统用了公开入口 → 从未注册 → 整帧不清屏、不 flush、不 swap → **窗口纯白且零报错**。用户侧表现就是"白屏" |
| **不要用 PowerShell 文本 cmdlet 读写这些文件** | `Get-Content` / `Set-Content` 会把 UTF-8 源码按控制台代码页（CP936）解码再写回，**损坏中文**。**这不是 PowerShell 5.1 的专利：本仓库在 `pwsh` 里也照样踩了一次**（`(Get-Content -Raw f) -replace … \| Set-Content -NoNewline f`）。最阴的地方是它**完全不报错**：编译通过、测试全绿，只有中文标签/注释变成 `膨胀�?` 这种乱码，肉眼不看那几行就发现不了。规则：源码一律用 `edit` / `write` 工具，或 .NET API（`[System.IO.File]::ReadAllText/WriteAllText` + `UTF8Encoding($false)`） |
| **`draw_line` / `draw_rect` 的粗细是【世界单位】，会被相机 zoom 缩放** | zoom < 1 时，按"像素直觉"写的细线（`thickness = 1.0`/`2.0`）在屏幕上只有 0.47/0.94 px → **亚像素四边形光栅化不出任何片元，整条线静默消失**（0 像素、无警告、无报错）。本仓库实测：`Luminous_astar_demo` 的 zoom = 0.46875，网格线（1.0）与两张表的边框（2.0）全部一个像素都没画，而同一批代码里 `thickness = 3.0` 的路径线（1.4 px）正常。**修法：所有世界空间的线宽/圆点半径一律 `thickness / zoom`**（按像素写、换算后传）。注意 `Screen` 阶段是像素空间，不受此影响 —— 这也是为什么 RTS 里 Screen 阶段的框选边框（1.5 px）一直正常 |
| **字号小于 ~10 px 的文字【几乎看不见】**（世界空间文字尤其） | 字形位图是按字号的【像素】数光栅化的，再把四边形放到屏幕上。`lum_tr_draw_layer`（世界空间）的字号是**世界单位**，位图按同一个数光栅化，屏幕上却要乘 zoom —— 等于把一张小位图放大，细小笔画的覆盖被线性过滤摊没。实测墨迹峰值覆盖（字号 = 光栅化高度，底色为 demo 的 closed 填充）：**7 px → 0.00（整串字一个像素都没有）**、8 px → 0.22、10 px → 0.66、12 px → 0.69、15 px → 0.85、20 px → 0.95。我给 A* demo 的格内数字一开始用 7 世界单位（≈8.8 px），结果 220 个标签全部画了却"看不见"，查了四轮（先怀疑 layer、再怀疑颜色阈值、再怀疑字体加载，最后按字号阶梯量覆盖才定性）。**规则：要读的文字一律走屏幕空间 `lum_tr_draw_screen`（1:1 光栅化），字号 ≥ 12 px；世界空间文字只适合"大"字（≥ 20 世界单位）** |
| **怀疑编码被弄坏：这样检测、这样恢复** | 检测：`New-Object System.Text.UTF8Encoding($false,$true)` 再 `GetString($bytes)` —— **严格解码失败就是坏了**；再 grep 一个已知中文串复核（注意别用有歧义的串：我拿 `禁止切角` 复查，结果它同时命中另一条测试的标签，白紧张一轮）。恢复：`git checkout -- <file>` 拿回已验证版本，再用 `edit` 工具重放改动，**然后必须重跑严格警告门** |
| **严格警告门不是可选项** | 普通 `cmake --build` **不含** `-Wshadow` 等严格集。我新写的测试里 `const lum_Vec2 s` 遮蔽了 `main` 顶层的 `rts_Selection s`，构建全过、31 组测试全绿，只有严格门报 `-Werror=shadow`。顺带：手工编辑后行尾可能变成 LF/CRLF 混用，用 .NET 统一（`ReadAllText` → `Replace("\r\n","\n")` → `Replace("\n","\r\n")` → `WriteAllText`） |
| **`Select-String -SimpleMatch` 会关掉正则** | 给它 `'a\|b\|c'` 这类模式，它按**整串字面量**匹配 → **静默 0 命中**，看起来像"没有引用/没有耦合"。我做 CircuitAI 耦合度统计时就这样得到过假的 `0/302`。要用 `\|` 就别加 `-SimpleMatch`；注意 `Select-String` 默认支持正则且**不区分大小写** |

---

## 5. 版本与提交

- **两条版本线，分开计数**：引擎（`include/lum_version.h`，当前 `prototype 1`）与
  glimmer 平台（`include/glimmer/glimmer_version.h`，当前 `prototype 2`）。
  格式统一为 **`<开发阶段> <迭代次数>`**，没有 major/minor/patch 三段
- **兼容性声明**：引擎版本头里写明它针对哪个 glimmer 版本（当前严格相等）。
  改一条版本线而忘了另一条 → **配置期 CMake `FATAL_ERROR`** + **编译期 `#error`** 两道门
  （都实测过会拦）。见 `docs/versioning.md` 的兼容性矩阵
- **唯一来源**是那两个头文件；CMake 在 `project()` **之前**读它们，改版本只改头文件
- 阶段是**有序数值**（prototype 1 < alpha 2 < beta 3 < rc 4 < release 5），
  编译期比较用 `Lum_Version_Encode(stage, iteration)` = `stage*256 + iteration`
  （刻意纯整数乘加，`#if` 不认 `(unsigned long)` 转型）；
  **阶段名串与数值表必须同步改**（两者没有语言层面的绑定）
- 迭代次数在本阶段内 +1，**阶段推进时从 1 重新开始** → 跨阶段比较必须带上阶段
- tag 形如 `engine-prototype-1` / `glimmer-prototype-1`（**注解** tag）。**已 push 的 tag 不动**
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
2. **`Lum_Debug` 改变公开面** → 改成 PUBLIC 编译定义或生成配置头。
   `lum_math2d` 的四个打印**已修**（始终声明 + 走日志级别过滤），只剩
   `Lum_Assert` 与 `Lum_Log_Trace/Debug`
3. **`lum_scene_io.h` 只有 11 行**，存档格式契约（版本、兼容承诺）不可见
4. **返回值约定三套并存**（`int` 1/0、`bool`、哨兵）；
   `lum_math2d` 有 11 个谓词返回 `int` 而非 `bool`
5. **存档版本门形同虚设**：`version/100` 只比 major，常量 `1u` → 实测接受 0–99
6. **两处未使用 include**：`glimmer/lum_window`→math2d、`glimmer/lum_time`→math2d
   （`lum_math2d→log` 那条已经用上了：调试打印现在真的走日志模块；
   隔离时又清掉三处 `#include <GLFW/glfw3.h>` 死包含 —— 渲染器里没有任何 glfw 调用）
7. **`tests/` 为空**，没有回归网
8. **glimmer 阶段 2：符号前缀**（见 §3）—— `lum_*` → `glimmer_*`，
   约 170 处调用点，属破坏性变更，冻结前必须表态
9. **平台边界要持续守**：往 `src/glimmer/` 里加东西前先看 `glimmer_hello` 能不能编过 ——
   它只链接 `libglimmer.a`，是"平台没被高层污染"的活判据
10. **3D 渲染器按 `docs/design/glimmer3d.md` 分期推进**（M1 最简渲染器 → M9 与 2D 合成，
    每期都写了代码规模与验收判据）。**M1 与 M2 已完成**：M1 = 1022 行（`lum_renderer3d` /
    `lum_mesh` / `lum_camera3d` + 数学补全），判据 = headless 断言 + 深度 A/B 读回；
    **M2 = 实例化**（模型矩阵走实例属性 location 2..5 + divisor 1，`draw_mesh` 退化成"画 1 个实例"，
    只保留一条绘制路径）—— 判据 = draw call 恒为 1 + 位置逐像素校验 + 与逐实例提交图像一致，
    实测提交耗时降 **169×（1000）/ 573×（10000）**。
    **M3 = 材质与贴图**（顶点加 uv → 32 字节固定布局、`Lum_Texture_Flag_Srgb`、`lum_Material3d`、
    绘制接口改为传材质）—— **批次单位变成（网格，材质），draw call 数 = 网格数 × 材质数、与实例总数无关**；
    实测 4096 物体 / 4 材质 = **4 次 draw call**（逐物体提交是 4096 次，耗时 52.7×）。
    下一步是 **M4 分趟与剔除**。新代码**沿用 `lum_*` / `Lum_*` 前缀与 `lum_*.h` 文件名**
    （用户已定；统一重命名仍留到"阶段 2"），放 `include/glimmer/` + `src/glimmer/`，
    每期结束回勾该文档第 8 节的台账并更新状态列
11. 仍未处理：`resources/` 下两个 Monotype 商业字体（未提交）、
    `tools/api-coverage.ps1`（未写）、`CMakePresets.json`（未加）
    （`tools/check-glimmer-boundary.ps1` 已写并进门禁）

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
| M2 | 移动与命令（右键下令、命令队列） | 🔶 移动 + 右键下令完成（`rts_orders.*`、网格编队展开）；**命令队列待做** |
| M3 | 寻路（网格 A\*）＋ 单位互斥/建筑阻挡 | ✅ 完成（`rts_navgrid.*` / `rts_path.*` / `rts_collision.*`） |
| M4 | 战斗（索敌/冷却/伤害/死亡/血条） | 待做 |
| M5 | 经济与建造 | 待做 |
| M6 | HUD ＋ 小地图（用 `lum_View` + `category_mask`） | 待做 |
| M7 | 存档、AI、胜负 | 待做 |

### A* 演示程序（`a_star_demo.c` → `Luminous_astar_demo`）

**刻意复用 `games/rts` 的 `rts_navgrid` + `rts_path`**，也就是游戏真正在跑的那一份，
而不是另写一个教学版 —— 目的是让两份实现不会各自漂移：改了 `rts_path.*` 就等于
改了游戏和 demo 的算法。所以它只做"画"和"编辑"，算法一行都不重写。
代价是这个目标要把 `games/rts` 加进包含路径（它不属于引擎公共头）。

- 三个鼠标键各管一件事：**左键设起点、中键设终点、右键画/擦墙**（单击切换、拖动连画）；
  任何编辑都会把搜索重置回"只有起点入队"的初始态
- **手动控制循环**才是重点：`[空格]` 推进**一步 = 定案一个格子**、`[回车]` 一次跑完、
  `[B]` 重置、`[A]` 自动播放（每帧 4 格）；另有 `[C]` 清墙 / `[M]` 随机墙 / `[V]` 两张表 /
  `[G]` 网格线 / `[Q]` 退出
- 画的是 A* **真实定案过的格子**（靠 `rts_path_last_visited` / `rts_path_last_open`
  这两个只读查询），HUD 报的是**真实读数**（定案格数、路点数、绕路倍数），不是示意
- **两张表的画法**（`layer` 各自独立：closed 垫在网格线之下、open 压在网格线之上）：
  closed = **半透明纯色填充**，逐格画会爆掉命令表，所以按**同行连续段合并成矩形**
  （`demo_draw_runs` 返回矩形数，HUD 会显示）；open = **每个格心一颗小圆点**
  （半径过 `demo_px()`）。实测 60 步时：closed 60 格 → 21 个矩形 / 11564 像素；
  open 46 个点 → 414 像素（每点约 3×3），抽样 6 个点全部 3 px 宽、中点与格心差 ≤ 1 px。
  **最初用的是"只画集合边界"的边框**，同心方框叠几层之后糊成一片，才换成本方案
- **格内标 g / h**（closed 格左上角 g、右上角 h，各一位小数）取自 `rts_path_last_costs()`，
  它是 `s_g` 与 `f - g` 的只读读数（回归用例 [41] 钉住：纯对角一步的 g 必须是 √2 而不是
  步数 1、每一格的 h 必须等于到终点的 octile、绕路时终点的 g 必须大于直线 octile）。
  文字**必须走屏幕空间** `lum_tr_draw_screen`，见下面那条字号陷阱。视野与精度不可兼得：
  一屏 12×12 格（每格 80 px、zoom 2.5）刚好并排放下两个 4 字符数字
- **相机跟随 + 键盘平移**：跟随"刚刚定案的那一格"（`rts_Path_search.current`，新增字段），
  WASD/方向键一按就接管（`g_follow=false`），`[F]` 切回。相机更新放在 **`Pre`**：
  ① 引擎在 `Frame_Begin` 把相机灌给渲染器，这里改的是下一帧的 VP（一帧延迟）；
  ② 渲染阶段 `ctx->dt` 恒为 0，平移速度必须按真实 dt 积分
- **屏幕空间的 HUD/标签要用"本帧渲染真正用的那个相机"**：引擎在 `Frame_Begin` 灌 VP，
  而 `sys_pre` 随后就把相机挪向下一帧 —— 所以在 `Screen` 阶段读实时相机做世界→屏幕，
  会比网格晚一帧，平移时文字整片甩出去。做法是在 **`Input` 阶段**抓一份 `g_frame_cam`
  （在 `Pre` 之前），`Screen` 阶段用它换算
- 墙**直接写 `blocked` 数组**，不走 `rts_navgrid_add_obstacle` —— 后者会按 RTS 的
  需要把障碍膨胀一格（单位有体积），对逐格编辑的 demo 会变成"点一下出 3×3"
- 它也是"改完 A* 之后肉眼验收"的最快途径：比开 RTS 摆布局快得多
- 自检写在 `Start` 里并打日志：相机缩放（网格四角 → 窗口四角）、
  点击往返（格心 → 像素 → 世界 → 格子）、三个 A* 场景（空地 / 封死 / 开口）。
  全是纯函数，冒烟时从 stderr 就能读到结论

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

### M2 期间确认的约束

- **"下令"可以放 Input，"改状态"必须放 Pre**：判定依据不是"操作重不重要"，而是
  **它读不读 `transform.world`**。
  - 右键下令 = 屏幕→世界，只需要 camera + window_size → **不读 world 矩阵** →
    放 Input，于是**同一步的 Pre 就能开始走，零帧延迟**
  - 框选/点选 = 要 `lum_scene_pick` 读 world → **必须放 Post**
  - 推进移动 = 写 `transform.position`（local）→ **必须放 Pre 或更早**；
    放到 Post 之后写 local 要到下一步才反映到 world，看起来就是"单位慢一帧"
- **移动指令塞在 `rts_Unit` 里，不新开组件**：单位已经用满 transform+sprite+unit，
  再挂一个就变 4 个组件 → 任何"遍历全部单位"的代码都取不到完整数据。
  将来的命令队列**扩字段，不要扩组件数**
- **编队展开必须两趟遍历**：网格列数依赖总数，而 `lum_Ecs_view` 是池上的游标，
  一趟里边数边写会让第一个单位按"目前只见到 1 个"算偏移，全挤在中心。
  也**不能把单位指针攒进数组**（同类型池 add 会 realloc 搬家）→ 老实遍历两趟
- **到点必须落成精确值**：`距离 <= 容差` 或 `一步能跨过` 时直接 `position = target`
  并清指令，而不是"停在容差内"。这样终态唯一（断言可以逐位比较），
  也不会在终点附近抖动。`dt <= 0` 一律直接返回（非步进阶段 `ctx->dt` 就是 0）

### M3 期间确认的约束

- **障碍"膨胀 1 格"可以把碰撞检测整个省掉**：格子取 32 世界单位、单位身宽 22，
  烘焙障碍时向四周多堵 1 圈，于是"格子可通行"就等价于"单位站在这格中心时
  整个身子都在可通行区"。绕行是**走出来的**，不需要任何几何碰撞/推挤代码。
  代价是 1 格宽的通道会被膨胀堵死 —— 布局时要留 ≥3 格宽的缺口
- **A\* 和拉直必须用同一条可通行规则**：对角移动要求两个正交邻居都可通行
  （禁止切角）。如果 A\* 有这条而 `line_clear` 没有，拉直就会把路径"拉"过
  一个 A\* 本来不会走的墙角 —— 表现为单位贴墙穿过去。
  宁可让 `line_clear` 比真实几何更保守（Bresenham 比真线段更粗），保守这一侧才安全
- **终点必须夹到可通行格，且【把夹后的点写回 `move_target`】**：玩家点在墙上时
  夹到最近的空格是合理的，但如果不写回，单位永远够不到原始目标 →
  走到头就重规划，**每帧重规划一次**。这是"夹取"最容易漏的一半
- **路点用定长缓冲 + 走到头重规划，不要动态数组**：单位已经用满 3 个组件槽，
  为路径开新组件会破坏"一次遍历拿到全部单位数据"。定长 16 个路点 +
  超了置 `truncated`、走到头就地重规划，够用且不需要分配器
- **A\* 的工作区用静态数组 + 代次号**：网格有界（64×64）所以一次都不 malloc；
  `stamp[i] == gen` 代替"每次查询清 4096 格"。堆用二叉堆 + 惰性删除
  （弹出时靠 `done` 判旧条目），省掉 decrease-key 和反查表。
  每格最多被 8 个邻居各松弛一次，堆容量取 10 倍格数就绝不溢出
- **必须在 `move_selected` 就算好路径**，不能在 `update` 里边走边算：
  下指令是低频操作（一次几千格 A\* 而已），而 `update` 每帧都跑，放那里会白烧 CPU
- **`lum_collide_*` 的 `depth` 是【整段】最小平移量**，不是"每边推多少"。
  头注释那句"A 沿 -normal、B 沿 +normal 各推 depth"极易读成"每边推一整个 depth"
  —— 那会把两个单位分开到两倍距离。对称分开要**各推 `depth/2`**。
  用测试钉死：边长 20、中心距 10 的一对，推完中心距正好 20.0（刚好接触）。
  碰撞箱取 `sprite.size` 的方形，判交走 `lum_collide_box_box`（内部 SAT），
  外面再加一层"外接圆"宽相排除（方形半宽 h 的外接半径是 h·√2）
- **"不许推进被堵格"这类守卫会把自己否决成死锁**：单位若已经站在被堵格里，
  它的每一次推力都被守卫拒绝 → 永久冻在原地。修法和 A\* 的"起点格豁免"同源：
  当前就在堵格里时，目的地一律放行。正常玩法不会出现单位在墙里，
  但"建筑盖在单位身上""地形变动"之类的将来会 —— **这条是集成探针抓出来的，
  headless 测试结构上测不到**（headless 里我自己摆位置，不会摆到墙里）
- **对称堆叠下"先累加再统一应用"（Jacobi）会明显变慢**：完全对称时，中间单位
  受到上下邻居的推力大小相等、方向相反，累加后精确抵消，整列只能从两端往里解。
  实测（34 个单位叠成一列、间距 3、空地、每 20 帧采样重叠对数）：
  Jacobi 第 **360** 帧归零，就地应用（Gauss-Seidel）第 **180** 帧归零。
  选了就地应用；代价是结果与遍历顺序有关（先被扫到的单位占一点点便宜），视觉上无所谓
- **死区（slop）是为了"定点"而不是省算力**：穿透小于 `Rts_Collision_Slop` 就不推。
  少了它，浮点上的推离永远差最后一点点，会每帧微推、永远抖 ——
  有了它，稳定后位置**逐位不变**（可以直接断言）

### 诊断手法（比截图可靠）

排查"画面不对"时，**不要靠截图**（GL 内容常常截不到，还会误抓桌面）：

1. **帧缓冲读回**：在 `Frame_End` 里、`swap_buffers` 之前用 `glReadPixels` 读回，
   扫描出**所有非清屏像素的包围盒**并打日志。一眼就能区分"没清屏 / 没画 / 画错位置"。
   （需要临时在引擎侧加，因为 `glad` 对游戏不可见）
2. **数据侧探针**：在游戏系统里自己遍历 transform+sprite，把 sprite 系统*应该*看到的数据
   （world 平移、size、layer、texture id）打出来，与相机状态对照
3. **注意探针读的时机**：`lum_renderer2d_get_draw_call_count()` 在 `begin_frame` 就被清零，
   在 `Screen` 阶段读它**必然是 0** —— 我为此白查了一轮。要读它得放在 `Frame_End` 之后
4. **判定"位置/朝向"要用逐行直方图，不要用"上下半区各数一半"**：
   分界线只要切偏一行，结论就可能完全反过来（我据此把已经正立的字形误判成颠倒，
   白折腾一轮）。正确做法是把目标区域**逐行**的像素数打出来，
   一眼就能看出质量分布在哪、哪一行最宽。
   实例：画 `LLLLLLLLLL`，正立时"横脚"那一行最宽且位于字形带**最下方**
5. **要证明"是我改的这一行导致的"，用同一二进制做 A/B，不要编两遍**：
   把可疑行为挂到一个**环境变量开关**上（`getenv` 读一次存 `static`），
   一次编译、跑两次、只换环境变量。这样排除"两次编译的其它差异"，
   也让"改前/改后"的对比无歧义。实测用 `LUM__ANCHOR=lb` 复现旧的左下角锚点
6. **让读回像素可归属**：全屏读回只给一堆非清屏像素，分不清是谁画的。
   做法是临时加一个**孤立探针实体**：唯一颜色（如品红）+ 位于无遮挡处
   + 只让它进入待测状态（如唯一被选中）→ 就能按**颜色分类**求包围盒。
   再让游戏侧每帧报一次"这个实体**应该**画在哪"（`lum_app_world_to_screen`），
   两条日志一减就是误差。实测：修复后 sprite/角标中心与期望位置差 0.5 px，
   旧行为差 12 px（= `size/2`），判据干净利落
7. **报"应该画在哪"时要连同相机状态一起打**：窗口一开，鼠标可能正在边缘触发
   边缘滚动，相机自己就飘了（实测两次运行 `camera.y` 分别是 −27 和 −3.6）。
   只打期望坐标而不打相机，就会把相机漂移误判成绘制偏移
8. **要量化一个外部大仓库时不要整仓 clone**：
   `git clone --depth 1 --filter=blob:none --sparse <url> <dir>` 然后
   `git sparse-checkout set <子目录>`，只落需要的目录。RecoilEngine 整仓很大，
   但我只取 `rts/ExternalAI/Interface` 就量出了它的 AI 接口规模。
   注意 `git sparse-checkout set` **没有 `--quiet`**（加了报 unknown option）
9. **headless 测不到"系统到底有没有跑"**：`rts_check` 是直接调逻辑函数的，
   所以即使我把系统注册到了错误的阶段（或根本没注册成功），它也照样全绿。
   补法是加**临时集成探针**：在 `Start` 里用代码代替鼠标下一道指令，
   再在 `Screen` 里每 30 帧打一次 `active_count` + 某个单位的位置，跑几秒看日志。
   实测一次就同时证实了：Pre 确实在真实帧循环里跑、`ctx->dt` 是真帧间隔、
   位移/时间正好等于 `speed`、编队终点精确等于公式算出的格点、到点后逐位不变。
   **凡是"注册/接线"类改动，都必须补这一发**，不能只靠 headless 全绿
10. **窗口化探针不是密闭环境：真实鼠标会往里面塞输入**。我跑寻路探针时看到
   单位停在 (464,48) 而不是预期的 (400,16)，一度以为夹取算错了 ——
   实际是**用户桌面的真鼠标点了一下窗口**，多下了一道指令，而探针每帧读的是
   "组件池里第一个单位"，那一刻它已经不是我以为的那个单位了。
   证据是同一份日志里 `moving` 从 1 跳到 18 再跳到 34（恰好是玩家 18 个、全体 34 个）。
   对付办法：① 探针要连**实体句柄 + 目标点**一起打，才能判断"读的是不是同一个单位"；
   ② 真正的断言放 headless 用例（`rts_check` 里那个用例 392 步逐点采样、
   终点逐位相等，才是可信的），窗口探针只用来证明"接线通了"
11. **"卡住不动"要先排除"被环境卡住"**：我量到"重叠对数卡在 33 上 300 帧不动"，
   一度判定是对称死锁；实际那次堆叠**紧挨着一个障碍物**，"对称抵消"和"撞墙推不动"
   两个原因混在了一起。换到空地、并用**同一二进制的 env 开关**重做 A/B，
   才量到真值（Jacobi 360 帧 / Gauss-Seidel 180 帧）。
   教训：**探针的位置与场景本身就是潜在的混杂因素**，下结论前先把环境因素清干净
12. **删代码块时最容易多留一个 `}`**（我在本次会话里犯了 **5 次**，全是同一模式：
   old_string 不含块尾的函数闭括号、new_string 里却补了一个）。
   做法：删块时把**块 + 紧随其后的锚点**一起写进 old_string；
   或者删完**立刻 read 一眼衔接处**再构建。构建报
   `expected identifier or '(' before '}' token` 就是它。
   **派生教训（更贵）**：
   - **别用 PowerShell 的 `.Replace()` 改源码**：文件是 CRLF 而我写的模式是 LF，
     于是**静默不匹配**、改动没生效，只剩最后一条单行替换生效 → 引入未声明变量。
     源码编辑一律用 `edit` 工具（它自己处理行尾）。
   - **绝对不要按字节索引给源码做手术**：我用 `IndexOf` 切片想删一段探针，
     结果把中间所有流程函数都剪掉了（探针在流程代码之前）。真剪坏了就
     `git checkout -- <file>` 拿回 HEAD —— 前提是这个文件本轮本来不该有别的改动，
     所以**改之前先想清楚"这个文件本轮该不该动"**。
13. **"某些绘制不出现"要用正交实验拆变量，不要逐个假设去改**：demo 里网格线和两张表的
   边框全部不渲染，我先怀疑 layer，改了四轮都没用。**一次做对的实验是**：
   在 layer 0..8 上各画一条【同样的竖线】（位置分开、颜色各异），结果 9 个 layer
   全部正常 → layer 立刻被排除，矛头指向唯一的另一个变量（粗细），随后一眼看出是
   亚像素问题。**教训：当"一部分正常、一部分不正常"，先造一个只让一个变量变化的对照，
   而不是凭直觉挑一个变量去改。**
14. **帧内读像素：注册位置与"先手动 flush"两个坑**。① `Screen` 阶段在 `Post_Process`
   **之后**（`… Scene → Scene_Overlay → Post_Process → Screen → Frame_End`），
   把读回系统注册在 `Post_Process` 会读到**刚被清空的缓冲**（我拿到满屏 `(0,0,0,0)`，
   一度以为"什么都没画"）。可注册阶段里 `Screen` 是最后一站，而 `Frame_End` 是引擎独占、
   注册会被静默拒绝。② 渲染器是**延迟提交**：真正的 `glDrawElements` 只发生在
   `lum_renderer2d_end_frame()` 里，所以读之前必须**自己先调一次** `end_frame()`
   （`Frame_End` 随后会再刷一次，此时命令已排好序，画面不受影响）。
   ③ 同阶段内系统按**注册顺序**执行（`lum__run_stage_systems` 只是顺序 for，不看 `order`），
   读回系统要**紧跟在绘制系统之后注册**。做法：把探针做成临时 target，
   `target_include_directories(... PRIVATE third_party)` 就能 `#include <glad/glad.h>`，
   **不必改引擎侧代码**；探针想复用游戏里的 `static` 系统/常量，
   可以在自己的 .c 里 `#define main xxx_unused` 再 `#include "a_star_demo.c"`。
15. **"同层谁在上"的最小对照：两条覆盖全屏的纯色矩形**。临时 target 里先记录 A（VP1）
   再记录 B（VP2），读屏幕中心 1 个像素：读到 A 说明**记录顺序不生效、状态字节生效**，
   读到 B 说明顺序正常。这一发把"背板压住文字"从猜测变成结论
   （同层不同 VP → 先记录的在上；同层同 VP → 后记录的在上）。
16. **"画了却看不见"要按【自变量阶梯】量，不要逐个猜原因**。格内 220 个数字全部
   `draw` 成功、`labels_drawn=220`，但整屏扫不到墨迹。我先猜 layer、再猜颜色阈值、
   再猜字体加载失败（用 `lum_tr_draw_layer` 补画洋红/青色对照组来分辨"字体问题"
   还是"坐标问题"），都没中；**一次做对的实验**是把同一串字按
   6/7/8/10/12/15/20 字号各画一行、逐行量墨迹峰值覆盖 —— 曲线一出来就定性了
   （7 px → 0.00，15 px → 0.85）。**教训同第 13 条：先造"只让一个变量变"的阶梯对照。**
   附带教训：**判墨迹别按标称颜色卡阈值**。字形核心覆盖率本身就不是 1.0，
   墨迹是"字色 × 覆盖 + 底色 × (1−覆盖)"，按色相（冷/暖相对底色反转）判才稳；
   我第一版按标称 RGB±18 卡，明明在渲染却报 0 像素。
17. **"渲染坏了"日志和自检都看不见，要读两个数**。白屏事故（帧边界系统没注册上）的
   stderr **一行都没有**（实测：破坏后 stderr 0 行、无任何报错），而自检那几行照打 ——
   **烟测不能只看日志**。两个直接读数：
   ① `lum_app_system_count(Lum_Stage_Frame_Begin) / (Frame_End) >= 1`：直接验"注册上了没"，
      这才是白屏的根因（实测破坏后是 `0/0`）；
   ② 帧缓冲里"非清屏像素数 + 包围盒"：画一个 800×600 的不透明矩形，正常应当是
      `lit_px=480000`、bbox 正好 800×600（破坏后 `lit_px=0`）。
   踩过的两个坑：**忘了手动 `lum_renderer2d_end_frame()` 就去读** → 永远 0
   （渲染器延迟提交，真正的 draw 在 Frame_End 里）；**想把读回放到"引擎自己刷完之后"**
   而去用 `Lum_Stage_Stop` → 探针**挂住不退出**，因为 `Stop` **不是每帧阶段**，
   它只在退出时跑一次（每帧的只有 Frame_Begin..Frame_End，而 Frame_End 是引擎独占）。
18. **工作区里可能混进"陈旧副本"：发现文件被改了就按内容证据判断，别猜**。
   本次会话中途 `src/lum_application.c` 被某个外部动作换成了**旧版本**（丢掉白屏修复、
   还把 `kStageName` 改名成 `g_stage_name`），mtime 落在我一段构建/清理命令之间。
   `git status` 才让它现形；`git diff` 一眼就能判方向（`-` 是 HEAD、`+` 是工作区），
   确认工作区是**退化的**之后就 `git checkout --` 恢复，然后重跑门禁 + 上面那个
   帧缓冲探针（因为自检/日志看不见渲染回归）。**教训：`git status` 是每轮收尾的必查项，
   而且"我改过的文件"和"工作区里变了的文件"必须能对上号。**
   **同类事件已发生第二次（本次是 `glimmer_hello3d.c`）**：它的帧上限被从循环条件里删掉
   （`while (lum_window_is_open() && frame < HELLO3D_FRAMES)` → `while (lum_window_is_open())`），
   而文件头的注释与自己写的"跑满 240 帧自动退出"仍然在、启动日志还照打 "240 frames" ——
   **同一个文件里自相矛盾**。代价是冒烟那一发从 1 秒变成"永远不退出"，我一开始还误判成性能问题
   （写了分段计时探针才发现每帧只有 4.16 ms，是循环没有上限）。三条可复用的判据：
   ① **冒烟必须既数 stderr 又断言"进程自行退出"**（只数 Warn/Error 会漏掉这类改动）；
   ② **新文件是未跟踪的，`git status` 帮助有限** —— 所以要把不变量做得便宜可复验
      （帧数、退出耗时、断言条数），跑一次就能看出退化；
   ③ **"跑得比预期久"要当成被改动的信号，而不是先怀疑性能**（性能问题先量，别先猜）。

### 施工纪律

1. 发现**引擎层面的新陷阱/新约定** → **先补本 skill**，再继续写游戏代码
2. 里程碑完成 → 更新上面的表 + 把"临时绕法"升级成引擎能力或写进 `docs/CONTRACT.md`
3. 每次改完跑：
   - 全量构建（0 warning）
   - 严格警告集：引擎 `-Iinclude -Ithird_party`、游戏**只给 `-Iinclude`**（证明纯消费者）
   - 公共头自包含 35/35；**游戏头也要自包含**（`-Iinclude -Igames/rts`，逐个头单独编）
   - `Luminous_rts_check` 全过
   - 改到"注册/接线"时**再加临时集成探针**（见诊断手法 9）
   - 三个可执行程序冒烟，stderr 必须为空
     （`Luminous_astar_demo` 会打两行 Info 自检，那是预期；只数 `Warn`/`Error`）

### 已修的引擎侧问题（本项目推进中发现）

- `collision_demo.c` 的 HUD 曾整段画在画面外（多套了一层 `screen_to_world`）—— 已修，
  详见 §4 的 "Screen 阶段的坐标已经是像素"
- **sprite 绘制锚点与拾取/框选不一致**（差半个 `size`）—— 已修：`lum__sys_sprite` 改用
  `origin = size/2`，并把"Sprite 以实体局部原点为中心"写进 `include/lum_scene.h`、
  `docs/CONTRACT.md` §8、`docs/modules/{scene,glimmer}.md`
- **glimmer 从"渲染器模块"升级为"基础平台"**（数学 + 平台 + 渲染器；`include/glimmer/` +
  `src/glimmer/` + 独立构建目标）。两次重组的经验：
  ① **链接器才是边界的第一发现者**。第一次隔离渲染器时它报出
     `libglimmer.a` 的 `lum_shader.c` / `lum_text_renderer.c` 引用了引擎的 `lum_fs_read*`
     → 于是有了 `luminous_base`；第二次把数学与平台并入 glimmer 时它又立刻暴露出
     `luminous_base` 本就该在里面（`glimmer` 自己就是那块地基）。
     **靠"把文件挪个目录"永远发现不了这类隐藏依赖。**
  ② **"能独立运行"必须可执行地证明**，不能只写在文档里：`glimmer_hello.c` 只链接
     `libglimmer.a`（把 `build/libLuminous.a` 改名藏起来照样链得出来），自己开窗口、
     自己跑帧循环、自己画；`tools/check-glimmer-boundary.ps1` 的规则 3 直接检查它的
     链接行里没有 `Luminous`。它同时是"往里加东西有没有污染平台"的活判据。
  ③ **平台边界的三条规则都用 A/B 验证过会真的报错**（故意插入违规包含 / 往 link.txt 塞
     `Luminous` → 退出码 1），另加一道"`include/glimmer/*.h` 只给 `-Iinclude` 也要编得过"
     的门，证明公共头没把 glad/stb/GLFW 泄漏给消费者。
  ④ **平台规模**：那次重组后 glimmer 3315 行实现（约占全库 40%）+ 934 行头，引擎高层 4822 + 1288 行头；M1 与相机合并之后平台为 **3776 + 1129**（见 architecture.md §7）。
     引擎的 Init 前 5 级（log/window/input/time/renderer）全部落在 glimmer，
     `glimmer_hello` 手写的启动顺序与它们逐条一致。

### 3D（M1）的两条经验

- **状态归属要写清楚，不然开关会静默失效**：`lum_renderer3d_begin_frame()` 最初每帧都把深度
  状态恢复成默认，于是调用方刚设的 `set_depth_test(false)` 被覆盖 —— 探针读回时**深度开/关两次
  数据完全相同**，开关等于不存在。结论：**"模式"（深度、混合、剔除）帧初不重置，"每帧统计"才重置**；
  `init` 设默认值，之后保持到有人改它。只跑不读回的话，这个 bug 会一直躺在库里。
- **失败先怀疑判据，再怀疑代码**：`glimmer_check` 第一版有一条假失败 ——
  我用 `|fwd.y| < 0.999` 断言"轨道相机夹取到极点之内"，但夹取到 `π/2 - 1e-3` 时
  `|fwd.y| ≈ 0.9999995`，本来就大于阈值。改成精确判据（`position.y ≈ d·sin(π/2 − 1e-3)`，
  外加"严格 < 1"的不等式）后才是有意义的验证。**阈值型断言要先算一遍它在边界上的取值。**

19. **量性能之前，先确认指标没坏 —— 三条已经踩过的坑**
    - **`clock()` 在 Windows 上量不出亚毫秒**：`CLOCKS_PER_SEC = 1000` 且按调度器时基跳动，
      粒度约 15 ms。用它量"一次 draw call 的开销"，会把 1000 个实例化量成 3.9 ms
      （比 10000 个还慢，明显荒谬）。→ 用 `timespec_get` 的**墙钟**，
      并**同时打印中位数与均值**（一次性尖峰会自己现形）。
    - **垂直同步会把帧钉在刷新率上**：本机 240 Hz = 4.167 ms，于是"提交耗时"里混进了 present
      的等待，而等待落在哪一组纯属偶然。→ 测量前 `lum_window_set_vsync(false)`。
    - **进程刚起来时 CPU 还在升频、驱动也在初始化**：单组内预热 60 帧仍不够，第一组系统性偏慢
      （0.178 ms vs 稳态 0.009 ms）。→ **整轮预热一遍再丢弃**。
    三条都属于同一类：**指标错了、代码没错**。所以性能结论必须能自洽解释（慢的那个为什么该慢），
    解释不通就先怀疑度量。另外：**"只数 draw call"不足以证明画对了** ——
    16 个实例全叠在同一个地方也是一次 draw call，必须同时校验位置（用相机矩阵预测像素再采样）。

20. **sRGB 标志只能标颜色贴图，数据贴图标了就出错**（M3 引入 `Lum_Texture_Flag_Srgb` 时立的规矩）
    - 该标：albedo、自发光 —— 它们是"颜色"，需要硬件在采样时做 sRGB→线性。
    - **绝不能标**：法线贴图、粗糙度/金属度、遮罩、AO。它们是**数值**不是颜色，标了会被当成颜色
      错误转换。症状是"光照方向莫名其妙"、法线贴图效果诡异 —— 极难反查。
    - 另一半要知道：标了 sRGB 之后画面会**变暗约 57%**（实测 83/255 → 36/255）。这不是 bug，
      而是链路只做了一半 —— 采样线性化了、光照在线性空间算了，但**输出还没编码回 sRGB**
      （或渲染到 sRGB 帧缓冲）。那一半属于 M7。要判断"是不是这个原因"：临时去掉 sRGB 标志，
      画面变亮即确诊。
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

- 函数 `lum_snake_case`；类型 / 枚举 / **宏** `Lum_Pascal_Snake`
  （宏一律 `Lum_` 前缀 + 逐词首字母大写，如 `Lum_Ecs_Max_Entities`、`Lum_Ui_Popup_Layer`）
- 全库仅两个例外：`containers/lum_list.h` 的**小写**泛型宏（`lum_list_each` 等，
  对齐函数命名），以及第三方要求的 `STB_IMAGE_IMPLEMENTATION`
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
| **`lum_scene_pick` 依赖 world 矩阵** | 未跑过 `Transform` 阶段 → 用的是上一帧矩阵，命中区域偏移 |
| **无线程安全** | 全库 0 个线程原语，遍地全局单例（log/input/time/window/ui/assets/effects/renderer2d/注册表/`lum_default_theme`）。跨线程即 UB，无诊断 |
| **`Lum_Debug` 改变公开面** | `Lum_Assert` 变 no-op、`lum_vec2_print` 系列消失、`Lum_Log_Trace/Debug` 变 no-op → 同版本号可有不同 API 面 |
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

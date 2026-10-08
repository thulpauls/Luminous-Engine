# 版本与编号

## 当前版本

**引擎与 glimmer 平台分开计数，各有各的版本线**：

| 版本线 | 唯一来源 | 当前版本 |
|---|---|---|
| **引擎 Luminous** | [`include/lum_version.h`](../include/lum_version.h) | **prototype 1** |
| **glimmer 平台** | [`include/glimmer/glimmer_version.h`](../include/glimmer/glimmer_version.h) | **prototype 2** |

引擎的版本头里额外声明**它针对哪个 glimmer 版本开发**（当前：`prototype 2`），
两侧必须成对修改，由两道门强制（见下方"兼容性矩阵"）。

**为什么分开计数**：两者是独立的东西。引擎可以升到 alpha 3 而 glimmer 仍停在
prototype 7，反过来也一样 —— 把两条线绑成一个号，就没法表达"平台没动、引擎动了"
这种最常见的状态。代价是必须**显式声明兼容关系**，这份声明就写在引擎的版本头里。

---

## 格式：`<开发阶段> <迭代次数>`

```
prototype 1
^^^^^^^^^ ^
  阶段词   本阶段内的第几次迭代
```

**不是 SemVer**：没有 major / minor / patch 三段。整个项目只有"阶段 + 迭代次数"两段，
阶段表达成熟度，迭代次数表达"在这个成熟度上走了多少步"。

### 阶段词表

阶段是**有序数值**（不是纯标签），这样 `#if` 才能表达"至少 beta"这类判断：

| 阶段 | 数值 | 含义 | 兼容承诺 |
|---|---:|---|---|
| `prototype` | 1 | 原型：接口随时可变，只求跑通 | **无**（当前所处阶段） |
| `alpha` | 2 | 功能大致齐备，接口趋于稳定 | ❌ 未定 |
| `beta` | 3 | 冻结接口，只修 bug | ❌ 未定 |
| `rc` | 4 | 发布候选 | ❌ 未定 |
| `release` | 5 | 正式 | ❌ 未定 |

> 旧方案把阶段当**纯字符串标签**、明确声明"不参与比较"，于是 `#if` 无法表达
> "至少 beta" —— 想判断成熟度只能靠人读文档。现在阶段有数值，这个问题没有了，
> 代价是"阶段数值表"和"阶段名串"必须同步改（两个版本头里的注释都写了这一点）。

### 迭代次数的规则

- **本阶段内每迭代一次 +1**：`prototype 1` → `prototype 2` → …
- **阶段推进时从 1 重新开始**：`prototype 9` → `alpha 1`（不是 `alpha 10`）
- 因此**跨阶段比较迭代次数没有意义**：`prototype 9` 与 `alpha 1` 不可比。
  比较必须带上阶段 —— `Encode` 把阶段编码在高位，天然满足这一点

### 编码与比较

```c
#define Lum_Version_Encode(stage, iteration) ((stage) * 256 + (iteration))
#define Lum_Version Lum_Version_Encode(Lum_Version_Stage_Current, Lum_Version_Iteration)

#if Lum_Version < Lum_Version_Encode(Lum_Version_Stage_Beta, 1)
  #error "本项目已进入 beta 之前的阶段不再支持"
#endif
```

阶段占高 8 位、迭代次数占低 8 位（各上限 255）。`Encode` 刻意**只用整数乘加、
不做类型转换** —— `#if` 的预处理器不认 `(unsigned long)` 这类多词转型
（GCC 报 `missing binary operator before token 'long'`）。

glimmer 侧同名宏是 `Lum_Glimmer_Version_Encode` / `Lum_Glimmer_Version`，规则一致。

---

## 兼容性矩阵（本文件的重点）

**当前引擎版本 `prototype 1` 所兼容的 glimmer 版本：**

| 引擎版本 | 兼容的 glimmer 版本 | 判定规则 |
|---|---|---|
| **prototype 1** | **prototype 2** | **阶段相同 且 迭代次数相同**（严格相等） |

为什么 prototype 阶段要求严格相等、而不是"大于等于"：这个阶段**没有任何向前兼容承诺**，
"针对哪个版本开发"就是全部事实 —— 宽松的判据只会掩盖真实的漂移。
将来进入 `alpha` / `beta` 后，判据会改成"引擎要求的最低 glimmer 版本"
（那时 `#if` 的阶段数值比较就有了真正的顺序语义）。

### 三道门（声明与事实不一致时，任何一道都能拦住）

| # | 位置 | 机制 | 报错时机 |
|---|---|---|---|
| ① | [`include/lum_version.h`](../include/lum_version.h) 末尾 | `#if Lum_Version_Glimmer_Iteration != Lum_Glimmer_Version_Iteration` → `#error` | **编译期**（每个包含它的编译单元） |
| ② | [`CMakeLists.txt`](../CMakeLists.txt) 版本解析段 | 解析两个头文件后比对，不一致直接 `FATAL_ERROR` | **配置期**（最早，信息最全） |
| ③ | 本文件的兼容性矩阵 + `glimmer_check` 的版本断言 | 人读的真相 + 对 glimmer 自身版本形态的自动检查 | 评审 / 回归检查 |

**实测证据**（A/B，把 glimmer 的迭代次数临时改成 2）：

```
配置期：CMake Error at CMakeLists.txt:66 (message):
        引擎声明兼容 glimmer prototype 1，但 include/glimmer/glimmer_version.h 里是 prototype 2。
编译期：include/lum_version.h:75:2: error: #error "引擎声明的 glimmer 迭代次数与
        include/glimmer/glimmer_version.h 不一致：两者必须成对修改"
```

两道门都真的会拦（不是"写了但从不触发"的装饰）。

---

## 唯一来源与 CMake 对接

版本号只有两个定义处（就是上面那两个头文件）。CMake **在 `project()` 之前**读取它们，
据此填 `project(VERSION)` 并做配置期兼容检查 —— **不反向生成头文件**，
所以构建脚本与公共头不可能各写一份而悄悄跑偏。

```cmake
file(READ ".../include/lum_version.h"              LUM_VERSION_HEADER)
file(READ ".../include/glimmer/glimmer_version.h"  LUM_GLIMMER_VERSION_HEADER)
# 解析：引擎的迭代次数/阶段名/它要求的 glimmer 版本；glimmer 自身的阶段与迭代次数
project(Luminous VERSION ${LUM_VERSION_ITERATION})   # CMake 只吃数字，阶段词单独用于消息
```

配置输出（实测）：

```
-- Luminous version: prototype 1  (针对 glimmer prototype 2)
-- glimmer version:  prototype 2
```

几个约定：

- CMake 的 `project(VERSION)` **只接受数字**，塞不进阶段词，所以喂进去的是**迭代次数**
  （`PROJECT_VERSION` = `"1"`）。阶段词单独取出用于消息与 tag
- **命名分属两套约定**：C 宏是 `Lum_Version_Iteration` / `Lum_Glimmer_Version_Iteration`，
  CMake 变量是 `LUM_VERSION_ITERATION` / `LUM_GLIMMER_ITERATION`
  （与 `LUM_C_COMPILER`、`LUM_ENGINE_SOURCES` 一致）。不要互相套用
- glimmer 的阶段号在头文件里是**符号**（`Lum_Glimmer_Version_Stage_Prototype`），
  CMake 先取符号名、再解析它的数值 —— 头文件必须是机器可解析的，这条一直成立

---

## 四个版本轴（只有第一条已落地）

| 轴 | 内容 | 现状 |
|---|---|---|
| ① 引擎 / 平台版本 | 本文件（两条版本线 + 兼容矩阵） | ✅ 已落地 |
| ② ABI 版本 | 共享库的 soname / 结构体布局兼容 | ❌ 未做，**也未预留**（当前是静态库，无 soname；头文件里没有 ABI 宏） |
| ③ 存档格式版本 | `scene_io` 落盘格式 | ⚠ 存在但**不可见**：`Lum_Scene_Io_Version = 1` 定义在 `src/lum_scene_io.c:12`；兼容门只比 major，实测接受 0–99 |
| ④ 外部依赖版本 | 见下方依赖矩阵 | 需在 README 声明 |

轴 ② 的关键点：ABI 版本必须能**独立于** API 版本前进，
否则"没改 API 但改了结构体布局"这种情况无法表达。**永远不要把轴 ③ 绑到轴 ① 上** ——
引擎 `prototype 5` 的存档格式可能是 3，引擎 `alpha 1` 的格式可能还是 3。

> ⚠ 现在有**两条**轴 ①（引擎 + glimmer）：如果将来 glimmer 真的作为独立库发布，
> 它自己也需要 ABI 轴（②）与依赖声明（④）。目前两者同仓库同构建，暂不分开处理。

---

## 依赖矩阵（轴 ④）

| 组件 | 要求 | 证据 |
|---|---|---|
| OpenGL | **3.3 core profile** | `src/glimmer/lum_window.c` 的 `glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR/MINOR, 3)` + `GLFW_OPENGL_CORE_PROFILE` |
| GLSL | **`#version 330 core`** | 写死在 `src/glimmer/lum_renderer2d.c`、`src/glimmer/lum_renderer3d.c`、`src/effects/lum_beam.c`、`src/effects/lum_glow.c` 的着色器源里 |
| GLFW | 3.4（仓库内 vendored） | `glfw-3.4/`；`third_party/GLFW/` 是其头文件的**重复副本**（逐字节相同） |
| glad | 随仓库生成版 | `third_party/glad/glad.h` + 根 `glad.c` |
| stb_image / stb_truetype | vendored | `third_party/`（`stb_image_write.h` 当前**零引用**） |
| C 标准 | **未固定** | `CMakeLists.txt` 没有 `CMAKE_C_STANDARD`；头文件用了 C11 的同名 typedef（`lum_scene.h`） |
| 工具链 | GCC / MinGW-w64 | `cmake -B build -G "MinGW Makefiles"` —— 生成器必须显式指定 |
| 平台 | Windows 实测 | GLFW 本身跨平台，但引擎未在其它平台验证 |

---

## git tag 约定

- **注解 tag**（`git tag -a`），两条版本线各自打标，名字形如
  **`engine-<阶段>-<迭代>`** 与 **`glimmer-<阶段>-<迭代>`**
- 显示串用空格（`prototype 1`），**tag 用连字符**（`engine-prototype-1`）——
  空格在 tag 名、文件名、`.pc` 的 `Version:` 字段里都很难受
- 当前应为：`engine-prototype-1`（引擎尚未迭代）、`glimmer-prototype-2`（已打）
- 重打 tag 只在**未推送**时进行；已 push 的 tag 一律不动（改写它等于骗人）

---

## 已定 / 未定

| 项目 | 状态 |
|---|---|
| 两条版本线分开计数 | ✅ 已定 |
| 格式 `<阶段> <迭代次数>` | ✅ 已定 |
| 阶段数值表 + 阶段名串（有序、可比较） | ✅ 已定 |
| 迭代次数在本阶段内递增、阶段推进时重置为 1 | ✅ 已定 |
| 编码方式与 `#if` 比较 | ✅ 已定 |
| 引擎声明所兼容的 glimmer 版本 | ✅ 已定（当前严格相等） |
| 三个头文件为唯一来源、CMake 只读 + 配置期校验 | ✅ 已定 |
| **什么算"一次迭代"（何时 +1）** | ❌ **未定**（暂按"一批相关变更 + 全门禁通过"算一次） |
| **何时该推进阶段** | ❌ **未定** |
| **各阶段的兼容性承诺**（进入 alpha 后判据怎么放宽） | ❌ **未定** |
| 存档格式版本的编号与兼容策略 | ❌ 待冻结时处理（现状见 `CONTRACT.md` §11） |

---

## 冻结时要补的

1. **迭代与阶段推进规则**：什么算一次迭代、推进阶段的判据（这两条一定，编号就不再靠感觉）
2. **各阶段的兼容性承诺**：`prototype` / `alpha` / `beta` / `rc` / `release` 各自允许什么变化；
   `alpha` 之后把"严格相等"放宽成"引擎要求的最低 glimmer 版本"
3. **存档格式版本**：把 `Lum_Scene_Io_Version` 对消费者公开，并写清兼容策略
   （当前 `1u` 使兼容门接受 0–99，见 `CONTRACT.md` §11）
4. **ABI 版本轴**：若将来出共享库（glimmer 独立发布是这条路的第一步），
   预留 `Lum_Abi_Version` 与 soname 方案
5. **固化 CMake 生成器**（`CMakePresets.json`），避免删 `build/` 后回落到 NMake

另见 [CONTRACT.md](CONTRACT.md) §7：`Lum_Debug` 会改变公开面，
**在这条解决之前，版本号无法可靠地标识 API 面**。

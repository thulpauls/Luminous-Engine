# 版本与编号

## 当前版本

```
prototype 0.1
^^^^^^^^^   开发阶段标记
          ^ 主要版本号
            ^ patch 版本号
```

三段式：**`<开发阶段> <主要版本号>.<patch 版本号>`**。注意**没有 minor**。

## 唯一来源

版本号只有一处定义：`include/lum_version.h`。
CMake **在 `project()` 之前读取它**，再据以填 `project(VERSION)` ——
不反向生成头文件，所以构建脚本与公共头不可能各写一份而漂移。

| 宏 | 当前值 | 含义 |
|---|---|---|
| `Lum_Version_Stage` | `"prototype"` | 开发阶段标记（**纯标签**） |
| `Lum_Version_Major` | `0` | 主要版本号（代际） |
| `Lum_Version_Patch` | `1` | patch 版本号（代际内迭代次数） |
| `Lum_Version` | `1` | 两段数字的编码值 = `Major*256 + Patch` |
| `Lum_Version_String` | `"prototype 0.1"` | 给人看的显示串 |

```c
/* 编译期比较：只比数字 */
#if Lum_Version < Lum_Version_Encode(1, 0)
  #error "需要 Luminous 1.0 或更高"
#endif
```

`Lum_Version_Encode(major, patch)` 刻意**只用整数乘加、不做任何类型转换** ——
`#if` 的预处理器不认 `(unsigned long)` 这类多词类型转型（GCC 报
`missing binary operator before token 'long'`）。每段上限 255（8 位）。

**阶段词不参与比较**：它只是成熟度标签，不进 `Lum_Version`，也无法用于 `#if` 的大小判断。
这是刻意定下的规则，不是遗漏。

---

## 已定 / 未定

| 项目 | 状态 |
|---|---|
| 三段结构与显示格式 | ✅ 已定 |
| 阶段词表（`prototype` / `alpha` / `beta` / `rc` / `release`） | 文档约定，**代码不校验** |
| 阶段不参与比较 | ✅ 已定 |
| 两段数字的编码方式与比较 | ✅ 已定 |
| 头文件为唯一来源、CMake 只读 | ✅ 已定 |
| **何时该动哪一段** | ❌ **未定** |
| **各阶段承诺什么兼容性** | ❌ **未定** |
| 是否补回 minor | ❌ 待冻结时决定 |
| 存档格式版本的编号与兼容策略 | ❌ 待冻结时处理（现状见 `CONTRACT.md` §11） |

---

## 已知取舍：没有 minor

两段数字里只有一段可动，因此：

- **"加了功能但没破坏兼容"（SemVer 的 minor）**
- **"只修了 bug"（patch）**

**两件事无法区分**，都表现为 patch +1。

在 prototype 阶段完全无所谓。但如果冻结成正式版后仍想区分这两类变更，
**加回 minor 是破坏性变更**（改了版本号的段数）。现在的取舍是先不补。

---

## 四个版本轴（只有第一条已落地）

| 轴 | 内容 | 现状 |
|---|---|---|
| ① 引擎 / API 版本 | 本文件 | ✅ 已落地 |
| ② ABI 版本 | 共享库的 soname / 结构体布局兼容 | ❌ 未做，**也未预留**（当前是静态库，无 soname；头文件里没有 ABI 宏） |
| ③ 存档格式版本 | `scene_io` 落盘格式 | ⚠ 存在但**不可见**：`Lum_Scene_Io_Version = 1` 定义在 `src/lum_scene_io.c:12`；兼容门只比 major，实测接受 0–99 |
| ④ 外部依赖版本 | 见下方依赖矩阵 | 需在 README 声明 |

轴 ② 的关键点：ABI 版本必须能**独立于** API 版本前进，
否则"没改 API 但改了结构体布局"这种情况无法表达。**永远不要把轴 ③ 绑到轴 ① 上** ——
引擎 1.4.0 的存档格式可能是 3，引擎 2.0.0 的格式可能还是 3。

---

## 依赖矩阵（轴 ④）

| 组件 | 要求 | 证据 |
|---|---|---|
| OpenGL | **3.3 core profile** | `src/lum_window.c` 的 `glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR/MINOR, 3)` + `GLFW_OPENGL_CORE_PROFILE` |
| GLSL | **`#version 330 core`** | 写死在 `lum_renderer2d.c`、`effects/lum_beam.c`、`effects/lum_glow.c` 的着色器源里 |
| GLFW | 3.4（仓库内 vendored） | `glfw-3.4/`；`third_party/GLFW/` 是其头文件的**重复副本**（逐字节相同） |
| glad | 随仓库生成版 | `third_party/glad/glad.h` + 根 `glad.c` |
| stb_image / stb_truetype | vendored | `third_party/`（`stb_image_write.h` 当前**零引用**） |
| C 标准 | **未固定** | `CMakeLists.txt` 没有 `CMAKE_C_STANDARD`；头文件用了 C11 的同名 typedef（`lum_scene.h`） |
| 工具链 | GCC / MinGW-w64 | `cmake -B build -G "MinGW Makefiles"` —— 生成器必须显式指定 |
| 平台 | Windows 实测 | GLFW 本身跨平台，但引擎未在其它平台验证 |

---

## git tag 约定

- **注解 tag**（`git tag -a`），名字形如 **`<阶段>-<主要>.<patch>`**
- 显示串用空格（`prototype 0.1`），**tag 用连字符**（`prototype-0.1`）——
  空格在 tag 名、文件名、`.pc` 的 `Version:` 字段里都很难受
- 当前：`prototype-0.1`
- 重打 tag 只在**未推送**时进行；已 push 的 tag 一律不动（改写它等于骗人）

---

## CMake 侧

```cmake
# 在 project() 之前解析
file(READ ".../include/lum_version.h" LUM_VERSION_HEADER)
# 逐段正则提取，提取不到就 FATAL_ERROR
project(Luminous VERSION ${LUM_VERSION_MAJOR}.${LUM_VERSION_PATCH})
```

- CMake 的 `project(VERSION)` **只接受数字**，塞不进阶段词。所以数字部分以
  `主要.patch` 两段喂进去（`PROJECT_VERSION` = `"0.1"`，占用的是 CMake 的
  major/minor 槽位），阶段词单独取出用于消息与 tag
- **命名分属两套约定**：C 宏是 `Lum_Version_Major`，CMake 变量是 `LUM_VERSION_MAJOR`
  （与 `LUM_C_COMPILER`、`LUM_ENGINE_SOURCES` 一致）。不要互相套用
- 实测：把 `Lum_Version_Patch` 临时改成 `2`，重配输出变为 `prototype 0.2`；
  还原后回到 `prototype 0.1` —— 证明是真解析，不是巧合

---

## 冻结时要补的

1. **编号规则**：何时动主要版本号、何时动 patch
2. **各阶段的兼容性承诺**：`prototype` / `alpha` / `beta` / `rc` / `release` 各自允许什么变化
3. **是否补回 minor**（决定后即为永久结构）
4. **存档格式版本**：把 `Lum_Scene_Io_Version` 对消费者公开，并写清兼容策略
   （当前 `1u` 使兼容门接受 0–99，见 `CONTRACT.md` §11）
5. **ABI 版本轴**：若将来出共享库，预留 `Lum_Abi_Version` 与 soname 方案
6. **固化 CMake 生成器**（`CMakePresets.json`），避免删 `build/` 后回落到 NMake

另见 [CONTRACT.md](CONTRACT.md) §7：`Lum_Debug` 会改变公开面，
**在这条解决之前，版本号无法可靠地标识 API 面**。

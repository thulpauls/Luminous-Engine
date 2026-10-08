# 基础层（Luminous 高层）

引擎高层的无副作用地基。**注意**：`log` / `assert` / `math2d` 已随第二次重组并入
**glimmer 基础平台**（`glimmer/lum_*.h`），本层现在只剩版本、碰撞与容器。

| 模块 | 头文件 | 职责 |
|---|---|---|
| `lum_version` | `lum_version.h` | **引擎**版本号（`<阶段> <迭代次数>`）**唯一来源** + 声明的兼容 glimmer 版本 |
| `lum_collision` | `lum_collision.h` | 纯几何碰撞、射线、包含判定（依赖 glimmer 的 `math2d`） |
| `lum_vector` | `containers/lum_vector.h` | 带 `elem_size` 的动态数组 |
| `lum_list` | `containers/lum_list.h` | 侵入式双向链表（**纯头**） |

**本层内部的依赖**：除 `lum_assert → lum_log` 外，各模块互不依赖。
`lum_math2d` / `lum_window` / `lum_time` 那三处名义依赖经实测为**未使用的 include**
（见 [architecture.md](../architecture.md) 第 4 节）。

---

## `lum_version`

**职责**：**引擎**版本号的唯一来源（格式 `<开发阶段> <迭代次数>`，当前 `prototype 1`），
并声明本引擎版本**针对哪个 glimmer 版本**开发。纯宏，无 `.c`。

glimmer 平台有自己独立的一条版本线，在 `glimmer/glimmer_version.h`（见 [glimmer.md](glimmer.md)）。

**契约**：编号规则、兼容性矩阵、CMake 如何取用，全部见 [versioning.md](../versioning.md)。

**易错点**

- `Lum_Version_Encode(stage, iteration)` **只用整数乘加、不做类型转换** ——
  这是刻意的：`#if` 的预处理器不认 `(unsigned long)` 这类多词类型转型
- 每段上限 **255**（8 位）：阶段在高位、迭代次数在低位
- **阶段名串（`Lum_Version_Stage_Name`）与阶段数值表必须同步改** ——
  数值用于比较、名串用于显示，两者没有语言层面的绑定关系
- `include/lum_version.h` 会包含 `glimmer/glimmer_version.h`，所以它**不再是叶子头**；
  相应地，引擎声明的 glimmer 版本与实际不一致时，**任何包含它的编译单元都会编译失败**
  （`#if` + `#error`），这是刻意的早期拦截

---

## `lum_log`

**职责**：分级日志（Trace / Debug / Info / Warn / Error / Fatal），全局单例。

**生命周期**：`lum_log_init` → … → `lum_log_shutdown`。
引擎的 Init 顺序里它**排第一**（后面任何一步失败都要能记录）。

**关键契约**

- `lum_log_init` 的默认值：`min_level = Debug`、`use_color = isatty(stderr)`、
  `output_stderr = true`、`file = NULL`
- `lum_log_set_file(FILE*)` 让日志**同时**写文件；文件分支**不带 ANSI 颜色**
- `Lum_Log_Info/Warn/Error/Fatal` 是宏，自动注入 `__FILE__` / `__LINE__` / `__func__`
- `lum_assert.h` 依赖 `lum_log_is_initialized`

**易错点**

- `Lum_Log_Trace` / `Lum_Log_Debug` 在**未定义 `Lum_Debug` 时是 no-op**
  （[CONTRACT.md](../CONTRACT.md) §7）
- 未初始化时所有调用**静默返回** —— 不缓存、不重放，早期日志会丢
- 日志**不是线程安全的**（[CONTRACT.md](../CONTRACT.md) §1）

---

## `lum_assert`

**职责**：`Lum_Assert(expr)` —— 只用于**编程错误**，失败即 `abort()`。纯头。

**关键契约**

- 定义 `Lum_Debug` 时：失败 → 记日志（日志未初始化则 `fprintf(stderr)`）→ `abort()`
- **未定义** `Lum_Debug` 时：整个断言变成 `((void)0)`

**易错点**

- **不要**用它做数据校验（用户输入、文件内容）—— 那类失败应返回 `0` / `false`
  （[CONTRACT.md](../CONTRACT.md) §2）
- release 下 `expr` **完全不被求值**，所以**绝不能**把有副作用的表达式放进去
  （例如 `Lum_Assert(++i < n)` 会静默改变行为）
- 它直接 `abort()`：**使用者无法接管**（没有自定义 handler 的钩子）

---

## `lum_math2d`

**职责**：2D 数学与矩阵运算。

**关键契约**

- 全部**按值**传递（`lum_Vec2` 等是小结构体）
- 角度**一律弧度**（[CONTRACT.md](../CONTRACT.md) §8）
- 浮点比较用全局常量 `Lum_Float_Abs_Epsilon`(1e-6f) / `Lum_Float_Rel_Epsilon`(1e-5f)，
  **调用方无法覆盖**
- 约定名字：`lum_vec2_0()` = 零向量、`lum_vec2_1()` = 全 1 向量、`lum_mat4_0()` = 零矩阵

**易错点**

- **11 个谓词返回 `int` 而不是 `bool`**（`lum_vec2_eq` / `lum_mat4_eq` /
  `lum_rect_contains` / `lum_float_is_zero` …）。这是全库唯一的返回值不一致处，
  见 [CONTRACT.md](../CONTRACT.md) §3
- 只有升维（`lum_vec2_to_vec3`、`lum_vec3_to_vec4`），**没有降维函数**
- 结构体字段是裸 `rotation`（无 `_rad` 后缀），但**同样是弧度**

---

## `lum_collision`

**职责**：纯几何碰撞检测 —— 无 GL、无 ECS 依赖，可独立单测。

**关键契约**（头文件已写明完整 9 条，这里是要点）

- 支持形状：点 / 圆 / 有向矩形(OBB) / **凸**多边形 / 线段
- **只支持凸多边形**。SAT 对非凸会给出**错误结果而非报错** ——
  入口自己调 `lum_polygon_is_convex` 校验（本层刻意不隐式校验，避免热路径开销）
- 返回 `1`/`0` 表示是否相交；接触信息写 `out_contact`，**允许传 NULL**
- **相切（穿透深度恰为 0）一律不算命中**；唯一例外是 `lum_polygon_contains_point`
  （边界算内部）
- `normal` 方向统一为 **A → B**；线段族例外：`depth` 恒为 `0`，
  `normal` 改为"指向线段一侧"
- **全程不分配内存**、不改动入参；顶点缓冲由调用方提供（栈数组即可）
- **返回 0 时所有输出参数行为未定义** —— 调用方必须先看返回值
- `lum_Polygon` 只是**顶点视图**（`const lum_Vec2*` + count），**不拥有内存**

**易错点**

- 忘了校验凸性 → 静默得到错误结果
- 先读 `out_contact` 再看返回值 → 未命中时读到未定义内容
- `lum_raycast_shape` **只支持 圆 / Box / 多边形**，不支持线段与点
- 顶点缓冲容量不足时返回 `0`（和"未命中"同一个返回值，语义要靠上下文区分）

---

## `containers/lum_vector`

**职责**：带 `elem_size` 的动态数组（`void*` 风格，无类型信息）。

**关键契约**

- 增长走 **`realloc`** —— 因此**所有返回的指针都会被后续 `push`/`emplace` 失效**。
  这是 [CONTRACT.md](../CONTRACT.md) §5（ECS 指针失效）的**根源**
- `lum_vector_remove(v, index)` → 用 `memmove` 左移，**保序**，O(n)
- `lum_vector_remove_swap(v, index)` → 把**末元素填入空洞**，**不保序**，O(1)
- `lum_vector_get_t` / `lum_vector_at_t` 是带类型的取值宏

**易错点**

- 两个函数都叫 "remove"，但**一个保序、一个不保序**，头文件里没写 ——
  选错会静默改变元素顺序（若该顺序有语义，就是难查的 bug）
- 指针失效（同上）
- `lum_vector_data()` 返回裸缓冲；`elem_size` 只有你自己知道

---

## `containers/lum_list`

**职责**：**侵入式**双向链表（节点内嵌在用户结构体里）。纯头，全部 `static inline`，**零分配**。

**关键契约**

- `lum_List_node` 必须作为**成员**嵌进用户类型
- 用 `lum_list_entry(node, T, member)` 从节点反查宿主指针（内部用 `offsetof`）
- 哨兵节点 `head`：`head.next` = 首元素，`head.prev` = 尾元素；空表时两者都指回 `head`
- 遍历用 `lum_list_each`；**迭代中要删除当前节点**就用 `lum_list_each_safe`
- `lum_list_pop_front` / `pop_back` 在空表时返回 `NULL`

**易错点**

- **命名例外**：本文件的宏是**小写** `lum_list_*`（对齐函数命名），
  与全库 `Lum_*` 宏约定不同 —— 不要以为写错了
- 同一节点**不能同时挂在两个链表**上
- `lum_list_remove` 只重新连接邻居，**不清空节点自身的 `prev`/`next`** ——
  重复 remove 会破坏链表
- 宏参数会被**求值多次**（`lum_list_each` 里 `(list)` 出现 2 次），
  所以别传带副作用的表达式

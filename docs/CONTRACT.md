# 语义契约

本文件收录**跨模块、无法从单个函数签名看出来**的约定。

- 逐符号参考在 `include/*.h` 的注释里，不在这里
- 分层与依赖见 [architecture.md](architecture.md)
- 每条都标注**【违反后果】**，否则读者无法判断严重性

> 标注 `⚠ 未成文` 的条目表示：**代码行为已经如此，但头文件里没有写**。
> 这些是冻结前要补进头文件的内容。

---

## 0. 术语表

中英混用会漂移，这里钉死。注意 **"层"有两个含义**。

| 术语 | 代码 | 含义 |
|---|---|---|
| 架构层 | 文档用词 | 基础 / 平台 / 渲染 / …（见 architecture.md） |
| **layer（渲染层）** | `int32_t layer` | **排序键**，越小越先绘制。与"架构层"同字不同义 |
| **category（分类）** | `uint32_t category` | 位掩码，供视图按位筛选可见性。**与 layer 正交** |
| stage（阶段） | `lum_Stage` | 状态机的阶段，顺序即引擎驱动顺序 |
| view（视图） | `lum_View` | 相机 + 视口矩形 + 渲染目标 + 分类掩码 |
| viewport（视口） | `int32_t viewport[4]` | 渲染目标**像素**、**左下原点** |
| scissor（剪裁） | — | 命令级剪裁矩形，同样是渲染目标像素、左下原点 |
| entity handle（句柄） | `lum_Entity` | 槽位下标 + 世代打包；`Lum_Entity_Null`(0) 无效 |
| component type | `lum_Component_type` | 注册得到的 id；`Lum_Component_Invalid` 无效 |
| local / world | `lum_Component_transform` | 两个变换空间，见 `lum_application.h` 的数据契约 |

---

## 1. 线程与单例模型

**结论：整个引擎是单线程、单实例的。**

- 全库**没有任何线程原语**（实测 `pthread` / `_Atomic` / `mtx_` / `Interlocked` / `volatile` 共 0 处）
- 下列模块都是**文件级全局单例**，不提供实例化：
  `lum_log` `lum_input` `lum_time` `lum_window` `lum_ui` `lum_assets` `lum_effects`
  `lum_renderer2d`、app 的系统注册表、`lum_default_theme`
- `lum_Context` 也是单例：**回调期间有效，不要保存指针**（`include/lum_application.h` 已写明）

**【违反后果】** 从任意第二个线程调用 → 数据竞争、未定义行为。**没有任何诊断**，
既不会加锁也不会报错。

**⚠ 未成文**：头文件里对线程模型**一字未提**。

---

## 2. 失败语义

两类失败，处理方式不同：

| 类别 | 例子 | 处理 |
|---|---|---|
| **编程错误** | 空指针、越界索引、非法枚举、重复 init | `Lum_Assert` → 记日志 → `abort()` |
| **数据相关失败** | 文件不存在、GL 资源创建失败、容量不足 | 返回 `0` / `false` / `NULL`，并 `Lum_Log_Error` |

**【违反后果】** `Lum_Debug` 未定义时 `Lum_Assert` 退化为 `((void)0)`，
编程错误从"立刻崩溃"变成**未定义行为**（见 §7）。

---

## 3. 返回值约定

**目前三套并存 —— 这是现存的不一致，冻结前应统一。**

| 风格 | 含义 | 例子 |
|---|---|---|
| `int`，1/0 | 1 = 成功 / 命中 | `lum_fs_write`、`lum_scene_load`、`lum_serial_r_f32`（1=命中并消费该行） |
| `bool` | 谓词 | `lum_ecs_alive`、`lum_collide_*`、`lum_ui_input_captured` |
| 哨兵值 | 特殊值表示失败 | `lum_shader_get_uniform_location` 返回 **-1**；`lum_shader_create_program_from_source` 返回 GL 句柄，**0** = 失败；`lum_ecs_component_type_from_name` 返回 `Lum_Component_Invalid` |

**例外（与全库其它谓词不一致）**：`lum_math2d.h` 的 11 个谓词返回 `int` 而非 `bool` ——
`lum_float_eq` / `lum_float_equal` / `lum_float_nearly_equal` / `lum_float_is_zero` /
`lum_vec2_eq` / `lum_vec3_eq` / `lum_vec4_eq` / `lum_mat4_eq` /
`lum_rect_contains` / `lum_rect_intersects` / `lum_rect_eq`。

---

## 4. 所有权与寿命

**引擎没有统一的命名约定**（没有 `_new` / `_borrow` 之类后缀），只能逐个记。
下表中"已写明"表示头文件注释里有。

| 函数 | 返回物 | 谁释放 | 何时失效 | 头文件 |
|---|---|---|---|---|
| `lum_ecs_create` | `lum_Ecs*` | 调用方 `lum_ecs_destroy` | — | ⚠ 未成文 |
| `lum_fs_read` | `char*`（**malloc**） | **调用方 `free`** | — | ⚠ 未成文 |
| `lum_fs_read_binary` | `data`（malloc） | **调用方 `free`** | — | ⚠ 未成文 |
| `lum_assets_texture` | `const lum_Texture2d*`（借用） | 不要释放 | `lum_assets_shutdown` 后 | ⚠ 未成文 |
| `lum_assets_font` | `lum_Font*`（借用） | 不要释放 | 同上 | ⚠ 未成文 |
| `lum_scene_name` | `const char*`（指向组件内缓冲） | 不要释放 | 实体销毁 / 池扩容后 | ⚠ 未成文 |
| `lum_ui_get_text`、`lum_ui_dropdown_option` | `const char*`（控件内深拷贝） | 不要释放 | 控件销毁后 | ⚠ 未成文 |
| `lum_material_variant_pool_acquire` | `lum_Material*`（池拥有） | 不要释放 | `..._pool_destroy` 后 | ⚠ 未成文 |
| `lum_window_get_native_handle` | `void*`（GLFWwindow*） | 不要释放 | 关窗后 | ⚠ 未成文 |
| `lum_app_get_ecs` / `lum_app_get_camera` | 引擎单例（借用） | 不要释放 | 引擎运行期间 |
| `lum_serial_writer_finish` | `const char*`（archive 拥有） | 不要释放 | `lum_serial_destroy` 前 | ✅ 已写明 |
| `lum_input_text` | `const char*`（同帧恒定） | 不要释放 | 下一帧 | ✅ 已写明 |

**【违反后果】** 该 free 的没 free → 泄漏；不该 free 的 free 了 → 堆破坏或崩溃。

---

## 5. ECS 指针失效规则 ⚠ 未成文（最重要的一条）

**事实**：组件池基于 `lum_Vector`，增长走 `realloc`
（`src/containers/lum_vector.c:31` 与 `:122`）。

**规则**：`lum_ecs_add` / `lum_ecs_get`（`include/lum_ecs.h:56-57`）
返回的是**指向池内 dense 数组的裸指针**。对**同一组件类型**再做一次 `add`，
可能使先前拿到的**所有**指针失效（realloc 搬家）。

同样适用：

- `lum_ecs_view_each` / `each2` / `each3` 的出参指针
- `lum_scene_add_transform` / `_sprite` / `_script` / `_name`
- `lum_component_glow_add` / `lum_component_beam_add`

**范围**：只有**同类型**的池之间互相影响；不同类型是独立池，互不干扰。

**推荐用法**

- 需要跨多个 `add` 保持引用 → 先收集 `lum_Entity` 句柄或组件类型 id，事后再 `get`
- 要遍历大量实体并同时创建组件 → 分两趟：先全部 `add`，再统一 `get`
- 帧内短命使用（取到就用完）是安全的

**【违反后果】** 悬垂指针 → 读到垃圾数据或崩溃。**没有任何诊断**。

---

## 6. 实体句柄与延迟回收

**模型**：`lum_Entity` = 槽位下标 + 世代 打包（`src/lum_ecs.c:8-11`；
20 位下标 + 12 位世代，上限 `Lum_Ecs_Max_Entities`）。

**规则**

1. `lum_ecs_alive` 要求「槽位 occupied && alive && 世代相符」三者同时成立
2. `lum_ecs_destroy_entity` **立即**把槽位置为 dead → `lum_ecs_alive` **马上**返回 false
3. 但槽位的**回收发生在 `lum_ecs_flush`**：那时才清理组件、并把世代 +1
   （`src/lum_ecs.c:196`，达到上限回绕到 1）
4. 因此销毁后、flush 前，句柄处于"已死但未回收"；flush 之后槽位可被新实体复用，
   **但世代已变，旧句柄不会误命中新实体**

**谁调 flush**

- 走引擎状态机时：`Lum_Stage_Commit` 阶段的内置 commit 系统**自动**调用
- 自己驱动 `lum_flow_run_steps` 或手写循环时：**自己调** `lum_ecs_flush`

**能否在遍历中销毁**：可以（每轮都查 `lum_ecs_alive`）。
但**不要在遍历中 `add` 同类型组件**（见 §5）。

**【违反后果】** 忘记 flush → 槽位与组件永不回收，`lum_ecs_entity_slot_count` 只增不减。

---

## 7. `Lum_Debug` 改变公开面

`add_compile_definitions(Lum_Debug)`（`CMakeLists.txt`）会改变**头文件内容**：

| 头文件 | `Lum_Debug` 定义 | 未定义 |
|---|---|---|
| `lum_assert.h` | `Lum_Assert` 真断言，`lum_assert_fail` 存在 | `Lum_Assert` = `((void)0)`，函数不存在 |
| `lum_log.h` | `Lum_Log_Trace/Debug` 是真实调用 | 两者都变 no-op |
| `lum_math2d.h` | `lum_vec2/3/4_print`、`lum_mat4_print` **始终有声明**（已修，见下） | 同左 |

**【违反后果】** **同一个版本号可以有不同 API 面**。用 `Lum_Debug` 打开的库
配未定义的头（或反之）→ 行为不一致、链接失败或断言静默消失。
这直接让版本号失去可信度。

**已修的部分**：`lum_math2d` 的四个打印函数不再随编译定义变化 —— 它们始终声明、
始终编译，输出走 `lum_log_message(Lum_Log_Level_Debug, ...)`，是否真的上屏/落盘
由日志模块的**运行时** `min_level` 决定（`lum_log_init` 默认 Debug，app 用
`desc.log_level` 调）。顺带修掉两件事：原来裸 `printf` 绕开日志模块（没有前缀、
不受级别控制、也进不了 `lum_log_set_file()` 的文件），以及自带 ANSI 反白转义
（重定向到文件就是乱码，`lum_log_enable_color(false)` 也关不掉）。
回归证据：同一份源码分别用「有 `-DLum_Debug`」与「没有」编译，输出逐行相同。

**冻结前仍需解决**（剩下两处，改成 PUBLIC 编译定义，或生成一个配置头）。

---

## 8. 坐标系与单位

| 空间 | 原点 / Y 方向 | 单位 |
|---|---|---|
| **世界空间**（相机） | Y **向上**（投影为 `ortho(-w/2, w/2, -h/2, h/2, …)`） | 世界单位 |
| **视口 / 剪裁** | **左下**原点 | 渲染目标**像素** |
| **屏幕空间**（`Lum_Stage_Screen`） | **左上**原点、Y **向下** | 像素 |
| **UI** | **左上**原点、Y **向下**（与鼠标输入同向） | 像素 |

注意：**视口是左下原点，屏幕空间是左上原点** —— 两者不同，混用会上下颠倒。

**实体的局部原点 = 绘制矩形的中心**

`transform.world` 的平移分量 `m[12..13]` 是**实体绘制矩形的中心**，不是角。
带 sprite 的实体，其 sprite 以该点为中心向四周展开 `size/2`。三处必须一致：

| 处 | 依据 |
|---|---|
| sprite 绘制 | `lum__sys_sprite` 传 `origin = size/2`（`src/lum_scene.c`） |
| 拾取 | `lum_scene_pick`：AABB = `world 平移 ± size × 层级缩放 / 2` |
| `lum_component_glow` | 渲染中心取 `m[12..13]`（跟随层级变换） |

- 严格说"中心"= **实体局部原点**经 `world` 映射后的点。当
  `transform.origin == (0,0)` 且无父级旋转时它等于 `position`；
  否则两者不等，而**上述三处都跟随局部原点**，所以仍然自洽
- 历史事故：`lum__sys_sprite` 曾传 `origin = (0,0)`（左下角锚点），
  而 pick / 框选按居中算 → 绘制与命中**差半个 `size`**。
  表现为"选择角标相对单位偏下偏左半个身位"、"点选要多点半个身位"。
  实测：单位 `size=24` 时二者中心相差 `(12,12)` 像素

**其它单位**

- **角度一律弧度**。参数名多带 `_rad`（`rotation_rad` / `delta_rad`）；
  但**结构体字段是裸 `rotation`**（`lum_Camera2d.rotation`、`lum_Component_transform.rotation`、
  `lum_Transform2d.rotation`、`lum_Box.rotation`），`lum_polygon_make_regular` 的
  `rotation` 参数也没带后缀 —— **不要按后缀判断，一律是弧度**
- 颜色：`lum_Vec4`，分量 0..1；**additive 混合下 `color.a` 不参与**
- 时间：秒（`float`）；`lum_time_get_total` 自引擎启动起

---

## 9. 枚举与常量的稳定性

**一旦冻结，下列只许追加、不许改序或改值**（顺序/数值本身就是语义）：

- `lum_Stage` —— 阶段的枚举顺序**就是**引擎的驱动顺序
- 内置组件 id：`lum_Component_transform_id`(0) … `lum_Component_name_id`(4)
- `lum_Blend_mode`、`lum_Primitive`、`lum_Texture_flag`
- `lum_Widget_type`、`lum_Ui_Align`
- `Lum_Sprite_Category_Default`（`1u << 0`）
- `lum_Keycode`、`lum_Mousebutton`（见 §10）

**硬上限**（改动即破坏性变更）：

| 常量 | 值 | 含义 |
|---|---|---|
| `Lum_Ecs_Max_Entities` | `1u << 20` | 实体槽位上限（句柄里只有 20 位下标） |
| `Lum_Serial_Max_Line` | 512 | 单行文本上限 |
| `Lum_Serial_Max_Args` | 16 | 单行参数个数上限 |
| `Lum_Serial_Max_Segment_Fields` | 32 | 单个组件段的字段数上限 |
| `Lum_Scene_Name_Max` | 64 | 实体名缓冲（**超长截断**） |
| `Lum_Material_Max_Params` | 16 | 材质参数槽 |
| `Lum_Material_Max_Name` | 31 | 参数名长度 |
| `Lum_Material_Max_Textures` | 4 | 材质纹理槽 |
| `Lum_Input_Text_Max` | 32 | 每帧字符流缓冲 |
| `Lum_Ui_Layer` | 1000 | UI 绘制层级 |

**另有结构性上限**：`lum_ecs_view_create3` 说明**视图最多同时匹配 3 个组件**
（`lum_Ecs_view.pools[3]`）。这是 API 限制，不是配置项。

---

## 10. 键码数值等于 GLFW

`Lum_Key_Escape = 256`、`Lum_Key_Enter = 257` … 与 GLFW 的 `GLFW_KEY_*` **逐条一致**；
`lum_Mousebutton` 的值也直接透传给 `glfwGetMouseButton`（`src/lum_input.c:91`）。

**【违反后果】** 换掉 GLFW 后端就会静默改变这些数值的含义。
**冻结前需表态**：是"有意跟随 GLFW"（写进契约），还是"待解耦"（加映射表）。

---

## 11. 序列化格式

- 文本、行导向、关键词作用域式；`#` 开头为注释行
- **结构行**：`version` / `entity` / `parent` / `component`
- **字段行**：`键 值...`；含空白的字符串加双引号（**无转义语法**）
- 读方向是单遍流式 + 一行预读：`r_*` 按 key 匹配当前预读行，不匹配则不消费、返回默认值

**版本与兼容**

- `Lum_Scene_Io_Version` 当前为 `1`，定义在 **`src/lum_scene_io.c:12`（消费者看不见）**
- 兼容门是 `version / 100u != Lum_Scene_Io_Version / 100u`，即**只比较 major**
- **实测行为**：`version 0` 接受、`version 99` 接受、`version 100` 拒绝 ——
  也就是当前常量 `1u` 使门"接受一切 0–99"

**持久化契约**

- 组件以**名字**落盘（`"glow"` / `"beam"` / `"transform"` …），不是 id
- 脚本按**注册名**恢复（函数指针不可落盘）；`user_data` **不参与序列化**，恢复后为 `NULL`

**【违反后果】** 改名 = 旧存档读不出该组件；字段表键名一变，旧字段静默取默认值。

---

## 12. 这些契约的可验证性

写了不执行会腐烂。能自动化的部分：

| 契约 | 可行的检查 |
|---|---|
| §9 枚举值 / 常量上限 | `_Static_assert`（编译期） |
| §8 结构体布局、`sizeof(lum_Vertex)` | `_Static_assert` |
| §3 返回值约定 | 统一后可由代码评审 + 测试覆盖 |
| §5 指针失效 | 测试：`add` 若干后对比旧指针（预期会变） |
| §6 世代语义 | 测试：销毁 → flush → 复用槽位，验证旧句柄失效 |
| §11 格式往返 | 测试：序列化 → 反序列化 → 比对 |
| §1 线程模型 | 无法自动查，只能靠文档与评审 |

`tests/` 目前为空，上述测试需要重建（见 `architecture.md` 第 8 节第 8 条）。

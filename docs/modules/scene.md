# 场景层

| 模块 | 头文件 | 职责 |
|---|---|---|
| `lum_ecs` | `lum_ecs.h` | 稀疏集 ECS（实体 / 组件 / 视图） |
| `lum_scene` | `lum_scene.h` | 内置组件 + 层级 + 视图 + 引擎系统注册 |
| `lum_scene_io` | `lum_scene_io.h` | 场景存档读写 |

三个模块的核心契约（**指针失效、世代回收**）在
[CONTRACT.md](../CONTRACT.md) §5 / §6，本页只列模块特有的部分。

---

## `lum_ecs`

**职责**：稀疏集（sparse-set）ECS，句柄带世代。

**关键契约**

- `lum_Ecs` 是**不透明类型**（头文件只有 `typedef struct lum_Ecs lum_Ecs;`）
- `lum_Entity` = 槽位下标(20 位) + 世代(12 位)，打包成 `uint32_t`；
  `Lum_Entity_Null`(0) 表示无效；上限 `Lum_Ecs_Max_Entities`(1<<20)
- 组件类型 id 由 `lum_ecs_register_component(ecs, name, size)` 注册获得：
  - **`size == 0` 会被拒绝**（记日志并返回 `Lum_Component_Invalid`）——
    所以"零尺寸标签组件"这个常见写法**不被支持**
  - 名字用于序列化（组件按名落盘）
- 数据访问：`lum_ecs_add` / `get` / `has` / `remove`
- 视图：`lum_ecs_view_create` / `create2` / `create3` +
  `lum_ecs_view_each` / `each2` / `each3`（返回 `Lum_Entity_Null` 表示结束）
  - **视图最多同时匹配 3 个组件** —— 这是结构性限制（`pools[3]`），不是配置项
  - `primary` 字段记录"元素最少的池"作主迭代，纯性能优化，调用方不用管
- 序列化元数据挂在 ECS 上：`lum_ecs_set_component_serializer`（自定义回调）与
  `lum_ecs_set_component_field_table`（字段表，内容由 serial 层解释）
- 槽位序遍历：`entity_slot_count` / `entity_at_slot` / `entity_slot_index`
  （序列化与调试用；空槽或死实体返回 `Lum_Entity_Null`）
- 销毁：`lum_ecs_destroy_entity` 立即置死；`lum_ecs_flush` 才真正回收并推进世代

**易错点**

- **指针失效**（[CONTRACT.md](../CONTRACT.md) §5）：`add`/`get`/`each` 返回的指针
  会被后续**同类型** `add` 失效
- 忘了 `lum_ecs_flush` → 槽位与组件永不回收。走引擎流程时
  `Lum_Stage_Commit` 会自动调；自己驱动时必须手动调
- 视图上限 3 个组件：需要 4 个条件时要拆成两次遍历
- 零尺寸组件注册失败

---

## `lum_scene`

**职责**：给 ECS 装上"引擎语义" —— 内置组件、层级树、视图，并把内置系统注册进状态机。

**内置组件 id**（固定，冻结后只许追加）

| id | 组件 | 说明 |
|---|---|---|
| 0 | `lum_Component_transform` | `position/scale/origin/rotation` + **`world`（派生，只读）** |
| 1 | `lum_Component_hierarchy` | 父/首子/兄弟链 + `child_count` |
| 2 | `lum_Component_sprite` | 纹理 / `size` / `uv` / `color` / `layer` / `category` / flip |
| 3 | `lum_Component_script` | `start` / `update` 回调 + `user_data` + `started` |
| 4 | `lum_Component_name` | 定长 64 字节实体名 |

**关键契约**

- `lum_scene_init(ecs)`：注册内置组件。**对同一 world 幂等**，
  但会**重置引擎系统注册**。world 创建后调一次
- `lum_scene_register_engine_systems()`：由 `lum_application` 在流程初始化时调用，
  **用户不应直接调用**（重复注册会让同一系统跑多次）
- **`layer` 与 `category` 正交**：`layer` 是排序键（越小越先画），
  `category` 是**位掩码**，供 `lum_View.category_mask` 按位筛选可见性。
  `Lum_Sprite_Category_Default` = `1u << 0`；视图掩码为 0 表示不筛选
- **`sprite.size` 是"自身绘制尺寸"**：不进层级矩阵、不受父级缩放影响，
  也**不影响子实体**。`uv` 默认 `(0,0,1,1)` 整图
- `transform.world` 是**派生数据**，每帧由 `Lum_Stage_Transform` 覆盖 —— **只读**

**视图 `lum_View`**

- 一个视图 = 相机 + 视口矩形 + 渲染目标 + 分类掩码 + 优先级 + 清屏
- `lum_view_default(name)` 填一套可用默认值
- `lum_view_add` **拷贝 desc，但 `name` 只存指针**（需要长命内存）
- `lum_view_get(ecs, index)` 返回可写指针（供每帧更新相机），
  但**索引是"绘制序"，后续 `add` 会让它漂移**
- `viewport` 单位是**渲染目标像素、左下原点**；`w` 或 `h <= 0` 表示"沿用当前视口"

**层级**

- `lum_scene_set_parent` 自动维护 `hierarchy` 组件与孩子链，并**沿祖先链查环**
- `parent` 传 `Lum_Entity_Null` 即摘除为根实体
- **级联销毁整棵子树**请用 `lum_scene_destroy_entity`，**不要**用 `lum_ecs_destroy_entity`

**脚本注册表**

- `lum_scene_register_script(name, start, update)` 建立 名字 → 回调对
- 序列化按**名字**恢复 script（函数指针不可落盘）
- **`user_data` 不参与序列化**，恢复后为 `NULL` —— 需要状态的脚本必须自行按实体重建

**拾取**

- `lum_scene_pick(ecs, world_point)`：返回 AABB 命中的最上层实体
  （sprite `layer` 最大，平局取后建者）；无命中返回 `Lum_Entity_Null`
- **只统计 transform + sprite 实体**（无 sprite 不可拾取）
- AABB = world 平移 ± `sprite.size × 层级缩放`（取矩阵列向量长度，**忽略旋转**）
- **依赖 `world` 矩阵**：必须先跑过包含 `Transform` 阶段的步进（管线后调用即安全）

**易错点**

- 混淆 `layer`（排序）与 `category`（可见性）
- `lum_scene_pick` 在未跑 `Transform` 时调用 → 用**上一帧**的 world 矩阵，命中区域偏移
- 手写 `lum_View` 时忘了初始化 `camera` → `zoom == 0` 算出退化投影
  （引擎有 `zoom <= 0 → 1.0` 兜底，但那是保险不是用法）
- `lum_view_add` 的 `name` 传了临时缓冲
- 用 `lum_ecs_destroy_entity` 销毁有子树的实体 → 留下悬空的层级链

---

## `lum_scene_io`

**职责**：场景存档（`lum_serial` 之上的场景语义层）。

**关键契约**

- `lum_scene_io_init(ecs)`：把内置组件与序列化绑定起来（`transform` / `sprite` /
  `hierarchy` / `script` / `name` 的字段表与自定义逻辑）
- `lum_scene_save(ecs, path)` / `lum_scene_load(ecs, path)` → **1/0**
- **load 会先清空当前 world**（`lum__scene_io_clear_world`），不是合并
- 文件头是 `version N`；缺失或版本不符会拒绝并记日志
- 组件以**名字**落盘；`parent` 引用**延迟到文件读完才统一接链**（单遍流式）
- 内部表示：`Lum_Scene_Io_Version` 定义在 `src/lum_scene_io.c:12`（**消费者看不到**）

**易错点**

- **load 清空现有 world** —— 想"叠加"必须自己做
- **版本门当前形同虚设**：兼容判断是 `version / 100 != 期望 / 100`，
  而常量是 `1u`，`1/100 == 0` → **实测接受 0–99 的任何版本号**
  （[CONTRACT.md](../CONTRACT.md) §11）
- `lum_scene_io.h` 只有 11 行、零注释 —— 格式、版本、兼容承诺全部不可见，
  这是冻结前要补的重点（见 [architecture.md](../architecture.md) 第 8 节第 4 条）

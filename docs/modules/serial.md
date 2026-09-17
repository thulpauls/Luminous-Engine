# 序列化层

| 模块 | 头文件 | 职责 |
|---|---|---|
| `lum_serial` | `lum_serial.h` | 与格式无关的读写抽象 + 声明式字段表 |

当前唯一的实现是**"关键词作用域式"文本格式**。`scene_io`（见 [scene.md](scene.md)）
是它的主要使用者；自定义组件也可以直接用。

---

## `lum_serial`

**职责**：把"组件 → 文本 / 文本 → 组件"这件事抽象成 Archive 接口，
组件序列化代码**不需要知道文件长什么样**。

**格式（当前实现）**

- 行导向，每行一条记录
- **结构行**：`version` / `entity` / `parent` / `component`
- **字段行**：`键 值...`，值以空白分隔；**含空白的字符串加双引号（无转义语法）**
- `#` 开头是注释行，空行忽略
- 读方向是**单遍流式 + 一行预读**：`r_*` 按 key 匹配**当前预读行**，
  不匹配就**不消费**并返回默认值；结构关键字行**永远不会**被字段读取消费

**写方向**

```
lum_serial_init_writer(&ar)
  lum_serial_w_linef(&ar, "entity %u", n)      // 结构行，格式化原样写
  lum_serial_w_f32 / w_i32 / w_u32 / w_bool / w_str / w_vec2 / w_vec4   // 字段行
  lum_serial_writer_finish(&ar, &size)          // 返回文本，archive 拥有，destroy 前有效
lum_serial_destroy(&ar)
```

**读方向**

```
lum_serial_init_reader(&ar, text)
  while (lum_serial_peek(&ar)) {
    key = lum_serial_peek_key(&ar);             // 预读行导航
    lum_serial_peek_argc / peek_argv(i) / peek_line
    lum_serial_consume(&ar);                    // 消费掉这一行
  }
```

- **`r_*` 的返回值语义**：**1 = 命中并消费该行；0 = 缺失**
  （`*out` 保持调用方传入的默认值，且**不消费任何行**）
- `lum_serial_begin_component(&ar)`：进入新的组件段时清空字段缓冲

**声明式字段表（普通组件不必写回调）**

```c
static const lum_Component_field_desc fields[] = {
  Lum_Component_Make_Field_Vec2(lum_My_component, position, "position", 0, 0),
  Lum_Component_Make_Field_F32 (lum_My_component, radius,   "radius",   1.0f),
};
lum_Component_type t = lum_serial_register_component(ecs, "my", sizeof(lum_My_component),
                                                     fields, 2);
```

- 宏自动取 `offsetof`，`Str` 字段的 `size` 取成员数组容量
- 组件级一站式：`lum_serial_component_write` / `lum_serial_component_read`
  —— **字段表优先，其次自定义回调**；两者都没有则返回 `0`（调用方可降级直转储）

**上限**

| 常量 | 值 | 含义 |
|---|---|---|
| `Lum_Serial_Max_Line` | 512 | 单行文本上限 |
| `Lum_Serial_Max_Args` | 16 | 单行参数个数上限 |
| `Lum_Serial_Max_Segment_Fields` | 32 | 单个组件段的字段数上限 |

**易错点**

- `r_*` **不消费不匹配的行** → 字段顺序无所谓，但也意味着**拼错 key 会静默取默认值**，
  不会有任何报错
- 字段数超过 `Lum_Serial_Max_Segment_Fields` → **告警并丢弃**（不是报错）
- 字符串**没有转义语法**：值里带双引号会破坏解析
- `lum_Archive` 是**公开结构体**（`buffer` / `segment` / `pending` / `has_pending` …），
  它的布局属于 ABI —— 冻结时要么接受，要么把它改成不透明类型
- `lum_serial_writer_finish` 返回的文本由 archive 拥有，**`lum_serial_destroy` 后失效**

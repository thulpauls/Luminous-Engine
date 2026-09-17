# 内置扩展组件

| 模块 | 头文件 | 职责 |
|---|---|---|
| `lum_components` | `components/lum_components.h` | 聚合入口：一次注册全部扩展组件 |
| `lum_component_glow` | `components/lum_component_glow.h` | 光晕 / 光环**实体组件** + 渲染系统 |
| `lum_component_beam` | `components/lum_component_beam.h` | 线状光束**实体组件** + 渲染系统 |

**与 [effects](effects.md) 的区别**：`effects` 是**即时绘制函数**（给坐标就画），
`components` 是**挂在实体上的 ECS 数据**（走场景管线自动绘制）。
两者共用同一套着色器参数语义。

---

## `lum_components`

**职责**：一次调用注册全部扩展组件（组件注册 + 字段表序列化绑定）。

```c
int lum_components_register(lum_Ecs* ecs);   // 1 成功 / 0 失败
```

- 内部依次注册 `glow` 与 `beam`；任一失败即返回 `0`（已成功的那些不会回滚）
- **渲染系统不会自动挂** —— 必须自己注册：

```c
lum_components_register(ecs);
lum_app_add_system(Lum_Stage_Scene, 0, lum_component_glow_render_system, NULL);
lum_app_add_system(Lum_Stage_Scene, 0, lum_component_beam_render_system, NULL);
```

---

## `lum_component_glow`

**职责**：实体上的光晕 / 光环。ECS 数据 + 渲染系统 + 字段表序列化"三件套"。

**关键契约**

- 组件名（落盘用）：**`"glow"`**
- `lum_component_glow_register(ecs)` → 组件 id（**对同一 world 幂等**）
- `lum_component_glow_type()` → 当前 world 的组件 id；**未注册过返回 `Lum_Component_Invalid`**
- `lum_component_glow_add(ecs, e)` → 填默认值；**未注册时返回 `NULL`**
- `lum_component_glow_render_system` 挂到 `Lum_Stage_Scene`；**渲染中心 = world 平移**，
  使用 additive 混合

**默认值**

| 字段 | 默认 | 含义 |
|---|---|---|
| `ring` | `false` | `false` = 高斯光晕，`true` = 中空光环 |
| `radius` | 60.0 | 可见半径 |
| `layer` | 0 | 排序键 |
| `color` | 白 | 光色 |
| `core_size` / `core_decay` / `edge_decay` | 0.18 / 1.0 / 4.0 | 光晕参数 |
| `halo_intensity` | 0.35 | 外围亮度乘数 |
| `ring_radius` / `ring_width` / `ring_intensity` | 0.6 / 0.12 / 1.0 | 光环参数 |
| `ring_inner_decay` / `ring_outer_decay` | 1.0 / 1.0 | 环内外衰减 |

---

## `lum_component_beam`

**职责**：实体上的线状光束。

**关键契约**

- 组件名（落盘用）：**`"beam"`**
- 接口与 glow 同构：`_register` / `_type` / `_add` / `_render_system`
- **端点偏移是本地坐标**（相对 `transform.position`），
  渲染时**经 world 矩阵变换** → 会跟随父级层级与旋转
- 使用 additive 混合

**默认值**

| 字段 | 默认 | 含义 |
|---|---|---|
| `start_offset` / `end_offset` | `(0,0)` / `(0,0)` | 相对 transform 的本地偏移 |
| `thickness` | 10.0 | 线宽 |
| `mode` | `Lum_Beam_Mode_Lateral` | 衰减模式（`int32_t`，避免组件头依赖 effects） |
| `layer` | 0 | 排序键 |
| `color` | 白 | 光色 |
| `core_size` / `core_decay` / `edge_decay` | 0.25 / 1.0 / 4.0 | 衰减参数 |

---

## 易错点（两个组件共通）

- **忘记注册** → `_add` 返回 `NULL`（因为 `_type()` 是 `Lum_Component_Invalid`），
  而且**不会有任何断言提示**，只表现为"组件加不上"
- **忘记挂渲染系统** → 组件数据存在、能序列化，但**屏幕上什么都没有**
- 两个组件的 `_add` 返回的指针同样受**指针失效**约束
  （[CONTRACT.md](../CONTRACT.md) §5）
- `layer` 与 `lum_Component_sprite.layer` 在同一个排序空间里竞争
- 组件名（`"glow"` / `"beam"`）是**持久化契约** —— 改名会让旧存档读不出该组件

# 特效

| 模块 | 头文件 | 职责 |
|---|---|---|
| `lum_effects` | `effects/lum_effects.h` | 门面：统一 init / shutdown |
| `lum_glow` | `effects/lum_glow.h` | 高斯光晕与环形光环的**即时绘制** |
| `lum_beam` | `effects/lum_beam.h` | 线状光效的**即时绘制** |

这三个模块提供**给坐标就画**的函数，不经过 ECS；对应的实体组件在
[components.md](components.md)。

---

## `lum_effects`

**职责**：把 glow 与 beam 的初始化收成一个门面。引擎 Init 第 6 级调的就是它。

**关键契约**

- `lum_effects_init()`：内部**先 `lum_glow_init()`，再 `lum_beam_init()`**；
  后者失败会**回滚前者**（`lum_glow_shutdown()`）
- `lum_effects_shutdown()`：**逆序**拆（先 beam 后 glow）
- `lum_effects_is_initialized()`

**易错点**

- **不要**同时用门面和各自的 init 混着调 —— 重复 init 会命中
  `Lum_Assert(!g_*.initialized && "double init")`（debug 下 abort）
- 光晕/光环/光束的绘制函数**在未初始化时静默返回**，不会报错

---

## `lum_glow`

**职责**：两种光效的即时绘制 —— 高斯衰减光晕，以及中空环形光环。

**关键契约**

| 函数 | 说明 |
|---|---|
| `lum_glow_draw(center, radius, layer, color)` | 用默认参数画高斯光晕 |
| `lum_glow_draw_advanced(center, radius, layer, color, core_size, core_decay, edge_decay, halo_intensity)` | 可调核心大小 / 核心衰减 / 边缘衰减 / 外围亮度 |
| `lum_glow_draw_ring(center, radius, layer, color)` | 用默认参数画光环 |
| `lum_glow_draw_ring_advanced(center, radius, layer, color, ring_radius, ring_width, ring_intensity, ring_inner_decay, ring_outer_decay)` | 可调环半径 / 环厚度 / 亮度 / 内外侧衰减 |

- `color.rgb` 是光色；**additive 混合下 `color.a` 不参与混合**
- 参数取值：`core_size` ∈ `(0, 1]`（亮核半径比例）、`ring_radius` ∈ `(0, 1]`
- **负半径会直接返回**（不绘制、不报错）
- 着色器里做了**过曝处理**：亮度 `> 1` 时向白色混合（`mix(col, vec3(lum), over)`）
- 默认值宏：`Lum_Glow_Default_*`（`Core_Size` 0.18 / `Edge_Decay` 4.0 /
  `Halo_Intensity` 0.35 / `Ring_Radius` 0.6 / `Ring_Width` 0.12 …）

**实现要点**：每个绘制调用会**按参数向材质变体池取一个材质**
（`lum_material_variant_pool_acquire`），池是固定容量 + **轮转复用**的
（见 [glimmer.md](glimmer.md) 的 `lum_material`）。

**易错点**

- 未 init 就 draw → **静默无输出**
- 每帧传不同的 `core_size` 等参数 → 会不断触发变体池的**轮转重建**，
  材质缓存失效（参数种类应尽量收敛）
- `layer` 与场景内 sprite 的 `layer` 在同一排序空间

---

## `lum_beam`

**职责**：线状光效的即时绘制。

**关键契约**

| 函数 | 说明 |
|---|---|
| `lum_beam_draw(start, end, thickness, mode, layer, color)` | 默认衰减参数 |
| `lum_beam_draw_advanced(start, end, thickness, mode, layer, color, core_size, core_decay, edge_decay)` | 可调核心宽度 / 强度 / 边缘衰减 |

**三种衰减模式**（`lum_Beam_mode`）

| 模式 | 衰减方向 |
|---|---|
| `Lum_Beam_Mode_Lateral`(0) | **横向**：从中心线向两侧（宽度方向）衰减 |
| `Lum_Beam_Mode_Longitudinal`(1) | **纵向**：从中心点向两端（长度方向）衰减 |
| `Lum_Beam_Mode_Center`(2) | **中心**：同时向宽度与长度方向衰减 |

- `color.rgb` 是光色；additive 下 `color.a` 不参与
- 内部只用一个着色器，靠 `u_mode` uniform 切换 `d` 的计算方式
- 默认值宏：`Lum_Beam_Default_Core_Size` 0.25 / `Core_Decay` 1.0 / `Edge_Decay` 4.0

**易错点**

- `mode` 是枚举（`lum_Beam_mode`），但实体组件里存的是 `int32_t`
  （`lum_Component_beam.mode`，避免组件头依赖本模块）—— 跨两者赋值时注意别写错值
- 同 glow：未 init 静默无输出；参数变化会驱动变体池轮转

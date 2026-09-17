# 资源层

| 模块 | 头文件 | 职责 |
|---|---|---|
| `lum_assets` | `lum_assets.h` | 磁盘资源的**进程级缓存**（纹理 + 字体），单例 |

`lum_assets` 是引擎里唯一的资源缓存。它不自己加载像素 —— 纹理交给 `lum_texture`
（内部走 stb_image），字体交给 `lum_text_renderer`（内部走 stb_truetype），
它只负责**按请求参数缓存并复用**。

---

## `lum_assets`

**职责**：按 `(路径, 参数)` 缓存已加载的资源，避免重复解码与重复上传。

**生命周期**

- `lum_assets_init()` / `lum_assets_shutdown()`
- 在引擎 Init 顺序里排**第 8 级（最后）** —— 它持有的是 GL 对象（纹理、图集），
  必须在渲染器与窗口就绪之后才能建；反过来关闭时要**最先拆**
  （见 [architecture.md](../architecture.md) 第 5 节）

**关键契约**

| 函数 | 说明 |
|---|---|
| `lum_assets_texture(path, flags)` | 命中就返回缓存；未命中则加载并存入。返回 `const lum_Texture2d*`（**借用**） |
| `lum_assets_font(path, height_pixel)` | 同上，缓存键是 `(路径, 字号)`。返回 `lum_Font*`（**借用**） |
| `lum_assets_texture_path(texture)` | **反查**加载路径（序列化引用用）。非本仓库来源的纹理返回 `NULL` |
| `lum_assets_count()` | 当前缓存条目数 |

- **缓存键包含参数**：同一个路径用不同 `flags`（纹理）或不同 `height_pixel`（字体）
  会各自成为**独立条目**，即加载两份
- **失败会被负缓存**：加载失败的条目记为 `failed`，之后**同一请求直接返回 `NULL`，
  不会重试**（见 `src/lum_assets.c` 中 `entry->failed` 的用法）
- 返回的指针由 registry 拥有，**`lum_assets_shutdown` 之后全部失效**
  （[CONTRACT.md](../CONTRACT.md) §4）

**易错点**

- **负缓存**：修好了磁盘上的文件，但进程不重启就不会重新加载 —— 迭代资源时容易困惑
- 同一路径不同 `flags` → 静默加载两份，显存翻倍
- **不要 `free` / `destroy`** 返回的指针，它们是借用的
- `lum_assets_texture_path` 只认本 registry 加载过的纹理；外部自建的
  `lum_Texture2d` 一律返回 `NULL`（序列化时表现为"该纹理不落盘"）
